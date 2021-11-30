//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <Foundation/Foundation.h>

@interface DaltonLensGUIMacOS : NSObject
- (instancetype)init;
- (BOOL)initialize;
- (void)shutdown;
- (void)grabScreenArea;
- (void)helpRequested;
- (void)notifySpaceChanged;
@end
