//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <algorithm>
#include <cmath>

namespace dl
{

    struct ColMajorMatrix3f
    {
        union {
            float v[3*3];
            
            // mRowCol
            struct {
                float m00, m10, m20;
                float m01, m11, m21;
                float m02, m12, m22;
            };
        };
        
        ColMajorMatrix3f() = default;
        
        // mRowCol
        ColMajorMatrix3f(float m00, float m01, float m02,
                         float m10, float m11, float m12,
                         float m20, float m21, float m22)
        : m00(m00), m01(m01), m02(m02),
          m10(m10), m11(m11), m12(m12),
          m20(m20), m21(m21), m22(m22)
        {
            
        }
    };
    
    inline uint8_t saturateAndCast (float v)
    {
        return std::max (std::min(v, 255.f), 0.f);
    };

    inline uint8_t roundAndSaturateToUint8 (float v)
    {
        return (uint8_t)std::max (std::min(v + 0.5f, 255.f), 0.f);
    };

    inline double pow7 (double x)
    {
        double pow3 = x*x*x;
        return pow3*pow3*x;
    }

    inline double sqr(double x)
    {
        return x*x;
    }

    static constexpr double Rad2Deg = 57.29577951308232;
    static constexpr double Deg2Rad = 0.017453292519943295;

    template <class T>
    T keepInRange (T value, T min, T max)
    {
        if (value < min)
            return min;
        if (value > max)
            return max;
        return value;
    }

    struct vec2i
    {
        vec2i (int x = 0, int y = 0) : x(x), y(y) {}
        
        union {
            int v[2];
            struct { int x, y; };
            struct { int col, row; };
        };
    };

    inline bool operator== (const vec2i& lhs, const vec2i& rhs){
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }

    struct Point
    {
        Point() = default;
        Point (double x, double y) : x(x), y(y) {}
    
        inline Point& operator-=(const Point& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }
        
        double x = NAN;
        double y = NAN;
    };

    inline Point operator-(const Point& p1, const Point& p2)
    {
        Point p = p1; p -= p2; return p;
    }

    struct Rect
    {
        Point origin;
        Point size;
        
        bool contains (const Point& p) const
        {
            return (p.x >= origin.x
                    && p.x < origin.x + size.x
                    && p.y >= origin.y
                    && p.y < origin.y + size.y);
        }
        
        Rect& operator*= (double s)
        {
            origin.x *= s;
            origin.y *= s;
            size.x *= s;
            size.y *= s;
            return *this;
        }
    };

} // dl
