//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#import <Foundation/Foundation.h>

#include <Dalton/Simulator.h>
#include <Dalton/Image.h>
#include <Dalton/Utils.h>

#include <Dalton/CpuProcessor.h>

#include <iostream>

using namespace dl;

int runOnImage (int argc, const char * argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: daltonTool inputImage.png outputImage.png" << std::endl;
        return 1;
    }
    
    ImageSRGBA inputImage;
    bool couldRead = dl::readPngImage(argv[1], inputImage);
    if (!couldRead)
    {
        std::cerr << "Could not open input image " << argv[1] << std::endl;
        return 1;
    }
    
    DLCpuProcessor* cpuProcessor = [DLCpuProcessor new];
    cpuProcessor.blindnessType = DLBlindnessType::Tritanope;
    cpuProcessor.processingMode = DLProcessingMode::DaltonizeV1;
    
    dl::ImageSRGBA outputImage = inputImage;
    
    [cpuProcessor transformSrgbaBuffer:(uint8_t*)outputImage.data() width:outputImage.width() height:outputImage.height()];
    
    dl::writePngImage(argv[2], outputImage);
    return 0;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        return runOnImage (argc, argv);
    }
    return 0;
}
