//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <XCTest/XCTest.h>
#import <CoreImage/CoreImage.h>

#include <Dalton/Simulator.h>
#include <Dalton/Image.h>
#include <Dalton/Utils.h>

#include <Dalton/MetalProcessor.h>
#include <Dalton/MetalRendererOffscreen.h>

#include <algorithm>
#include <fstream>
#include <vector>

using namespace dl;

@interface MetalTests : XCTestCase
{
    DLMetalProcessor* _metalProcessor;
    id<MTLDevice> _mtlDevice;
    DLDLMetalRendererOffscreen* _offscreenWrapper;
    
    DLCpuProcessor* _cpuProcessor;
}
@end

@implementation MetalTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
    
    _mtlDevice = MTLCreateSystemDefaultDevice();
    XCTAssert(_mtlDevice);
    _metalProcessor = [[DLMetalProcessor alloc] initWithDevice:_mtlDevice];
    
    _offscreenWrapper = [[DLDLMetalRendererOffscreen alloc] initWithProcessor:_metalProcessor];
    DLMetalUniforms* uniformsBuffer = _offscreenWrapper.uniformsBuffer;
    uniformsBuffer[0].underCursorRgba[0] = 0;
    uniformsBuffer[0].underCursorRgba[1] = 0;
    uniformsBuffer[0].underCursorRgba[2] = 0;
    uniformsBuffer[0].underCursorRgba[3] = 1.0;
    uniformsBuffer[0].frameCount = 1;
    
    _cpuProcessor = [[DLCpuProcessor alloc] init];
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)compareImage:(const dl::ImageSRGBA&)firstImage with:(const dl::ImageSRGBA&)secondImage
{
    auto pixelsAreSimilar = [](const dl::PixelSRGBA& lhs, const dl::PixelSRGBA& rhs, int threshold) {
        return (std::abs(lhs.r - rhs.r) <= threshold
                && std::abs(lhs.g - rhs.g) <= threshold
                && std::abs(lhs.b - rhs.b) <= threshold
                && std::abs(lhs.a - rhs.a) <= threshold);
    };
    
    for (int r = 0; r < firstImage.height(); ++r)
        for (int c = 0; c < firstImage.width(); ++c)
        {
            auto cpuV = firstImage(c,r);
            auto metalV = secondImage(c,r);
            bool ok = pixelsAreSimilar(cpuV, metalV, 2);
            
            XCTAssert(ok, "Result is not the same [%d %d %d %d] -> [%d %d %d %d]",
                      cpuV.r, cpuV.g, cpuV.b, cpuV.a,
                      metalV.r, metalV.g, metalV.b, metalV.a);
            if (!ok)
                break;
        }
}

- (void)runProcessingMode:(DLProcessingMode)processingMode
            blindnessType:(DLBlindnessType)blindnessType
               inputSRGBA:(const dl::ImageSRGBA&)inputSRGBA
{
    _metalProcessor.processingMode = DLProcessingMode::DaltonizeV1;
    _metalProcessor.blindnessType = blindnessType;
    
    _cpuProcessor.processingMode = _metalProcessor.processingMode;
    _cpuProcessor.blindnessType = _metalProcessor.blindnessType;
    
    dl::ImageSRGBA metalOutput;
    {
        dl::ScopeTimer _ ("Metal::applyPipelineToSRGBAImage");
        [_offscreenWrapper applyPipelineToSRGBAImage:inputSRGBA outputImage:metalOutput];
    }
    
    dl::ImageSRGBA cpuOutput = inputSRGBA;
    [_cpuProcessor transformSrgbaBuffer:(uint8_t*)cpuOutput.data()
                                  width:cpuOutput.width()
                                 height:cpuOutput.height()];
    
    [self compareImage:metalOutput with:cpuOutput];
    
    dl::writePngImage(dl::formatted("/tmp/debug_output_%d_%d_metal.png",
                                    (int)processingMode, (int)blindnessType),
                      metalOutput);
    
    dl::writePngImage(dl::formatted("/tmp/debug_output_%d_%d_cpu.png",
                                    (int)processingMode, (int)blindnessType),
                      cpuOutput);
}

- (void)testDaltonizeV1 {
    
    _metalProcessor.processingMode = DLProcessingMode::DaltonizeV1;
    _metalProcessor.blindnessType = DLBlindnessType::Tritanope;
    
    dl::ImageSRGBA inputSRGBA (320,240);
    inputSRGBA.apply([](int c, int r, auto& v) {
        v = PixelSRGBA(c%255, r%255, (c+r)%255, 255);
    });
    
    dl::writePngImage("/tmp/debug_input.png", inputSRGBA);
        
    for (const auto blindnessType : {
        DLBlindnessType::Protanope,
        DLBlindnessType::Deuteranope,
        DLBlindnessType::Tritanope})
    {
        [self runProcessingMode:DLProcessingMode::DaltonizeV1
                  blindnessType:blindnessType
                     inputSRGBA:inputSRGBA];
    }
}

@end
