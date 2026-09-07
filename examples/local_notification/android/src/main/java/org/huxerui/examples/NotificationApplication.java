package org.huxerui.examples;

import android.annotation.SuppressLint;
import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Build;
import android.widget.RemoteViews;

import org.huxerui.HuxerUILocalNotificationContent;
import org.huxerui.HuxerUILocalNotificationLayout;
import org.huxerui.HuxerUILocalNotificationLayoutProvider;
import org.huxerui.PlatformPayload;

public final class NotificationApplication extends Application implements HuxerUILocalNotificationLayoutProvider {
    private static final String NOTIFICATION_CHANNEL_ID = "huxerui.local-notification.downloads";
    private static final String DOWNLOAD_TEMPLATE_ID = "huxerui.local-notification.download";

    @Override
    public void onCreate() {
        super.onCreate();
        if (Build.VERSION.SDK_INT >= 26) {
            Api26.createNotificationChannel(this);
        }
    }

    @Override
    public boolean supportsLocalNotificationTemplate(String identifier) {
        return DOWNLOAD_TEMPLATE_ID.equals(identifier);
    }

    @Override
    public HuxerUILocalNotificationLayout createLocalNotificationLayout(HuxerUILocalNotificationContent content) {
        if (!supportsLocalNotificationTemplate(content.getTemplateIdentifier())) {
            return null;
        }
        PlatformPayload data = content.getData();
        String fileName = data.requireField("file_name").requireString();
        String status = data.requireField("status").requireString();
        long downloaded = data.requireField("downloaded_bytes").requireInt64();
        PlatformPayload totalValue = data.requireField("total_bytes");
        long total = totalValue.isNull() ? 0 : totalValue.requireInt64();
        if (downloaded < 0 || total < 0) {
            throw new IllegalArgumentException("HuxerUI download template byte counts must not be negative");
        }
        boolean indeterminate = total == 0 && "Downloading".equals(status);
        int progress = total > 0 ? (int) Math.min(100, downloaded * 100.0 / total) : 0;
        if ("Complete".equals(status)) {
            progress = 100;
        }
        RemoteViews compact = new RemoteViews(getPackageName(), R.layout.notification_download);
        compact.setTextViewText(R.id.notification_title, fileName);
        compact.setTextViewText(R.id.notification_body, content.getBody());
        compact.setProgressBar(R.id.notification_progress, 100, progress, indeterminate);
        if (!data.requireField("expanded").requireBoolean()) {
            return new HuxerUILocalNotificationLayout(compact);
        }
        RemoteViews expanded = new RemoteViews(getPackageName(), R.layout.notification_download_expanded);
        expanded.setTextViewText(R.id.notification_title, fileName);
        expanded.setTextViewText(R.id.notification_body, content.getBody());
        expanded.setTextViewText(R.id.notification_detail, "Tap to inspect the submitted file name and progress data.");
        expanded.setProgressBar(R.id.notification_progress, 100, progress, indeterminate);
        return new HuxerUILocalNotificationLayout(compact, expanded, null);
    }

    @SuppressLint("NewApi")
    private static final class Api26 {
        private Api26() {}

        static void createNotificationChannel(Context context) {
            NotificationManager manager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
            if (manager == null) {
                return;
            }
            // Frequent progress replacements should not produce sound or heads-up interruptions.
            NotificationChannel channel = new NotificationChannel(NOTIFICATION_CHANNEL_ID, "Notification demo",
                                                                   NotificationManager.IMPORTANCE_LOW);
            channel.setDescription("Default reminders and download progress from the HuxerUI notification example");
            manager.createNotificationChannel(channel);
        }
    }
}
