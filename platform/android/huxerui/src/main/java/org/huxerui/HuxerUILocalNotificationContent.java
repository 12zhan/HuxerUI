package org.huxerui;

/** Immutable content supplied to an Android local-notification layout provider. */
public final class HuxerUILocalNotificationContent {
    private final String identifier;
    private final String title;
    private final String body;
    private final String templateIdentifier;
    private final PlatformPayload data;

    HuxerUILocalNotificationContent(String identifier, String title, String body, String templateIdentifier,
                                   PlatformPayload data) {
        this.identifier = identifier;
        this.title = title;
        this.body = body;
        this.templateIdentifier = templateIdentifier;
        this.data = data;
    }

    public String getIdentifier() {
        return identifier;
    }

    public String getTitle() {
        return title;
    }

    public String getBody() {
        return body;
    }

    public String getTemplateIdentifier() {
        return templateIdentifier;
    }

    /** Returns the immutable application data snapshot submitted with this notification. */
    public PlatformPayload getData() {
        return data;
    }
}
