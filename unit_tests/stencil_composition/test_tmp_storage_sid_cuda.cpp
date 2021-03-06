/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <gridtools/stencil_composition/backend_cuda/tmp_storage_sid.hpp>

#include <memory>

#include <gtest/gtest.h>

#include <gridtools/common/integral_constant.hpp>
#include <gridtools/stencil_composition/sid/allocator.hpp>
#include <gridtools/stencil_composition/sid/concept.hpp>
#include <gridtools/tools/backend_select.hpp>

#include "../tools/multiplet.hpp"

namespace gridtools {
    namespace cuda {
        namespace {
            using namespace literals;

            using extent_t = extent<-1, 2, -3, 4>;
            constexpr auto blocksize_i = 32_c;
            constexpr auto blocksize_j = 8_c;

            int_t n_blocks_i = 11;
            int_t n_blocks_j = 12;
            int_t k_size = 13;

            template <class T>
            struct data_store {
                using data_t = T;
            };

#ifndef GT_ICOSAHEDRAL_GRIDS
            TEST(tmp_cuda_storage_sid, write_in_blocks) {
                using index_info = multiplet<5>;

                auto alloc = sid::make_allocator(&std::make_unique<char[]>);

                auto testee = make_tmp_storage<index_info>(
                    1_c, blocksize_i, blocksize_j, extent_t{}, n_blocks_i, n_blocks_j, k_size, alloc);

                auto strides = sid::get_strides(testee);
                auto origin = sid::get_origin(testee);

                // write block id
                for (int i = extent_t::iminus(); i < blocksize_i + extent_t::iplus(); ++i)
                    for (int j = extent_t::jminus(); j < blocksize_j + extent_t::jplus(); ++j)
                        for (int bi = 0; bi < n_blocks_i; ++bi)
                            for (int bj = 0; bj < n_blocks_j; ++bj)
                                for (int k = 0; k < k_size; ++k) {
                                    auto ptr = origin();
                                    sid::shift(ptr, host::at_key<dim::i>(strides), i);
                                    sid::shift(ptr, host::at_key<dim::j>(strides), j);
                                    sid::shift(ptr, host::at_key<sid::blocked_dim<dim::i>>(strides), bi);
                                    sid::shift(ptr, host::at_key<sid::blocked_dim<dim::j>>(strides), bj);
                                    sid::shift(ptr, host::at_key<dim::k>(strides), k);
                                    *ptr = {i, j, bi, bj, k};
                                }
                // validate that block id is correct, i.e. there were no overlapping memory accesses in the write
                for (int i = extent_t::iminus(); i < blocksize_i + extent_t::iplus(); ++i)
                    for (int j = extent_t::jminus(); j < blocksize_j + extent_t::jplus(); ++j)
                        for (int bi = 0; bi < n_blocks_i; ++bi)
                            for (int bj = 0; bj < n_blocks_j; ++bj)
                                for (int k = 0; k < k_size; ++k) {
                                    auto ptr = origin();
                                    sid::shift(ptr, host::at_key<dim::i>(strides), i);
                                    sid::shift(ptr, host::at_key<dim::j>(strides), j);
                                    sid::shift(ptr, host::at_key<sid::blocked_dim<dim::i>>(strides), bi);
                                    sid::shift(ptr, host::at_key<sid::blocked_dim<dim::j>>(strides), bj);
                                    sid::shift(ptr, host::at_key<dim::k>(strides), k);
                                    EXPECT_EQ((index_info{i, j, bi, bj, k}), *ptr);
                                }
            }

#else
            constexpr auto ncolors = 2_c;
            TEST(tmp_cuda_storage_sid_block, write_in_blocks) {
                using index_info = multiplet<6>;

                auto alloc = sid::make_allocator(&std::make_unique<char[]>);

                auto testee = make_tmp_storage<index_info>(
                    2_c, blocksize_i, blocksize_j, extent_t{}, n_blocks_i, n_blocks_j, k_size, alloc);

                auto strides = sid::get_strides(testee);
                auto origin = sid::get_origin(testee);

                // write block id
                for (int i = extent_t::iminus(); i < blocksize_i + extent_t::iplus(); ++i)
                    for (int j = extent_t::jminus(); j < blocksize_j + extent_t::jplus(); ++j)
                        for (int c = 0; c < ncolors; ++c)
                            for (int bi = 0; bi < n_blocks_i; ++bi)
                                for (int bj = 0; bj < n_blocks_j; ++bj)
                                    for (int k = 0; k < k_size; ++k) {
                                        auto ptr = origin();
                                        sid::shift(ptr, host::at_key<dim::i>(strides), i);
                                        sid::shift(ptr, host::at_key<dim::j>(strides), j);
                                        sid::shift(ptr, host::at_key<dim::c>(strides), c);
                                        sid::shift(ptr, host::at_key<sid::blocked_dim<dim::i>>(strides), bi);
                                        sid::shift(ptr, host::at_key<sid::blocked_dim<dim::j>>(strides), bj);
                                        sid::shift(ptr, host::at_key<dim::k>(strides), k);
                                        *ptr = {i, j, c, bi, bj, k};
                                    }

                // validate that block id is correct, i.e. there were no overlapping memory accesses in the write
                for (int i = extent_t::iminus(); i < blocksize_i + extent_t::iplus(); ++i)
                    for (int j = extent_t::jminus(); j < blocksize_j + extent_t::jplus(); ++j)
                        for (int c = 0; c < ncolors; ++c)
                            for (int bi = 0; bi < n_blocks_i; ++bi)
                                for (int bj = 0; bj < n_blocks_j; ++bj)
                                    for (int k = 0; k < k_size; ++k) {
                                        auto ptr = origin();
                                        sid::shift(ptr, host::at_key<dim::i>(strides), i);
                                        sid::shift(ptr, host::at_key<dim::j>(strides), j);
                                        sid::shift(ptr, host::at_key<dim::c>(strides), c);
                                        sid::shift(ptr, host::at_key<sid::blocked_dim<dim::i>>(strides), bi);
                                        sid::shift(ptr, host::at_key<sid::blocked_dim<dim::j>>(strides), bj);
                                        sid::shift(ptr, host::at_key<dim::k>(strides), k);
                                        EXPECT_EQ((index_info{i, j, c, bi, bj, k}), *ptr);
                                    }
            }
#endif
        } // namespace
    }     // namespace cuda
} // namespace gridtools
