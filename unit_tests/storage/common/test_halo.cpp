/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "gtest/gtest.h"

#include <type_traits>

#include <gridtools/common/gt_assert.hpp>
#include <gridtools/storage/common/halo.hpp>

using namespace gridtools;

TEST(Halo, HaloTest) {
    // test zero halo getter
    GT_STATIC_ASSERT((std::is_same<zero_halo<4>, halo<0, 0, 0, 0>>::value), "get zero halo failed");
    GT_STATIC_ASSERT((std::is_same<zero_halo<3>, halo<0, 0, 0>>::value), "get zero halo failed");
    GT_STATIC_ASSERT((std::is_same<zero_halo<2>, halo<0, 0>>::value), "get zero halo failed");
    GT_STATIC_ASSERT((std::is_same<zero_halo<1>, halo<0>>::value), "get zero halo failed");

    // test value correctness
    GT_STATIC_ASSERT((halo<2, 3, 4>::at<0>() == 2), "halo value is wrong");
    GT_STATIC_ASSERT((halo<2, 3, 4>::at<1>() == 3), "halo value is wrong");
    GT_STATIC_ASSERT((halo<2, 3, 4>::at<2>() == 4), "halo value is wrong");
}
