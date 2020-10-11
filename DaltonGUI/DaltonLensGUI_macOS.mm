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

@end
