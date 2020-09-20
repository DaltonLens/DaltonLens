#include "DaltonViewerLib_macOS.h"
#include "DaltonViewerLib.h"

#include <vector>

@interface DaltonViewerMacOS() {
    DaltonViewer _viewer;
}
@end

@implementation DaltonViewerMacOS

- (instancetype)init
{
    self = [super init];
    return self;
}

- (BOOL)initializeWithArgc:(int)argc argv:(NSArray<NSString*>*)argv
{
    std::vector<char*> argv_c (argc);
    for (int i = 0; i < argc; ++i)
    {
        argv_c[i] = const_cast<char*>(argv[i].UTF8String);
    }
    return _viewer.initialize(argc, &argv_c[0]);
}

- (void)runOnce
{
    _viewer.runOnce();
}

@end
