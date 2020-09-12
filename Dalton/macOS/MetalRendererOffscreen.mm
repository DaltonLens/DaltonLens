//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <Foundation/Foundation.h>

#import <CoreImage/CoreImage.h>

#import "MetalRendererOffscreen.h"

#import "MetalRendererImpl.h"

#include <cstdint>

@interface DLDLMetalRendererOffscreen()
{
    DLMetalProcessor* _processor;
    dl::MetalDataOffScreen _mtl;
}
@end

@implementation DLDLMetalRendererOffscreen

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

- (void)applyPipelineToSRGBAImage:(const dl::ImageSRGBA&)image outputImage:(dl::ImageSRGBA&)outputImage
{
    _mtl.ensureTextureInitialized(&_mtl.inputTexture, image.width(), image.height(), MTLTextureUsageShaderRead);
    _mtl.ensureTextureInitialized(&_mtl.outputTexture, image.width(), image.height(), MTLTextureUsageRenderTarget);
    
    outputImage.ensureAllocatedBufferForSize(image.width(), image.height());
    
    auto mtlEntireImageRegion = MTLRegionMake2D(0, 0, image.width(), image.height());
    
    [_mtl.inputTexture replaceRegion:mtlEntireImageRegion
                         mipmapLevel:0 withBytes:image.data() bytesPerRow:image.bytesPerRow()];
    
    _mtl.renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(1.0, 0, 0, 1.0);
    _mtl.renderPassDescriptor.colorAttachments[0].texture = _mtl.outputTexture;
    _mtl.renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    _mtl.renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    
    auto pipelineState = _processor.filteringPipeline;
    
    // Now add a blit to the CPU-accessible buffer
    if (_mtl.outputBuffer.length != outputImage.sizeInBytes())
    {
        _mtl.outputBuffer = [_mtl.device newBufferWithLength:outputImage.sizeInBytes()
                                                     options:MTLResourceOptionCPUCacheModeDefault];
    }
    
    auto commandBuffer = [_mtl.commandQueue commandBuffer];
    
    auto commandEncoder = [commandBuffer renderCommandEncoderWithDescriptor:_mtl.renderPassDescriptor];
    [commandEncoder setRenderPipelineState:pipelineState];
    [commandEncoder setVertexBuffer:_mtl.quadVertexBuffer offset:0 atIndex:0];
    [commandEncoder setFragmentTexture:_mtl.inputTexture atIndex:0];
    [commandEncoder setFragmentBuffer:_mtl.uniformBuffer offset:0 atIndex: 0];
    [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:1];
    [commandEncoder endEncoding];
    
    id<MTLBlitCommandEncoder> blitCommandEncoder = commandBuffer.blitCommandEncoder;
    [blitCommandEncoder copyFromTexture:_mtl.outputTexture
                            sourceSlice:0
                            sourceLevel:0
                           sourceOrigin:MTLOriginMake(0, 0, 0)
                             sourceSize:MTLSizeMake(outputImage.width(), outputImage.height(), 1)
                               toBuffer:_mtl.outputBuffer
                      destinationOffset:0
                 destinationBytesPerRow:outputImage.bytesPerRow()
               destinationBytesPerImage:outputImage.sizeInBytes()
                                options:MTLBlitOptionNone];
    [blitCommandEncoder endEncoding];
    
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
    
    assert(commandBuffer.error == nil);
    assert(commandBuffer.status == MTLCommandBufferStatusCompleted);
    
    dl::ImageSRGBA flippedSRGBAView ((uint8_t*)_mtl.outputBuffer.contents,
                                     outputImage.width(),
                                     outputImage.height(),
                                     (int)outputImage.bytesPerRow(),
                                     nullptr);
    
    for (int r = 0; r < outputImage.height(); ++r)
    {
        memcpy (outputImage.atRowPtr(r), flippedSRGBAView.atRowPtr(outputImage.height()-r-1), outputImage.bytesPerRow());
    }
}

@end
