//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "DaltonLensGUI_macOS.h"
#include "DaltonLensGUI.h"
#include "PlatformSpecific.h"

#include <vector>

@interface DaltonLensGUIMacOS() {
    dl::DaltonLensGUI _dlGui;
    dl::DisplayLinkTimer _dlTimer;
}
@end

@implementation DaltonLensGUIMacOS

- (instancetype)init
{
    self = [super init];
    return self;
}

- (BOOL)initialize
{
    BOOL success = _dlGui.initialize();
    if (!success)
        return NO;

    [self start];
    return YES;
}

- (void)start
{
    _dlTimer.setCallback ([self]() { _dlGui.runOnce (); });
}

- (void)shutdown
{
    _dlGui.shutdown();
}

- (void)grabScreenArea
{
    _dlGui.toogleGrabScreenArea();
}

- (void)helpRequested
{
    _dlGui.helpRequested();
}

-(void)notifySpaceChanged
{
    _dlGui.notifySpaceChanged();
}

@end
