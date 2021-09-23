//
// Copyright (c) 2017, Nicolas Burrus
// This software may be modified and distributed under the terms
// of the BSD license.  See the LICENSE file for details.
//

#pragma once

#include <utest/utest.h>

#define ASSERT_DOUBLE_EQ_WITH_ACCURACY(x,y,eps)                                \
  UTEST_SURPRESS_WARNING_BEGIN do {                                            \
    double diff = (double)x-double(y);                                         \
    diff = diff < 0. ? -diff : diff;                                           \
    if (diff > eps) {                                                          \
      UTEST_PRINTF("%s:%u: Failure\n", __FILE__, __LINE__);                    \
      UTEST_PRINTF("  Expected : %f\n", (double)x);                            \
      UTEST_PRINTF("    Actual : %f\n", (double)y);                            \
      UTEST_PRINTF("      Diff : %f > %f\n", diff, (double)eps);               \
      *utest_result = 1;                                                       \
      return;                                                                  \
    }                                                                          \
  }                                                                            \
  while (0)                                                                    \
  UTEST_SURPRESS_WARNING_END