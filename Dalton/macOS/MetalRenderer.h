//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#import "Common.h"
#import "MetalProcessor.h"

#import <Foundation/Foundation.h>

#import <CoreGraphics/CoreGraphics.h>

#import <Metal/Metal.h>

@interface DLMetalRenderer : NSObject

@property (readonly) struct DLMetalUniforms* uniformsBuffer;

- (id)init UNAVAILABLE_ATTRIBUTE;
- (instancetype)initWithProcessor:(DLMetalProcessor*)processor;
- (void)renderWithScreenImage:(CGImageRef)screenImage
                commandBuffer:(id<MTLCommandBuffer>)commandBuffer
         renderPassDescriptor:(MTLRenderPassDescriptor*)rpd;
@end
