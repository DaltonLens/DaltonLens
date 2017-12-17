//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <Foundation/Foundation.h>

#import <CoreImage/CoreImage.h>

#import "MetalRenderer.h"

#import "MetalRendererImpl.h"

#include <cstdint>

@interface DLMetalRenderer()
{
    DLMetalProcessor* _processor;
    dl::MetalDataOnScreen _mtl;
}
@end

@implementation DLMetalRenderer

- (instancetype)initWithProcessor:(DLMetalProcessor*)processor
{
    self = [super init];
    if (self)
    {
        _processor = processor;
        _mtl.initialize(processor.mtlDevice);
    }
    return self;
}

-(DLMetalUniforms*) uniformsBuffer
{
    return static_cast<DLMetalUniforms*>([_mtl.uniformBuffer contents]);
}

- (void)renderWithScreenImage:(CGImageRef)screenImage
                commandBuffer:(id<MTLCommandBuffer>)commandBuffer
         renderPassDescriptor:(MTLRenderPassDescriptor*)rpd
{
    const int width = (int)CGImageGetWidth(screenImage);
    const int height = (int)CGImageGetHeight(screenImage);
    
    rpd.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    
    CIImage* ciImage = [[CIImage alloc] initWithCGImage:screenImage];
    
    _mtl.ensureTextureInitialized(&_mtl.screenTexture,
                                  width,
                                  height,
                                  MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite);
    
    // Render the CoreImage context onto the Metal texture. This should all happen in GPU.
    [_mtl.ciContext render:ciImage
              toMTLTexture:_mtl.screenTexture commandBuffer:commandBuffer
                    bounds:CGRectMake(0,0,width,height)
                colorSpace:CGColorSpaceCreateWithName(kCGColorSpaceSRGB)];
    
    auto pipelineState = _processor.filteringPipeline;
    
    auto commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:rpd];
    [commandEncoder setRenderPipelineState:pipelineState];
    [commandEncoder setVertexBuffer:_mtl.quadVertexBuffer offset:0 atIndex:0];
    [commandEncoder setFragmentTexture:_mtl.screenTexture atIndex:0];
    [commandEncoder setFragmentBuffer:_mtl.uniformBuffer offset:0 atIndex: 0];
    [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:1];
    [commandEncoder endEncoding];
}

@end

