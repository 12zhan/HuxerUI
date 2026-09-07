package org.huxerui;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** Delivers application-declared, one-shot HuxerUI local-notification alarms. */
public final class HuxerUILocalNotificationReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        HuxerUILocalNotification.deliverScheduled(context, intent);
    }
}
