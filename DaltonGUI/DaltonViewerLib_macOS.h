#import <Foundation/Foundation.h>

@interface DaltonViewerMacOS : NSObject
- (instancetype)init;
- (BOOL)initializeWithArgc:(int)argc argv:(NSArray<NSString*>*)argv;
- (void)runOnce;
- (BOOL)shouldExit;
@end

@interface DaltonPointerOverlayMacOS : NSObject
@property (readwrite,nonatomic) BOOL enabled;
- (instancetype)init;
- (BOOL)initialize;
- (void)runOnceWithMousePosX:(float)mousePosX mousePosY:(float)mousePosY allocateTextureBlock:(unsigned (^)())allocateTextureBlock;
@end
