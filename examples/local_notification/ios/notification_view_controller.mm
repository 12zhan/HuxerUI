#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>
#import <UserNotificationsUI/UserNotificationsUI.h>

#import <huxerui/ios/platform_registry.h>

@interface HUXDownloadNotificationViewController : UIViewController <UNNotificationContentExtension>
@property(nonatomic, strong) UILabel* file_label;
@property(nonatomic, strong) UILabel* status_label;
@property(nonatomic, strong) UILabel* detail_label;
@property(nonatomic, strong) UIProgressView* progress_view;
@property(nonatomic, strong) UIActivityIndicatorView* activity_indicator;
@end

@implementation HUXDownloadNotificationViewController

- (void)loadView {
  self.view = [[UIView alloc] init];
  self.view.backgroundColor = UIColor.systemBackgroundColor;
  self.file_label = [[UILabel alloc] init];
  self.file_label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  self.file_label.numberOfLines = 2;
  self.file_label.lineBreakMode = NSLineBreakByTruncatingMiddle;
  self.status_label = [[UILabel alloc] init];
  self.status_label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  self.status_label.numberOfLines = 0;
  self.detail_label = [[UILabel alloc] init];
  self.detail_label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  self.detail_label.textColor = UIColor.secondaryLabelColor;
  self.detail_label.numberOfLines = 0;
  self.progress_view = [[UIProgressView alloc] initWithProgressViewStyle:UIProgressViewStyleDefault];
  self.activity_indicator =
      [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  self.activity_indicator.hidesWhenStopped = YES;

  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:@[
      self.file_label, self.status_label, self.progress_view, self.activity_indicator, self.detail_label,
  ]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = 10;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
      [stack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:16],
      [stack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-16],
      [stack.topAnchor constraintEqualToAnchor:self.view.topAnchor constant:16],
      [stack.bottomAnchor constraintLessThanOrEqualToAnchor:self.view.bottomAnchor constant:-16],
  ]];
  self.preferredContentSize = CGSizeMake(320, 180);
}

- (void)didReceiveNotification:(UNNotification*)notification {
  [self loadViewIfNeeded];
  [self.activity_indicator stopAnimating];
  self.activity_indicator.hidden = YES;
  self.progress_view.hidden = YES;
  self.detail_label.hidden = YES;
  self.file_label.text = @"HuxerUI SDK download";
  self.status_label.text = @"Missing or invalid download data";

  // The extension receives persisted snapshots, not shared app state or permission to access the downloaded file.
  HUXPlatformPayload* data = HUXGetLocalNotificationData(notification.request.content);
  if (data == nil || data.kind != HUXPlatformPayloadKindObject) {
    return;
  }
  @try {
    NSString* file_name = [[data field:@"file_name"] stringValue];
    NSString* status = [[data field:@"status"] stringValue];
    int64_t downloaded = [[data field:@"downloaded_bytes"] integerValue];
    HUXPlatformPayload* total_value = [data field:@"total_bytes"];
    int64_t total = total_value.kind == HUXPlatformPayloadKindNull ? 0 : total_value.integerValue;
    BOOL expanded = [[data field:@"expanded"] booleanValue];
    if (downloaded < 0 || total < 0) {
      return;
    }
    self.file_label.text = file_name;
    self.status_label.text = status;
    self.detail_label.text = total > 0
        ? [NSString stringWithFormat:@"%lld / %lld KiB received",
                    (long long)(downloaded / 1024), (long long)(total / 1024)]
        : [NSString stringWithFormat:@"%lld KiB received; total unknown", (long long)(downloaded / 1024)];
    self.detail_label.hidden = !expanded;
    BOOL indeterminate = total == 0 && [status isEqualToString:@"Downloading"];
    self.progress_view.hidden = indeterminate;
    self.activity_indicator.hidden = !indeterminate;
    if (indeterminate) {
      [self.activity_indicator startAnimating];
    } else {
      float progress = total > 0 ? (float)MIN(1.0, (double)downloaded / (double)total) : 0.0F;
      self.progress_view.progress = [status isEqualToString:@"Complete"] ? 1.0F : progress;
    }
    self.preferredContentSize = CGSizeMake(320, expanded ? 180 : 140);
  } @catch (NSException*) {
    // Public payload accessors reject missing fields and mismatched kinds rather than fabricating progress.
  }
}

@end
