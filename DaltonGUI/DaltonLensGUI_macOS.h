#import <Foundation/Foundation.h>

@interface DaltonLensGUIMacOS : NSObject
- (instancetype)init;
- (BOOL)initialize;
- (void)shutdown;
- (void)grabScreenArea;
- (void)helpRequested;
@end
