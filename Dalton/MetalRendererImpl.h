//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#import "MetalProcessor.h"

#import <Metal/Metal.h>

#import <CoreImage/CoreImage.h>

#include <vector>

namespace dl
{
    
    struct MetalDataCommon
    {
        virtual void initialize(id<MTLDevice> device)
        {
            this->device = device;
            commandQueue = [device newCommandQueue];
            quadVertexData = {
                // float4 XYZW
                // float2 UV
                1.0, -1.0, 0.0, 1.0,
                1.0, 0.0,
                
                -1.0, -1.0, 0.0, 1.0,
                0.0, 0.0,
                
                1.0, 1.0, 0.0, 1.0,
                1.0, 1.0,
                
                -1.0, 1.0, 0.0, 1.0,
                0.0, 1.0
            };
            
            auto quadVertexDataSize = quadVertexData.size() * sizeof(float);
            quadVertexBuffer = [device newBufferWithBytes:quadVertexData.data()
                                                   length:quadVertexDataSize
                                                  options:MTLResourceCPUCacheModeDefaultCache];
            
            uniformBuffer = [device newBufferWithLength:sizeof(DLMetalUniforms)
                                                options:MTLResourceCPUCacheModeWriteCombined];
            
            ciContext = [CIContext contextWithMTLDevice:device];
        }
        
        void ensureTextureInitialized (id<MTLTexture> __strong * texturePtr,
                                       int width,
                                       int height,
                                       MTLTextureUsage textureUsage)
        {
            id<MTLTexture> texture = *texturePtr;
            
            if (texture == nil
                || texture.width != width
                || texture.height != height)
            {
                auto* textureDesc = [MTLTextureDescriptor
                                     texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                     width:width
                                     height:height
                                     mipmapped:NO];
                
                textureDesc.usage = textureUsage;
                
                texture = [device newTextureWithDescriptor:textureDesc];
                *texturePtr = texture;
            }
        }
        
        CIContext* ciContext = nil;
        id<MTLDevice> device = nil;
        id<MTLCommandQueue> commandQueue = nil;
        std::vector<float> quadVertexData;
        id<MTLBuffer> quadVertexBuffer = nil;
        id<MTLBuffer> uniformBuffer = nil;
        id<MTLBuffer> outputBuffer = nil;
    };
    
    struct MetalDataOnScreen : public MetalDataCommon
    {
        id<MTLTexture> screenTexture = nil;
    };
    
    struct MetalDataOffScreen : public MetalDataCommon
    {
        id<MTLTexture> outputTexture = nil;
        id<MTLTexture> inputTexture = nil;
        MTLRenderPassDescriptor* renderPassDescriptor = nil;
        
        void initialize(id<MTLDevice> device) override
        {
            MetalDataCommon::initialize(device);
            renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        }
    };

    
} // dl

