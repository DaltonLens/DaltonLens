//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <Cocoa/Cocoa.h>
#import <ServiceManagement/ServiceManagement.h>

#import "DaltonGUI/DaltonLensGUI_macOS.h"

// MARK: - DaltonWindow

@interface DaltonWindow : NSWindow
@end

@implementation DaltonWindow
- (BOOL)canBecomeKey {
  return YES;
}
@end

// MARK: - AppDelegate

@interface AppDelegate : NSObject <NSApplicationDelegate>

@property(nonatomic, strong) DaltonLensGUIMacOS *daltonGUI;
@property(nonatomic, strong) NSStatusItem *statusItem;
@property(nonatomic, strong) NSMenuItem *launchAtStartupMenuItem;
@property(nonatomic, strong) id spaceChangeObserver;

@end

@implementation AppDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    _daltonGUI = [[DaltonLensGUIMacOS alloc] init];
  }
  return self;
}

// MARK: - NSApplicationDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  [self createStatusBarItem];
  [self launchDaltonLensGUI];

  // Observe space changes
  __weak AppDelegate *weakSelf = self;
  self.spaceChangeObserver = [[NSWorkspace sharedWorkspace].notificationCenter
      addObserverForName:NSWorkspaceActiveSpaceDidChangeNotification
                  object:nil
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification *_Nonnull note) {
                [weakSelf.daltonGUI notifySpaceChanged];
              }];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  [self.daltonGUI shutdown];
  
  if (self.spaceChangeObserver) {
    [[NSWorkspace sharedWorkspace].notificationCenter
        removeObserver:self.spaceChangeObserver];
  }
}

// MARK: - Status Bar

- (void)createStatusBarItem {
  self.statusItem = [[NSStatusBar systemStatusBar]
      statusItemWithLength:NSSquareStatusItemLength];

  // Load icon from bundle resources (not asset catalog)
  NSString *iconPath = [[NSBundle mainBundle] pathForResource:@"DaltonLensIcon_32x32" ofType:@"png"];
  NSImage *icon = iconPath ? [[NSImage alloc] initWithContentsOfFile:iconPath] : nil;
  if (!icon) {
    // Fallback to system symbol if icon not found
    icon = [NSImage imageWithSystemSymbolName:@"eye"
                     accessibilityDescription:@"DaltonLens"];
    [icon setTemplate:YES];
  } else {
    // Set the size to 18x18 points for menu bar (the image will be scaled)
    [icon setSize:NSMakeSize(18, 18)];
    // Don't use template mode - this is a colorful icon
    [icon setTemplate:NO];
  }
  self.statusItem.button.image = icon;

  NSMenu *menu = [[NSMenu alloc] init];

  // Grab Screen Region
  NSMenuItem *grabItem =
      [[NSMenuItem alloc] initWithTitle:@"Grab Screen Region"
                                 action:@selector(grabScreenArea)
                          keyEquivalent:@" "];
  grabItem.keyEquivalentModifierMask = NSEventModifierFlagCommand |
                                       NSEventModifierFlagControl |
                                       NSEventModifierFlagOption;
  grabItem.target = self;
  [menu addItem:grabItem];

  [menu addItem:[NSMenuItem separatorItem]];

  // Help
  NSMenuItem *helpItem = [[NSMenuItem alloc] initWithTitle:@"Help"
                                                    action:@selector(showHelp)
                                             keyEquivalent:@""];
  helpItem.target = self;
  [menu addItem:helpItem];

  [menu addItem:[NSMenuItem separatorItem]];

  // Launch at Startup
  self.launchAtStartupMenuItem =
      [[NSMenuItem alloc] initWithTitle:@"Launch at Startup"
                                 action:@selector(toggleLaunchAtStartup:)
                          keyEquivalent:@""];
  self.launchAtStartupMenuItem.target = self;
  [self updateLaunchAtStartupMenuItemState];
  [menu addItem:self.launchAtStartupMenuItem];

  [menu addItem:[NSMenuItem separatorItem]];

  // Quit
  NSMenuItem *quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit"
                                                    action:@selector(quit)
                                             keyEquivalent:@"q"];
  quitItem.target = self;
  [menu addItem:quitItem];

  self.statusItem.menu = menu;
}

- (void)updateLaunchAtStartupMenuItemState {
  if (@available(macOS 13.0, *)) {
    SMAppService *service = [SMAppService mainAppService];
    BOOL isEnabled = (service.status == SMAppServiceStatusEnabled);
    self.launchAtStartupMenuItem.state =
        isEnabled ? NSControlStateValueOn : NSControlStateValueOff;
  }
}

// MARK: - DaltonLens GUI

- (void)launchDaltonLensGUI {
  [self.daltonGUI initialize];
}

// MARK: - Actions

- (void)grabScreenArea {
  [self.daltonGUI grabScreenArea];
}

- (void)showHelp {
  [self.daltonGUI helpRequested];
}

- (void)toggleLaunchAtStartup:(NSMenuItem *)sender {
  if (@available(macOS 13.0, *)) {
    SMAppService *service = [SMAppService mainAppService];
    BOOL wasEnabled = (service.status == SMAppServiceStatusEnabled);
    NSError *error = nil;

    if (wasEnabled) {
      [service unregisterAndReturnError:&error];
    } else {
      [service registerAndReturnError:&error];
    }

    if (error) {
      NSAlert *alert = [[NSAlert alloc] init];
      alert.alertStyle = NSAlertStyleCritical;
      alert.messageText = @"Could not change startup setting";
      alert.informativeText = error.localizedDescription;
      [alert runModal];
    }
    [self updateLaunchAtStartupMenuItemState];
  }
}

- (void)quit {
  [NSApplication.sharedApplication terminate:self];
}

@end

// MARK: - Main

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSApplication *app = [NSApplication sharedApplication];

    AppDelegate *delegate = [[AppDelegate alloc] init];
    app.delegate = delegate;

    [app run];
  }
  return 0;
}
