//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <Foundation/Foundation.h>

#import "MetalProcessor.h"

#include <cstdint>

@interface DLMetalProcessor()
{
    NSMutableDictionary* _pipelineStates;
    // var pipelineStates : [UInt32 /* processing mode */ : Array<MTLRenderPipelineState?>]
}

@property id<MTLDevice> mtlDevice;
@property id<MTLLibrary> mtlLibrary;

@property MTLVertexDescriptor* mtlVertexDescriptor;
@property id<MTLFunction> mtlVertexQuad;

@end

@implementation DLMetalProcessor

- (instancetype)initWithDevice:(id<MTLDevice>)device
{
    self = [super init];
    if (self)
    {
        self.processingMode = DLProcessingMode::Nothing;
        self.blindnessType = DLBlindnessType::Protanope;
        
        self.mtlDevice = device;
        
        // Using this to support unit tests.
        NSBundle *bundle = [NSBundle bundleForClass:[self class]];
        NSError* error = nil;
        self.mtlLibrary = [device newDefaultLibraryWithBundle:bundle error:&error];
        
        NSAssert(self.mtlLibrary != nil, @"Could not create Metal library %@", [error localizedDescription]);
        
        self.mtlVertexQuad = [self.mtlLibrary newFunctionWithName:@"vertex_quad"];
        
        // float4 XYZW float2 UV
        self.mtlVertexDescriptor = [[MTLVertexDescriptor alloc] init];
        self.mtlVertexDescriptor.attributes[0].format = MTLVertexFormatFloat4;
        self.mtlVertexDescriptor.attributes[0].bufferIndex = 0;
        self.mtlVertexDescriptor.attributes[0].offset = 0;
        
        self.mtlVertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
        self.mtlVertexDescriptor.attributes[1].bufferIndex = 0;
        self.mtlVertexDescriptor.attributes[1].offset = 4*sizeof(float);
        
        self.mtlVertexDescriptor.layouts[0].stride = (4+2)*sizeof(float);
        self.mtlVertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
        
        _pipelineStates = [[NSMutableDictionary alloc] init];
        [self populatePipelinesDictionary];
    }
    return self;
}

-(id<MTLRenderPipelineState>) filteringPipeline
{
    NSArray* pipelinesForMode = _pipelineStates[@(self.processingMode)];
    if ([pipelinesForMode count] > 1)
    {
        return (id<MTLRenderPipelineState>)pipelinesForMode[self.blindnessType];
    }
    return (id<MTLRenderPipelineState>)pipelinesForMode[0];
}

-(void)populatePipelinesDictionary
{
    _pipelineStates[@(Nothing)] = @[[self createPipelineWithFragment:@"fragment_passthrough"]];
    
    _pipelineStates[@(SimulateDaltonism)] = @[[self createPipelineWithFragment:@"fragment_simulateDaltonism_protanope"],
                                              [self createPipelineWithFragment:@"fragment_simulateDaltonism_deuteranope"],
                                              [self createPipelineWithFragment:@"fragment_simulateDaltonism_tritanope"],
                                              ];
    
    _pipelineStates[@(DaltonizeV1)] = @[
                                        [self createPipelineWithFragment:@"fragment_daltonizeV1_protanope"],
                                        [self createPipelineWithFragment:@"fragment_daltonizeV1_deuteranope"],
                                        [self createPipelineWithFragment:@"fragment_daltonizeV1_tritanope"],
                                        ];
    
    _pipelineStates[@(SwitchCbCr)] = @[[self createPipelineWithFragment:@"fragment_swapCbCr"]];
    _pipelineStates[@(SwitchAndFlipCbCr)] = @[[self createPipelineWithFragment:@"fragment_swapAndFlipCbCr"]];
    _pipelineStates[@(InvertLightness)] = @[[self createPipelineWithFragment:@"fragment_invertLightness"]];
    _pipelineStates[@(HighlightColorUnderMouse)] = @[[self createPipelineWithFragment:@"fragment_highlightSameColorWithAntialising"]];
    _pipelineStates[@(HighlightExactColorUnderMouse)] = @[[self createPipelineWithFragment:@"fragment_highlightSameColor"]];
}

-(id<MTLRenderPipelineState>) createPipelineWithFragment:(NSString*)fragmentName
{
    auto* fragmentShader = [self.mtlLibrary newFunctionWithName:fragmentName];
    
    auto* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.vertexFunction = self.mtlVertexQuad;
    pipelineDesc.fragmentFunction = fragmentShader;
    pipelineDesc.vertexDescriptor = self.mtlVertexDescriptor;
    pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    
    NSError* error = nil;
    id<MTLRenderPipelineState> pipelineState = [self.mtlDevice newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
    
    if (pipelineState == nil)
    {
        @throw @"Could not create rendering pipeline.";
    }
    
    return pipelineState;
}

@end
