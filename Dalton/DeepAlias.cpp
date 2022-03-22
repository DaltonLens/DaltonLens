//
// Copyright (c) 2022, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#include "DeepAlias.h"

#include <Dalton/Image.h>
#include <Dalton/Utils.h>

#include <zv/Client.h>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>

namespace dl
{

cv::Mat3b toMat3b(const ImageSRGBA& inputRgba)
{
    int ncols = inputRgba.width();
    int nrows = inputRgba.height();
    cv::Mat3b cv_im (nrows, ncols);
    for (int r = 0; r < nrows; ++r)
    {
        const PixelSRGBA* inRowPtr = inputRgba.atRowPtr(r);
        cv::Vec3b* outRowPtr = cv_im[r];
        for (int c = 0; c < ncols; ++c)
        {
            outRowPtr[c][0] = inRowPtr[c].r;
            outRowPtr[c][1] = inRowPtr[c].g;
            outRowPtr[c][2] = inRowPtr[c].b;
        }
    }
    return cv_im;
}

dl::ImageSRGBA denormalizeToSRGBA(const cv::Mat3f& inputFloat, float mean, float dev)
{
    const int ncols = inputFloat.cols;
    const int nrows = inputFloat.rows;
    dl::ImageSRGBA outIm (ncols, nrows);
    for (int r = 0; r < nrows; ++r)
    {
        PixelSRGBA* outRowPtr = outIm.atRowPtr(r);
        const cv::Vec3f* inRowPtr = inputFloat[r];
        for (int c = 0; c < ncols; ++c)
        {
            // https://stackoverflow.com/questions/1914115/converting-color-value-from-float-0-1-to-byte-0-255
            outRowPtr[c].r = cv::saturate_cast<uint8_t>((inRowPtr[c][0] * dev + mean)*256.f);
            outRowPtr[c].g = cv::saturate_cast<uint8_t>((inRowPtr[c][1] * dev + mean)*256.f);
            outRowPtr[c].b = cv::saturate_cast<uint8_t>((inRowPtr[c][2] * dev + mean)*256.f);
            outRowPtr[c].a = 255;
        }
    }
    return outIm;
}

struct DeepAlias::Impl
{
    cv::dnn::Net net;

    bool ensureNetLoaded ()
    {
        if (!net.empty())
            return true;

        // FIXME: need to store this resource somewhere.
        net = cv::dnn::readNetFromONNX("/home/nb/Perso/DaltonLensPrivate/charts/pretrained/regression_unetres_v4.onnx");

        return !net.empty();
    }
};

DeepAlias::DeepAlias ()
: impl (new Impl())
{}
    
DeepAlias::~DeepAlias () = default;

namespace { // anonymous

int numSlices (int targetSize, int maxSize)
{
    int slices = 1;
    while (std::ceil(float(targetSize) / slices) > maxSize)
    {
        slices += 1;
    }
    return slices;
}

int findClosestSize (int minSize, const std::vector<int>& possibleSizes)
{
    return *std::upper_bound (possibleSizes.begin(), possibleSizes.end(), minSize);
}

float tileCenterInImage (int inputSize, int numSlices, int tileIdx)
{
    const float sizePerSlice = float (inputSize) / numSlices;
    const float start = tileIdx * sizePerSlice;
    const float end = (tileIdx + 1) * sizePerSlice;
    return (start + end) * 0.5f;

}

} // anonymous

ImageSRGBA DeepAlias::undoAntiAliasing (const ImageSRGBA& inputRgba)
{
    ScopeTimer _ ("DeepAlias.undoAntialiasing");

    if (!impl->ensureNetLoaded ())
        return inputRgba;

    const double mean = 0.5;
    const double dev = 0.5;

    // Multiples of 64. Don't allow sizes that are too small to make sure that we'll keep some context.
    std::vector<int> possibleSizes = {128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960, 1024, 1088, 1152, 1216, 1280 };

    std::vector<cv::Rect> rois;

    const int w = inputRgba.width();
    const int h = inputRgba.height();

    dl_dbg ("inputSize %d %d", w, h);

    int numHorizSlices = numSlices (w, possibleSizes.back());
    int numVertSlices = numSlices (h, possibleSizes.back());
    dl_dbg ("slices %d %d", numHorizSlices, numVertSlices);

    int minTileWidth = std::ceil(float(w) / numHorizSlices);
    int minTileHeight = std::ceil(float(h) / numVertSlices);

    int tileWidth = findClosestSize (minTileWidth, possibleSizes);
    int tileHeight = findClosestSize (minTileHeight, possibleSizes);

    dl_dbg ("minTile %d %d", minTileWidth, minTileHeight);
    dl_dbg ("finalTile %d %d", tileWidth, tileHeight);

    cv::Mat3b cvInput = toMat3b (inputRgba);
    cv::Mat3b cvTile (tileHeight, tileWidth);
    cv::Mat3f cvOutput (inputRgba.height(), inputRgba.width());
    cvOutput = cv::Vec3f(1.f, 0.f, 0.f);

    for (int tile_r = 0; tile_r < numVertSlices; ++tile_r)
    for (int tile_c = 0; tile_c < numHorizSlices; ++tile_c)
    {
        dl_dbg ("tile %d %d", tile_r, tile_c);
        cvTile = cv::Vec3b(0,0,0);

        const float xCenter = tileCenterInImage (w, numHorizSlices, tile_c);
        const float yCenter = tileCenterInImage (h, numVertSlices, tile_r);

        cv::Rect inputROI;
        inputROI.width = std::min (tileWidth, w);
        inputROI.height = std::min (tileHeight, h);
        inputROI.x = std::roundf(xCenter - inputROI.width/2.f);
        inputROI.y = std::roundf(yCenter - inputROI.height/2.f);
        inputROI.x = keepInRange (inputROI.x, 0, w - inputROI.width);
        inputROI.y = keepInRange (inputROI.y, 0, h - inputROI.height);

        dl_dbg ("inputROI %d %d %d %d", inputROI.x, inputROI.y, inputROI.width, inputROI.height);

        cv::Rect tileROI = cv::Rect(0,0,inputROI.width, inputROI.height);
        cvInput(inputROI).copyTo(cvTile(tileROI));

        cv::Mat4b cvTileRGBA;
        cv::cvtColor (cvTile, cvTileRGBA, cv::COLOR_RGB2RGBA);
        zv::logImageRGBA (formatted("input-%d-%d", tile_r, tile_c), cvTileRGBA.data, cvTileRGBA.cols, cvTileRGBA.rows, cvTileRGBA.step);

        // TODO: could try to fill a batch instead of processing them one by one?

        cv::Mat inputBlob = cv::dnn::blobFromImage (cvTile, 1.f / (dev * 255.f), cv::Size (), cv::Scalar::all (mean * 255.f), false /* swapRB */, false /* crop */, CV_32F);
        impl->net.setInput (inputBlob, "input");
        cv::Mat output_blob = impl->net.forward ();

        std::vector<cv::Mat> outputImages;
        cv::dnn::imagesFromBlob (output_blob, outputImages);
        cv::Mat3f output3f = outputImages[0];

        output3f(tileROI).copyTo(cvOutput(inputROI));
    }

    ImageSRGBA outputSRGBA = denormalizeToSRGBA(cvOutput, mean, dev);
    return outputSRGBA;
}

} // dl