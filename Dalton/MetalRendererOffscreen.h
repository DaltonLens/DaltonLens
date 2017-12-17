//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#import "Common.h"
#import "Image.h"
#import "MetalProcessor.h"

#import <Foundation/Foundation.h>

#import <Metal/Metal.h>

@interface DLDLMetalRendererOffscreen : NSObject

@property (readonly) DLMetalUniforms* uniformsBuffer;

- (id)init UNAVAILABLE_ATTRIBUTE;
- (instancetype)initWithProcessor:(DLMetalProcessor*)processor;
- (void)applyPipelineToSRGBAImage:(const dl::ImageSRGBA&)image outputImage:(dl::ImageSRGBA&)outputImage;

@end
