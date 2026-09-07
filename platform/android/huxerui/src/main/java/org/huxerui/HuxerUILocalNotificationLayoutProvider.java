package org.huxerui;

/**
 * Supplies application-owned RemoteViews for declared local-notification templates.
 *
 * <p>Implement this interface on the application's {@code Application} class. HuxerUI invokes it on the Android main
 * thread for immediate delivery and from its alarm receiver after Android recreates the application for scheduled
 * delivery.
 */
public interface HuxerUILocalNotificationLayoutProvider {
    /** Returns whether this application owns the requested stable template identifier. */
    boolean supportsLocalNotificationTemplate(String identifier);

    /** Creates fresh views for one supported notification, or returns null when construction fails. */
    HuxerUILocalNotificationLayout createLocalNotificationLayout(HuxerUILocalNotificationContent content);
}
