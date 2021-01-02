//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "DaltonLensGUI_macOS.h"
#include "DaltonLensGUI.h"

#include <vector>

@interface DaltonLensGUIMacOS() {
    dl::DaltonLensGUI _dlGui;
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
    return _dlGui.initialize();
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

@end
