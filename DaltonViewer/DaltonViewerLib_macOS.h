#import <Foundation/Foundation.h>

@interface DaltonViewerMacOS : NSObject
- (instancetype)init;
- (BOOL)initializeWithArgc:(int)argc argv:(NSArray<NSString*>*)argv;
- (void)runOnce;
- (BOOL)shouldExit;
@end
