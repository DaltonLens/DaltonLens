//
//  main.m
//  DaltonLensLaunchAtLoginHelper
//
//  Created by Nicolas Burrus on 3/13/17.
//  Copyright © 2017 Nicolas Burrus. All rights reserved.
//

#import <Cocoa/Cocoa.h>

int main(int argc, const char * argv[]) {
    
    // Check if main app is already running; if yes, do nothing and terminate helper app
    BOOL alreadyRunning = NO;
    NSArray *running = [[NSWorkspace sharedWorkspace] runningApplications];
    for (NSRunningApplication *app in running) {
        if ([[app bundleIdentifier] isEqualToString:@"org.daltonLens.DaltonLens"]) {
            alreadyRunning = YES;
        }
    }
    
    if (!alreadyRunning) {
        // Recover the parent app folder.
        NSString *path = [[[[[[NSBundle mainBundle] bundlePath]
                             stringByDeletingLastPathComponent]
                            stringByDeletingLastPathComponent]
                           stringByDeletingLastPathComponent]
                          stringByDeletingLastPathComponent];
        [[NSWorkspace sharedWorkspace] launchApplication:path];
    }
    [NSApp terminate:nil];
}
