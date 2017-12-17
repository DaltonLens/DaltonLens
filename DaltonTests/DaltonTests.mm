//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <XCTest/XCTest.h>

#include <Dalton/Simulator.h>
#include <Dalton/Image.h>
#include <Dalton/Utils.h>

#include <algorithm>
#include <fstream>

using namespace dl;

@interface DaltonTests : XCTestCase

@end

@implementation DaltonTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

// Adapted from http://stackoverflow.com/questions/6163611/compare-two-files
bool areFilesIdentical (const std::string& path1, const std::string& path2)
{
    using std::ifstream;
    
    ifstream f1 (path1);
    assert ((bool)f1);
    
    ifstream f2 (path2);
    assert ((bool)f2);
    
    auto size1 = f1.seekg(0, ifstream::end).tellg();
    f1.seekg(0, ifstream::beg);
    
    auto size2 = f2.seekg(0, ifstream::end).tellg();
    f2.seekg(0, ifstream::beg);
    
    if (size1 != size2)
        return false;
    
    constexpr const size_t BLOCKSIZE = 4096;
    
    size_t remaining = size1;
    while (remaining)
    {
        char buffer1[BLOCKSIZE], buffer2[BLOCKSIZE];
        size_t size = std::min(BLOCKSIZE, remaining);
        
        f1.read(buffer1, size);
        f2.read(buffer2, size);
        
        if (memcmp(buffer1, buffer2, size) != 0)
            return false;
        
        remaining -= size;
    }
    
    return true;
}

- (void)testSimulate {
    
    NSBundle *bundle = [NSBundle bundleForClass:[DaltonTests class]];
    NSString *imagePath = [bundle pathForResource:@"RandomPlots" ofType:@"png"];
    
    ImageSRGBA srgbaImage;
    const bool couldLoad = dl::readPngImage([imagePath UTF8String], srgbaImage);
    XCTAssert(couldLoad, "Could not load the image");
    
    ImageLMS lmsImage;
    SRGBAToLMSConverter converter;
    converter.convertToLms(srgbaImage, lmsImage);
    
    ImageLMS deuteranopeLmsImage = lmsImage;
    ImageLMS protanopeLmsImage = lmsImage;
    ImageLMS tritanopeLmsImage = lmsImage;
    
    Simulator simulator;
    simulator.applySimulation(deuteranopeLmsImage, DLBlindnessType::Deuteranope);
    simulator.applySimulation(protanopeLmsImage, DLBlindnessType::Protanope);
    simulator.applySimulation(tritanopeLmsImage, DLBlindnessType::Tritanope);
    
    for (const auto& it : {
        std::make_pair("deuteranope", &deuteranopeLmsImage),
        std::make_pair("protanope",   &protanopeLmsImage),
        std::make_pair("tritanope",   &tritanopeLmsImage),
    })
    {
        const std::string testName = it.first;
        const auto& lmsImage = *it.second;
        
        NSLog(@"Testing %s", testName.c_str());
        
        ImageSRGBA simulatedImage;
        converter.convertToSrgba(lmsImage, simulatedImage);
        
        const std::string tmpFileName = "/tmp/RandomPlots_" + testName + ".png";
        dl::writePngImage (tmpFileName, simulatedImage);
        
        std::string refFileName = "ref_RandomPlots_" + testName;
        NSString* refFilePath = [bundle pathForResource:[NSString stringWithUTF8String:refFileName.c_str()] ofType:@"png"];
        
        NSAssert(refFilePath, @"Reference file %s not in Bundle?", refFileName.c_str());
        
        XCTAssert (areFilesIdentical(tmpFileName, [refFilePath UTF8String]));
    }
}

@end
