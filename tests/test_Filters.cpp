//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include <Dalton/Utils.h>
#include <Dalton/MathUtils.h>
#include <Dalton/Image.h>
#include <Dalton/Filters.h>
#include <Dalton/OpenGL.h>

#include <tests/Common.h>

#include <gl3w/GL/gl3w.h>
#include <GLFW/glfw3.h>

using namespace dl;

bool pixelsAreSimilar (const PixelSRGBA& p1, const PixelSRGBA& p2, int maxDiff)
{
    for (int i = 0; i < 4; ++i)
        if (std::abs(p1.v[i] - p2.v[i]) > maxDiff)
            return false;
    return true;
}

bool imagesAreSimilar (const ImageSRGBA& im1, const ImageSRGBA& im2, int maxDiff)
{
    if (im1.width() != im2.width() || im1.height() != im2.height())
    {
        dl_dbg ("Size mismatch");
        return false;
    }

    for (int r = 0; r < im1.height(); ++r)
    {
        const PixelSRGBA* p1Row = im1.atRowPtr(r);
        const PixelSRGBA* p2Row = im2.atRowPtr(r);
        for (int c = 0; c < im1.width(); ++c)
        {
            if (!pixelsAreSimilar(p1Row[c], p2Row[c], maxDiff))
            {
                dl_dbg ("Images differ at row %d col %d (%d %d %d %d) vs (%d %d %d %d)",
                        r, c, 
                        p1Row[c].r, p1Row[c].g, p1Row[c].b, p1Row[c].a,
                        p2Row[c].r, p2Row[c].g, p2Row[c].b, p2Row[c].a);
                return false;
            }
        }
    }

    return true;
}

UTEST(Daltonize, DaltonizeGPU)
{
    ImageSRGBA im;
    const std::string sourceImagePrefix = TEST_IMAGES_DIR "rgbColorSpan";
    readPngImage (sourceImagePrefix + ".png", im);
    ASSERT_EQ(im.width(), 216);

    int glfw_err = glfwInit();
    ASSERT_EQ(glfw_err, GLFW_TRUE);

    GLContext context;

    // Initialize the GL loader once the context has been created.
    int gl3w_err = gl3wInit();
    ASSERT_EQ(gl3w_err, 0);

    GLTexture texture;
    texture.initialize ();
    texture.upload (im);

    Filter_Daltonize filter;
    filter.initializeGL ();

    ImageSRGBA output;
    GLFilterProcessor processor;
    processor.initializeGL ();

    auto testDaltonize = [&](const Filter_Daltonize::Params& params, const std::string& name) {
        filter.setParams (params);
        processor.render (filter, texture.textureId(), texture.width(), texture.height(), &output);
        ASSERT_EQ(output.width(), im.width());
        ASSERT_EQ(output.height(), im.height());
        
        ImageSRGBA gt_im;
        readPngImage (sourceImagePrefix + "_" + name + ".png", gt_im);
        ASSERT_EQ(gt_im.width(), output.width());

        ASSERT_TRUE(imagesAreSimilar(gt_im, output, 1));
        // writePngImage ("test_Filters_GPU_output_" + name + ".png", output);
    };

    Filter_Daltonize::Params params;
    params.simulateOnly = false;

    params.kind = Filter_Daltonize::Params::Kind::Protanope;
    testDaltonize (params, "daltonize_protanope");
    
    params.kind = Filter_Daltonize::Params::Kind::Deuteranope;
    testDaltonize (params, "daltonize_deuteranope");

    params.kind = Filter_Daltonize::Params::Kind::Tritanope;
    testDaltonize (params, "daltonize_tritanope");
}

UTEST(Daltonize, DaltonizeCPU)
{
    ImageSRGBA im;
    const std::string sourceImagePrefix = TEST_IMAGES_DIR "rgbColorSpan";
    readPngImage (sourceImagePrefix + ".png", im);
    ASSERT_EQ(im.width(), 216);

    Filter_Daltonize filter;
    ImageSRGBA output;

    auto testDaltonize = [&](const Filter_Daltonize::Params& params, const std::string& name) {
        filter.setParams (params);
        filter.applyCPU (im, output);
        ASSERT_EQ(output.width(), im.width());
        ASSERT_EQ(output.height(), im.height());
        
        ImageSRGBA gt_im;
        readPngImage (sourceImagePrefix + "_" + name + ".png", gt_im);
        ASSERT_EQ(gt_im.width(), output.width());

        ASSERT_TRUE(imagesAreSimilar(gt_im, output, 2));
        // writePngImage ("test_Filters_CPU_output_" + name + ".png", output);
    };

    Filter_Daltonize::Params params;
    params.simulateOnly = false;

    params.kind = Filter_Daltonize::Params::Kind::Protanope;
    testDaltonize (params, "daltonize_protanope");
    
    params.kind = Filter_Daltonize::Params::Kind::Deuteranope;
    testDaltonize (params, "daltonize_deuteranope");

    params.kind = Filter_Daltonize::Params::Kind::Tritanope;
    testDaltonize (params, "daltonize_tritanope");
}

UTEST_MAIN();