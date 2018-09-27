/*
  GridTools Libraries

  Copyright (c) 2017, ETH Zurich and MeteoSwiss
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  For information: http://eth-cscs.github.io/gridtools/
*/
#pragma once

#include "../../common/defs.hpp"
#include "../../common/host_device.hpp"

#include "../backend_ids.hpp"
#include "../coordinate.hpp"

namespace gridtools {
    namespace tmp_storage {
        // strategy::naive specialisations
        template <class StorageInfo, class /*MaxExtent*/, class GridType>
        uint_t get_i_size(
            backend_ids<platform::x86, GridType, strategy::naive> const &, uint_t /*block_size*/, uint_t total_size) {
            static constexpr auto halo = StorageInfo::halo_t::template at<
                coord_i<backend_ids<platform::x86, GridType, strategy::naive>>::value>();
            return total_size + 2 * halo;
        }

        template <class StorageInfo, class /*MaxExtent*/, class GridType>
        GT_FUNCTION int_t get_i_block_offset(
            backend_ids<platform::x86, GridType, strategy::naive> const &, uint_t /*block_size*/, uint_t /*block_no*/) {
            static constexpr auto halo = StorageInfo::halo_t::template at<
                coord_i<backend_ids<platform::x86, GridType, strategy::naive>>::value>();
            return halo;
        }

        template <class StorageInfo, class /*MaxExtent*/, class GridType>
        uint_t get_j_size(
            backend_ids<platform::x86, GridType, strategy::naive> const &, uint_t /*block_size*/, uint_t total_size) {
            static constexpr auto halo = StorageInfo::halo_t::template at<
                coord_j<backend_ids<platform::x86, GridType, strategy::naive>>::value>();
            return total_size + 2 * halo;
        }

        template <class StorageInfo, class /*MaxExtent*/, class GridType>
        GT_FUNCTION int_t get_j_block_offset(
            backend_ids<platform::x86, GridType, strategy::naive> const &, uint_t /*block_size*/, uint_t /*block_no*/) {
            static constexpr auto halo = StorageInfo::halo_t::template at<
                coord_j<backend_ids<platform::x86, GridType, strategy::naive>>::value>();
            return halo;
        }

        // Block specialisations
        template <class StorageInfo, class /*MaxExtent*/, class GridType>
        uint_t get_i_size(
            backend_ids<platform::x86, GridType, strategy::block> const &, uint_t block_size, uint_t /*total_size*/) {
            static constexpr auto halo = StorageInfo::halo_t::template at<
                coord_i<backend_ids<platform::x86, GridType, strategy::block>>::value>();
            return (block_size + 2 * halo) * omp_get_max_threads();
        }

        template <class StorageInfo, class /*MaxExtent*/, class GridType>
        GT_FUNCTION int_t get_i_block_offset(
            backend_ids<platform::x86, GridType, strategy::block> const &, uint_t block_size, uint_t /*block_no*/) {
            static constexpr auto halo = StorageInfo::halo_t::template at<
                coord_i<backend_ids<platform::x86, GridType, strategy::block>>::value>();
            return (block_size + 2 * halo) * omp_get_thread_num() + halo;
        }

        template <class StorageInfo, class /*MaxExtent*/, class GridType>
        uint_t get_j_size(
            backend_ids<platform::x86, GridType, strategy::block> const &, uint_t block_size, uint_t /*total_size*/) {
            static constexpr auto halo = StorageInfo::halo_t::template at<
                coord_j<backend_ids<platform::x86, GridType, strategy::block>>::value>();
            return block_size + 2 * halo;
        }

        template <class StorageInfo, class /*MaxExtent*/, class GridType>
        GT_FUNCTION int_t get_j_block_offset(
            backend_ids<platform::x86, GridType, strategy::block> const &, uint_t /*block_size*/, uint_t /*block_no*/) {
            static constexpr auto halo = StorageInfo::halo_t::template at<
                coord_j<backend_ids<platform::x86, GridType, strategy::block>>::value>();
            return halo;
        }
    } // namespace tmp_storage
} // namespace gridtools