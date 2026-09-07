# Local Notifications

Use this reference for local operating-system notifications, native template setup, scheduling, cancellation, and notification activation data. These are not in-application Toasts or remote push notifications.

## Submission and authorization

`UseApplication().LocalNotifications()` returns the current Runtime's local-notification handle. Read `Capabilities()` when UI needs the current transport snapshot. Await `CheckAuthorizationAsync()`, `RequestAuthorizationAsync()`, `ShowAsync()`, `ScheduleAsync()`, and `CancelAsync()` from a `TaskScope`; authorization queries run independently, while authorization requests and mutations preserve submission order.

The permission types and notification API are declared in `<huxerui/system.h>`. Both authorization operations return `PermissionStatus`: `Granted` and `Provisional` allow notification submission, with provisional access retaining native presentation limits such as quiet delivery. Native ephemeral access maps to `Granted` while effective; do not treat a grant as an indefinite authorization guarantee or add notifications to the `Permission` enum.

Use non-empty stable UTF-8 identifiers for replacement, cancellation, and activation routing. `ShowAsync()` replaces both a pending schedule and delivered presentation with the same identifier; `ScheduleAsync()` replaces only the pending schedule. At least one of `LocalNotification::title` or `body` must resolve to non-empty text. Scheduling is one-shot, best effort, and requires a future absolute `std::chrono::system_clock::time_point`; `Accepted` does not guarantee user-visible delivery. Native declarations, identity, channels, and final product policy remain application-shell responsibilities.

`LocalNotification::presentation` defaults to `DefaultNotificationPresentation`. Use `TemplateNotificationPresentation{.identifier = "application.reminder"}` only when the application shell owns the same stable identifier. `capabilities.can_use_templates` reports a host template mechanism, while an unknown identifier returns `Unavailable` without falling back to system presentation.

## Data and activation

`LocalNotification::data` carries a `PlatformPayload` snapshot to native layouts and back through `NotificationActivation::data`, including cold-start activation and scheduled delivery. It defaults to Null. Only resource-free values are allowed, with a 64 KiB encoded HUXP limit; never pass `ExternalTexture` or `FileReference`, including inside collections. Keep sensitive or large data in application storage, and validate returned fields and current authorization independently.

Read `UseApplication().StartupActivation()` and register `OnActivation(...)` during composition. Inspect `NotificationActivation` with `std::get_if` or `std::visit`, validate the identifier and expected payload fields, and update application-owned state or navigation. Android interaction may become startup or subsequent activation according to Activity creation. iOS and macOS interactions enter `OnActivation()` even when they launched the process because their native delegate responses arrive after host launch.

## Android shell and templates

Configured Android hosts use `NotificationManager` for presentation, an application-declared receiver with an inexact alarm for durable one-shot scheduling, and Activity Intents for activation. The Android shell creates its channel before `HuxerUIView`, supplies `org.huxerui.local_notification.channel_id` and `org.huxerui.local_notification.small_icon` application metadata, declares `POST_NOTIFICATIONS` for Android 13 or later, and explicitly declares `org.huxerui.HuxerUILocalNotificationReceiver` as non-exported when scheduling is required. Scheduling does not promise exact alarms or persistence across device restart.

Template presentation requires API 24 or later and an `Application` implementing `HuxerUILocalNotificationLayoutProvider`. Declare supported identifiers and return fresh `RemoteViews` from `createLocalNotificationLayout`; HuxerUI retains the notification builder, channel, icon, identity, activation, and scheduling.

Read application parameters through `HuxerUILocalNotificationContent.getData()`. Use `new HuxerUILocalNotificationLayout(contentView)` when no custom expanded or heads-up layout is needed; the three-argument constructor accepts independently optional compact, expanded, and heads-up layouts, with at least one supplied. Omitting an expanded layout does not disable system-controlled expansion. Select layouts from the application's requirements rather than always providing all three.

## Apple shells and templates

iOS and macOS expose authorization, immediate presentation, durable one-shot scheduling, cancellation, and primary-action activation through User Notifications.

iOS templates select categories declared by embedded Notification Content Extensions through `UNNotificationExtensionCategory`. The extension owns the expanded UI; the ordinary banner and notification-list layout remain system-controlled. In the extension, include `<huxerui/ios/platform_registry.h>` and call `HUXGetLocalNotificationData(content)` (Swift: `localNotificationData(content)`) to obtain `HUXPlatformPayload`. An absent value produces a Null payload; nil signals invalid data. Use the public helper without decoding private `userInfo` keys or creating a Runtime.

macOS supports default presentation and data round-trip but not template layouts. Do not assume iOS Content Extensions are available on macOS.

## Updating application-owned progress

A native template may render progress supplied through application-defined `data` fields. Reuse the notification identifier for replacements, throttle updates, and await or coalesce submissions instead of launching a Task for every byte-progress callback. Publish final state after output finalization, and keep notification failure distinct from download failure. A progress layout does not imply a background download service, resumable transfer, or survival of the originating Runtime; the last notification can outlive the work it described. Keep file names and progress snapshots separate from file-access capabilities, and validate activation fields before using them.

## Unsupported capabilities

Windows, Linux, and Web currently report unavailable capabilities and operations. Unknown template identifiers return `Unavailable` without falling back to default presentation. Do not emulate unsupported delivery with Toast, System Tray, or Runtime timers.
