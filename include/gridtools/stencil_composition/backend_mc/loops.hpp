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
#include <utility>

#include "../../common/defs.hpp"
#include "../../common/generic_metafunctions/for_each.hpp"
#include "../../common/tuple_util.hpp"
#include "../../meta.hpp"
#include "../dim.hpp"
#include "../sid/concept.hpp"
#include "execinfo_mc.hpp"

namespace gridtools {
    namespace mc {
        namespace loops_impl_ {
            template <class Stage, class Ptr, class Strides>
            GT_FORCE_INLINE void i_loop(int_t size, Stage stage, Ptr &ptr, Strides const &strides) {
#ifdef NDEBUG
// TODO(anstaf & fthaler):
//   Maybe we have to re-run tests with different combinations of pragmas on different compilers,
//   the current set of pragmas is at the border of legality for the present code, so maybe we can find a better option.
#pragma ivdep
#ifndef __INTEL_COMPILER
#pragma omp simd
#endif
#endif
                for (int_t i = 0; i < size; ++i) {
                    using namespace literals;
                    stage(ptr, strides);
                    sid::shift(ptr, sid::get_stride<dim::i>(strides), 1_c);
                }
                sid::shift(ptr, sid::get_stride<dim::i>(strides), -size);
            }

            template <class Ptr, class Strides>
            struct k_i_loops_f {
                int_t m_i_size;
                Ptr &m_ptr;
                Strides const &m_strides;

                template <class Cell, class KSize>
                GT_FORCE_INLINE void operator()(Cell cell, KSize k_size) const {
                    for (int_t k = 0; k < k_size; ++k) {
                        i_loop(m_i_size, cell, m_ptr, m_strides);
                        cell.inc_k(m_ptr, m_strides);
                    }
                }
            };

            template <class Ptr, class Strides>
            GT_FORCE_INLINE k_i_loops_f<Ptr, Strides> make_k_i_loops(int_t i_size, Ptr &ptr, Strides const &strides) {
                return {i_size, ptr, strides};
            }

            template <class Stage, class Grid, class Composite, class KSizes>
            auto make_loop(std::true_type, Grid const &grid, Composite composite, KSizes k_sizes) {
                using extent_t = typename Stage::extent_t;
                using ptr_diff_t = sid::ptr_diff_type<Composite>;
                auto strides = sid::get_strides(composite);
                ptr_diff_t offset{};
                sid::shift(offset, sid::get_stride<dim::i>(strides), extent_t::minus(dim::i()));
                sid::shift(offset, sid::get_stride<dim::j>(strides), extent_t::minus(dim::j()));
                return [origin = sid::get_origin(composite) + offset,
                           strides = std::move(strides),
                           k_start = grid.k_start(Stage::interval()),
                           k_sizes = std::move(k_sizes)](execinfo_block_kparallel_mc const &info) {
                    ptr_diff_t offset{};
                    sid::shift(offset, sid::get_stride<dim::thread>(strides), omp_get_thread_num());
                    sid::shift(offset, sid::get_stride<sid::blocked_dim<dim::i>>(strides), info.i_block);
                    sid::shift(offset, sid::get_stride<sid::blocked_dim<dim::j>>(strides), info.j_block);
                    sid::shift(offset, sid::get_stride<dim::k>(strides), info.k);
                    auto ptr = origin() + offset;

                    int_t j_count = extent_t::extend(dim::j(), info.j_block_size);
                    int_t i_size = extent_t::extend(dim::i(), info.i_block_size);

                    for (int_t j = 0; j < j_count; ++j) {
                        using namespace literals;
                        int_t cur = k_start;
                        tuple_util::for_each(
                            [&ptr, &strides, &cur, k = info.k, i_size](auto cell, auto k_size) {
                                if (k >= cur && k < cur + k_size)
                                    i_loop(i_size, cell, ptr, strides);
                                cur += k_size;
                            },
                            Stage::cells(),
                            k_sizes);
                        sid::shift(ptr, sid::get_stride<dim::j>(strides), 1_c);
                    }
                };
            }

            template <class Grid, class Loops>
            void run_loops(std::true_type, Grid const &grid, Loops loops) {
                execinfo_mc info(grid);
                int_t i_blocks = info.i_blocks();
                int_t j_blocks = info.j_blocks();
                int_t k_size = grid.k_size();
#pragma omp parallel for collapse(3)
                for (int_t j = 0; j < j_blocks; ++j) {
                    for (int_t k = 0; k < k_size; ++k) {
                        for (int_t i = 0; i < i_blocks; ++i) {
                            tuple_util::for_each([block = info.block(i, j, k)](auto &&loop) { loop(block); }, loops);
                        }
                    }
                }
            }

            template <class Stage, class Grid, class Composite, class KSizes>
            auto make_loop(std::false_type, Grid const &grid, Composite composite, KSizes k_sizes) {
                using extent_t = typename Stage::extent_t;
                using ptr_diff_t = sid::ptr_diff_type<Composite>;

                auto strides = sid::get_strides(composite);
                ptr_diff_t offset{};
                sid::shift(offset, sid::get_stride<dim::i>(strides), extent_t::minus(dim::i()));
                sid::shift(offset, sid::get_stride<dim::j>(strides), extent_t::minus(dim::j()));
                sid::shift(
                    offset, sid::get_stride<dim::k>(strides), grid.k_start(Stage::interval(), Stage::execution()));

                return [origin = sid::get_origin(composite) + offset,
                           strides = std::move(strides),
                           k_shift_back = -grid.k_size(Stage::interval()) * Stage::k_step(),
                           k_sizes = std::move(k_sizes)](execinfo_block_kserial_mc const &info) {
                    sid::ptr_diff_type<Composite> offset{};
                    sid::shift(offset, sid::get_stride<dim::thread>(strides), omp_get_thread_num());
                    sid::shift(offset, sid::get_stride<sid::blocked_dim<dim::i>>(strides), info.i_block);
                    sid::shift(offset, sid::get_stride<sid::blocked_dim<dim::j>>(strides), info.j_block);
                    auto ptr = origin() + offset;

                    int_t j_size = extent_t::extend(dim::j(), info.j_block_size);
                    int_t i_size = extent_t::extend(dim::i(), info.i_block_size);

                    auto k_i_loops = make_k_i_loops(i_size, ptr, strides);
                    for (int_t j = 0; j < j_size; ++j) {
                        using namespace literals;
                        tuple_util::for_each(k_i_loops, Stage::cells(), k_sizes);
                        sid::shift(ptr, sid::get_stride<dim::k>(strides), k_shift_back);
                        sid::shift(ptr, sid::get_stride<dim::j>(strides), 1_c);
                    }
                };
            }

            template <class Grid, class Loops>
            void run_loops(std::false_type, Grid const &grid, Loops loops) {
                execinfo_mc info(grid);
                int_t i_blocks = info.i_blocks();
                int_t j_blocks = info.j_blocks();
#pragma omp parallel for collapse(2)
                for (int_t j = 0; j < j_blocks; ++j) {
                    for (int_t i = 0; i < i_blocks; ++i) {
                        tuple_util::for_each([block = info.block(i, j)](auto &&loop) { loop(block); }, loops);
                    }
                }
            }
        } // namespace loops_impl_
        using loops_impl_::make_loop;
        using loops_impl_::run_loops;
    } // namespace mc
} // namespace gridtools
