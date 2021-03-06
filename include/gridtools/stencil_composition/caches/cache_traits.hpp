/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
   @file
   @brief File containing metafunctions and traits common to all cache classes
*/

#pragma once

#include <type_traits>

#include "../../common/integral_constant.hpp"
#include "../../meta/macros.hpp"
#include "cache.hpp"

namespace gridtools {

    /**
     * @struct is_ij_cache
     * metafunction determining if a type is a cache of IJ type
     */
    template <typename T>
    struct is_ij_cache : std::false_type {};

    template <typename Arg, cache_io_policy cacheIOPolicy>
    struct is_ij_cache<detail::cache_impl<cache_type::ij, Arg, cacheIOPolicy>> : std::true_type {};

    /**
     * @struct is_k_cache
     * metafunction determining if a type is a cache of K type
     */
    template <typename T>
    struct is_k_cache : std::false_type {};

    template <typename Arg, cache_io_policy cacheIOPolicy>
    struct is_k_cache<detail::cache_impl<cache_type::k, Arg, cacheIOPolicy>> : std::true_type {};

    /**
     * @struct is_flushing_cache
     * metafunction determining if a type is a flush cache
     */
    template <typename T>
    struct is_flushing_cache : std::false_type {};

    template <cache_type cacheType, typename Arg>
    struct is_flushing_cache<detail::cache_impl<cacheType, Arg, cache_io_policy::flush>> : std::true_type {};

    template <cache_type cacheType, typename Arg>
    struct is_flushing_cache<detail::cache_impl<cacheType, Arg, cache_io_policy::fill_and_flush>> : std::true_type {};

    /**
     * @struct is_filling_cache
     * metafunction determining if a type is a filling cache
     */
    template <typename T>
    struct is_filling_cache : std::false_type {};

    template <cache_type cacheType, typename Arg>
    struct is_filling_cache<detail::cache_impl<cacheType, Arg, cache_io_policy::fill>> : std::true_type {};

    template <cache_type cacheType, typename Arg>
    struct is_filling_cache<detail::cache_impl<cacheType, Arg, cache_io_policy::fill_and_flush>> : std::true_type {};

    template <typename T>
    struct is_local_cache : std::false_type {};

    template <cache_type cacheType, typename Arg>
    struct is_local_cache<detail::cache_impl<cacheType, Arg, cache_io_policy::local>> : std::true_type {};

    /**
     * @struct cache_parameter
     *  trait returning the parameter Arg type of a user provided cache
     */

    namespace lazy {
        template <typename T>
        struct cache_parameter;

        template <cache_type cacheType, typename Arg, cache_io_policy cacheIOPolicy>
        struct cache_parameter<detail::cache_impl<cacheType, Arg, cacheIOPolicy>> {
            using type = Arg;
        };
    } // namespace lazy
    GT_META_DELEGATE_TO_LAZY(cache_parameter, typename T, T);

    namespace cache_traits_impl_ {
        template <cache_io_policy>
        struct make_cache_io_policies : meta::list<> {};

        template <>
        struct make_cache_io_policies<cache_io_policy::fill_and_flush>
            : meta::list<integral_constant<cache_io_policy, cache_io_policy::fill>,
                  integral_constant<cache_io_policy, cache_io_policy::flush>> {};
        template <>
        struct make_cache_io_policies<cache_io_policy::fill>
            : meta::list<integral_constant<cache_io_policy, cache_io_policy::fill>> {};

        template <>
        struct make_cache_io_policies<cache_io_policy::flush>
            : meta::list<integral_constant<cache_io_policy, cache_io_policy::flush>> {};

        template <class Plh, class CacheTypes = meta::list<>, class CacheIOPolicies = meta::list<>>
        struct cache_info {
            using plh_t = Plh;
            using cache_types_t = CacheTypes;
            using cache_io_policies_t = CacheIOPolicies;
        };

        template <class Cache>
        using make_cache_map_item = cache_info<typename Cache::arg_t,
            meta::list<integral_constant<cache_type, Cache::cacheType>>,
            typename make_cache_io_policies<Cache::cacheIOPolicy>::type>;

        template <class Caches>
        using make_cache_map = meta::transform<make_cache_map_item, Caches>;

        template <class Map, class Plh>
        using lookup_cache_map = meta::mp_find<Map, Plh, cache_info<Plh>>;
    } // namespace cache_traits_impl_
    using cache_traits_impl_::lookup_cache_map;
    using cache_traits_impl_::make_cache_map;

} // namespace gridtools
