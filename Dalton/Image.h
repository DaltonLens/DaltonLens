//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <memory>
#include <functional>
#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <algorithm>
#include <string>

namespace dl
{
    
    template <class T>
    class Image
    {
    public:
        inline int width () const { return _width; }
        inline int height () const { return _height; }
        
        inline bool hasData () const { return _width>0 && _height>0; }
        
        inline const T* data () const { return reinterpret_cast<T*>(_data); }
        inline T* data () { return reinterpret_cast<T*>(_data); }
        inline const uint8_t* rawBytes () const { return _data; }
        inline uint8_t* rawBytes () { return _data; }
        
        inline T* atRowPtr (int r) { return reinterpret_cast<T*>(_data + r*_bytesPerRow); }
        inline const T* atRowPtr (int r) const { return reinterpret_cast<T*>(_data + r*_bytesPerRow); }
        
        inline const T& operator()(int c, int r) const { return atRowPtr(r)[c]; }
        inline T& operator()(int c, int r) { return atRowPtr(r)[c]; }
        
        inline size_t sizeInBytes () const { return _bytesPerRow * _height; }
        inline size_t bytesPerRow () const { return _bytesPerRow; }
     
    public:
        using ReleaseFuncType = std::function<void(uint8_t** ptr)>;
        
        static ReleaseFuncType defaultFreeReleaseFunc ()
        {
            return [](uint8_t** ptr) { if (ptr != nullptr) free (*ptr); *ptr = nullptr; };
        }
        
        static ReleaseFuncType noopReleaseFunc ()
        {
            return [](uint8_t** ptr) {};
        }
        
    public:
        // Empty image.
        Image () = default;
        
        ~Image ()
        {
            releaseData ();
        }
        
        // Image with owned buffer.
        Image (int width, int height)
        {
            allocateOwnedBuffer (width, height);
        }
        
        // Move constructor
        Image (Image&& rhs)
        {
            // this is empty here.
            this->swap (rhs);
        }
        
        // Copy constructor
        Image (const Image& rhs)
        {
            allocateOwnedBuffer (rhs._width, rhs._height);
            copyDataFrom (rhs._data, rhs._bytesPerRow, rhs._width, rhs._height);
        }
        
        Image (uint8_t* otherData,
               int otherWidth,
               int otherHeight,
               int otherBytesPerRow,
               const ReleaseFuncType& releaseFunc = defaultFreeReleaseFunc())
        {
            _data = otherData;
            _width = otherWidth;
            _height = otherHeight;
            _bytesPerRow = otherBytesPerRow;
            _releaseFunc = releaseFunc;
        }
        
        // Move assignment operator
        Image& operator= (Image&& rhs)
        {
            releaseData ();
            _data = rhs.data;
            _width = rhs._width;
            _height = rhs._height;
            _bytesPerRow = rhs._bytesPerRow;
            _releaseFunc = rhs._releaseFunc;
            _allocatedBytes = rhs._allocatedBytes;

            rhs._data = nullptr;
            rhs._width = 0;
            rhs._height = 0;
            rhs._bytesPerRow = 0;
            rhs._releaseFunc = nullptr;
            rhs._allocatedBytes = 0;
            
            return *this;
        }
        
        // Copy assignment operator
        Image& operator= (const Image& rhs)
        {
            releaseData ();
            if (!rhs.hasData())
                return;
                
            allocateOwnedBuffer (rhs.width, rhs.height);
            copyFrom (rhs);
            return *this;
        }
        
        void ensureAllocatedBufferForSize (int width, int height)
        {
            if (_width == width && _height == height)
                return;
            
            auto requiredBytes = computeRequiredAllocatedBytesForSize (width, height);
            // fprintf (stderr, "Allocated: %zu Required=%lu\n", _allocatedBytes, requiredBytes);
            if (requiredBytes <= _allocatedBytes)
            {
                // Do not reallocate memory, just change the way we use it.
                _width = width;
                _height = height;
                _bytesPerRow = computeAlignedBytesPerRowForWidth(_width);
                return;
            }
            
            allocateOwnedBuffer (width, height);
        }
        
        void swap (Image& rhs)
        {
            std::swap (_data, rhs._data);
            std::swap(_width, rhs._width);
            std::swap(_height, rhs._height);
            std::swap(_bytesPerRow, rhs._bytesPerRow);
            std::swap(_releaseFunc, rhs._releaseFunc);
            std::swap(_allocatedBytes, rhs._allocatedBytes);
        }
        
        // Warning: does not allocate any data.
        void copyDataFrom (const Image& rhs)
        {
            copyDataFrom (rhs._data, rhs._bytesPerRow, rhs._width, rhs._height);
        }
        
        void copyDataFrom (const uint8_t* otherData, int otherBytesPerRow, int otherWidth, int otherHeight)
        {
            assert (_width == otherWidth);
            assert (_height == otherHeight);
            
            for (int row = 0; row < _height; ++row)
            {
                const uint8_t* otherRowPtr = otherData + otherBytesPerRow * row;
                uint8_t* thisRowPtr = _data + _bytesPerRow * row;
                memcpy (thisRowPtr, otherRowPtr, _width*sizeof(T));
            }
        }
        
        void fill (T value)
        {
            std::fill (data(), atRowPtr(height()), value);
        }
        
        template <class FuncT>
        void foreach_row (const FuncT& func) const
        {
            const int cols = width();
            for (int r = 0; r < height(); ++r)
                func (atRowPtr(r), cols);
        }
        
        template <class FuncT>
        void foreach_row (const FuncT& func)
        {
            const int cols = width();
            for (int r = 0; r < height(); ++r)
                func (atRowPtr(r), cols);
        }
        
        template <class FuncT>
        void apply (const FuncT& func)
        {
            const int cols = width();
            for (int r = 0; r < height(); ++r)
            {
                auto* rowPtr = atRowPtr(r);
                for (int c = 0; c < cols; ++c)
                {
                    func(c, r, rowPtr[c]);
                }
            }
        }
        
    private:
        
        static int computeAlignedBytesPerRowForWidth (int width)
        {
            int bytesPerRow = width*sizeof(T);
            bytesPerRow += (16-bytesPerRow%16)%16; // make sure each row is 16 bytes aligned for SIMD, etc.
            return bytesPerRow;
        }
        
        size_t computeRequiredAllocatedBytesForSize (int width, int height)
        {
            auto bytesPerRow = computeAlignedBytesPerRowForWidth(width);
            return bytesPerRow * height;
        }
        
        void allocateOwnedBuffer (int width, int height)
        {
            releaseData ();
            
            _width = width;
            _height = height;
            
            _bytesPerRow = computeAlignedBytesPerRowForWidth(width);
            
            // fprintf(stderr, "bytesPerRow before padding = %lu, after = %d\n", _width*sizeof(T), _bytesPerRow);
            
            auto sizeInBytes = computeRequiredAllocatedBytesForSize(width, height);
            assert (sizeInBytes > 0);
            
            _data = (uint8_t*) malloc(sizeInBytes);
            
            // fprintf (stderr, "Allocated new data, ptr = %p\n", _data);
            
            _allocatedBytes = sizeInBytes;
            
            _releaseFunc = [](uint8_t** ptr) {
                if (ptr != nullptr)
                    free (*ptr);
                *ptr = nullptr;
            };
        }
        
        void releaseData ()
        {
            if (_data == nullptr)
                return;
            
            if (_releaseFunc)
                _releaseFunc (&_data);
            
            _data = nullptr;
            _width = 0;
            _height = 0;
            _bytesPerRow = 0;
            _allocatedBytes = 0;
        }
    
    private:
        int _width = 0;
        int _height = 0;
        int _bytesPerRow = 0;
        uint8_t* _data = nullptr;
        ReleaseFuncType _releaseFunc = nullptr;
        
        // bytes that we allocated ourselves. Can be different from actual size if we used ensureAllocatedBufferForSize
        size_t _allocatedBytes = 0;
    };
    
    struct PixelSRGBA
    {
        PixelSRGBA() = default;
        
        PixelSRGBA (uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        : r(r), g(g), b(b), a(a)
        {}
        
        union {
            uint8_t v[4];
            struct {
                uint8_t r;
                uint8_t g;
                uint8_t b;
                uint8_t a;
            };
        };
        
        inline bool operator== (const PixelSRGBA& rhs) const
        {
            return r==rhs.r && g==rhs.g && b==rhs.b && a==rhs.a;
        }
    };
    
    struct PixelXYZ
    {
        PixelXYZ() = default;
        
        PixelXYZ (float x, float y, float z)
        : x(x), y(y), z(z)
        {}
        
        union {
            float v[3];
            
            struct {
                float x;
                float y;
                float z;
            };
            
            struct {
                float l;
                float m;
                float s;
            };
            
            struct {
                float r;
                float g;
                float b;
            };
        };
        
        bool operator== (const PixelXYZ& rhs) const {
            return x == rhs.x && y == rhs.y && z == rhs.z;
        }
    };
    
    struct PixelLinearRGB : public PixelXYZ { using PixelXYZ::PixelXYZ; };
    struct PixelLMS : public PixelXYZ { using PixelXYZ::PixelXYZ; };
    
    // Strong types to avoid confusion.
    using ImageSRGBA = Image<PixelSRGBA>;
    using ImageLinearRGB = Image<PixelLinearRGB>;
    using ImageXYZ = Image<PixelXYZ>;
    using ImageLMS = Image<PixelLMS>;
    
    bool readPngImage (const std::string& inputFileName,
                       ImageSRGBA& outputImage);
    
    bool writePngImage (const std::string& filePath,
                        const ImageSRGBA& image);
    
} // dl
