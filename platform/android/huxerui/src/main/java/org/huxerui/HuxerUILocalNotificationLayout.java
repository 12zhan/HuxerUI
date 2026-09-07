package org.huxerui;

import android.widget.RemoteViews;

/** Holds the fresh RemoteViews used for one decorated Android notification. At least one view must be non-null. */
public final class HuxerUILocalNotificationLayout {
    private final RemoteViews contentView;
    private final RemoteViews expandedContentView;
    private final RemoteViews headsUpContentView;

    /** Supplies only a normal custom layout; expanded and heads-up custom layouts remain absent. */
    public HuxerUILocalNotificationLayout(RemoteViews contentView) {
        this(contentView, null, null);
    }

    public HuxerUILocalNotificationLayout(RemoteViews contentView, RemoteViews expandedContentView,
                                         RemoteViews headsUpContentView) {
        this.contentView = contentView;
        this.expandedContentView = expandedContentView;
        this.headsUpContentView = headsUpContentView;
    }

    public RemoteViews getContentView() {
        return contentView;
    }

    public RemoteViews getExpandedContentView() {
        return expandedContentView;
    }

    public RemoteViews getHeadsUpContentView() {
        return headsUpContentView;
    }

    boolean hasViews() {
        return contentView != null || expandedContentView != null || headsUpContentView != null;
    }
}
