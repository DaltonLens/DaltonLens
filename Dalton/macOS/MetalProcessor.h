//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#import "Common.h"

#import "CpuProcessor.h"

#import <Foundation/Foundation.h>

#import <Metal/Metal.h>

struct DLMetalUniforms
{
    float underCursorRgba[4];
    float grabScreenRectangle[4];
    int32_t frameCount;
    int32_t padding[3];
};

@interface DLMetalProcessor : NSObject
@property DLProcessingMode processingMode;
@property DLBlindnessType blindnessType;
@property (readonly) id<MTLDevice> mtlDevice;
@property (readonly) id<MTLRenderPipelineState> filteringPipeline;

- (id)init UNAVAILABLE_ATTRIBUTE;
- (instancetype)initWithDevice:(id<MTLDevice>)device;
@end
