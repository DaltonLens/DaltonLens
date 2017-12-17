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

using namespace dl;

@interface DLImageTests : XCTestCase

@end

@implementation DLImageTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testBasicRGBAOperations {
    
    ImageSRGBA im1 (639, 479);
    std::fill (im1.atRowPtr(0), im1.atRowPtr(480), PixelSRGBA(255,127,63,31));
    
    XCTAssert(im1(320,240) == PixelSRGBA(255,127,63,31));
    
    ImageSRGBA im2 = im1;
    XCTAssert(im1.data() != im2.data());
    
    XCTAssert(im2(320,240) == PixelSRGBA(255,127,63,31));
    
    ImageSRGBA im3 = std::move(im2);
    XCTAssert(im2.data() == nullptr);
    XCTAssert(im3(320,240) == PixelSRGBA(255,127,63,31));
    
    auto* prevData = im3.data();
    im3.ensureAllocatedBufferForSize(320, 240);
    XCTAssert(im3.data() == prevData);
    im3.ensureAllocatedBufferForSize(639, 479);
    XCTAssert(im3.data() == prevData);
    
    // Still equal since padding should compensate.
    im3.ensureAllocatedBufferForSize(640, 480);
    XCTAssert(im3.data() == prevData);
    
    im3.ensureAllocatedBufferForSize(641, 480);
    // This test is actually not guaranteed, since the OS might very well reuse the same address.
    // XCTAssert(im3.data() != prevData);
    im3(640,479) = PixelSRGBA(255,255,63,31);
    XCTAssert(im3(640,479) == PixelSRGBA(255,255,63,31));
}

- (void)testBasicXYZOperations {
    
    ImageXYZ im1 (640, 480);
    std::fill (im1.atRowPtr(0), im1.atRowPtr(480), PixelXYZ(1.0, 2.0, 3.0));
    
    XCTAssert(im1(320,240) == PixelXYZ(1.0, 2.0, 3.0));
    
    ImageXYZ im2 = im1;
    XCTAssert(im1.data() != im2.data());
    
    XCTAssert(im2(320,240) == PixelXYZ(1.0, 2.0, 3.0));
    
    ImageXYZ im3 = std::move(im2);
    XCTAssert(im2.data() == nullptr);
    XCTAssert(im3(320,240) == PixelXYZ(1.0, 2.0, 3.0));
    
}

@end
