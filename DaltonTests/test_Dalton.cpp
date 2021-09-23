//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include <Dalton/Utils.h>
#include <Dalton/MathUtils.h>
#include <Dalton/Image.h>

#include <DaltonTests/Common.h>

using namespace dl;

UTEST(Image, BasicRGBA)
{
    ImageSRGBA im1(639, 479);
    std::fill(im1.atRowPtr(0), im1.atRowPtr(480), PixelSRGBA(255, 127, 63, 31));

    ASSERT_TRUE(im1(320, 240) == PixelSRGBA(255, 127, 63, 31));

    ImageSRGBA im2 = im1;
    ASSERT_NE(im1.data(), im2.data());

    ASSERT_TRUE(im2(320, 240) == PixelSRGBA(255, 127, 63, 31));

    ImageSRGBA im3 = std::move(im2);
    ASSERT_EQ(im2.data(), nullptr);
    ASSERT_TRUE(im3(320, 240) == PixelSRGBA(255, 127, 63, 31));

    auto *prevData = im3.data();
    im3.ensureAllocatedBufferForSize(320, 240);
    ASSERT_EQ(im3.data(), prevData);
    im3.ensureAllocatedBufferForSize(639, 479);
    ASSERT_EQ(im3.data(), prevData);

    // Still equal since padding should compensate.
    // FIXME: this test is actually failing! What should we do?
    im3.ensureAllocatedBufferForSize(640, 480);
    ASSERT_EQ(im3.data(), prevData);

    im3.ensureAllocatedBufferForSize(641, 480);
    // This test is actually not guaranteed, since the OS might very well reuse the same address.
    // XCTAssert(im3.data() != prevData);
    im3(640, 479) = PixelSRGBA(255, 255, 63, 31);
    ASSERT_TRUE(im3(640, 479) == PixelSRGBA(255, 255, 63, 31));
}

UTEST(Image, BasicXYZ)
{
    ImageXYZ im1 (640, 480);
    std::fill (im1.atRowPtr(0), im1.atRowPtr(480), PixelXYZ(1.0, 2.0, 3.0));
    
    ASSERT_TRUE(im1(320,240) == PixelXYZ(1.0, 2.0, 3.0));
    
    ImageXYZ im2 = im1;
    ASSERT_TRUE(im1.data() != im2.data());
    
    ASSERT_TRUE(im2(320,240) == PixelXYZ(1.0, 2.0, 3.0));
    
    ImageXYZ im3 = std::move(im2);
    ASSERT_EQ(im2.data(), nullptr);
    ASSERT_TRUE(im3(320,240) == PixelXYZ(1.0, 2.0, 3.0));
}

UTEST(MathUtils, Rect)
{
    Rect r1 = Rect::from_x_y_w_h(10, 20, 30, 40);
    Rect r2 = Rect::from_x_y_w_h(-5, 30, 15, 20);
    auto r3 = r1.intersect(r2);
    ASSERT_DOUBLE_EQ_WITH_ACCURACY(r3.origin.x, 10, 1e-7);
    ASSERT_DOUBLE_EQ_WITH_ACCURACY(r3.origin.y, 30, 1e-7);
    ASSERT_DOUBLE_EQ_WITH_ACCURACY(r3.size.x, 0, 1e-7);
    ASSERT_DOUBLE_EQ_WITH_ACCURACY(r3.size.y, 20, 1e-7);
}

UTEST_MAIN();
