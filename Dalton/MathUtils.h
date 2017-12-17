//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <algorithm>

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

} // dl
