/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <type_traits>

#include "../common/defs.hpp"
#include "../common/host_device.hpp"
#include "../common/integral_constant.hpp"

namespace gridtools {
    namespace execute {
        struct parallel {};
        struct forward {};
        struct backward {};

        template <typename T>
        struct is_parallel : std::false_type {};

        template <>
        struct is_parallel<parallel> : std::true_type {};

        template <typename T>
        struct is_forward : std::false_type {};

        template <>
        struct is_forward<forward> : std::true_type {};

        template <typename T>
        struct is_backward : std::false_type {};

        template <>
        struct is_backward<backward> : std::true_type {};

        template <typename T>
        constexpr integral_constant<int_t, is_backward<T>::value ? -1 : 1> step = {};
    } // namespace execute

    template <typename T>
    struct is_execution_engine : std::false_type {};

    template <>
    struct is_execution_engine<execute::parallel> : std::true_type {};

    template <>
    struct is_execution_engine<execute::forward> : std::true_type {};

    template <>
    struct is_execution_engine<execute::backward> : std::true_type {};

} // namespace gridtools
