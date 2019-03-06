/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <gtest/gtest.h>

#include <gridtools/stencil-composition/conditionals/case_.hpp>
#include <gridtools/stencil-composition/conditionals/switch_.hpp>
#include <gridtools/stencil-composition/stencil-composition.hpp>
#include <gridtools/tools/computation_fixture.hpp>

namespace gridtools {
    namespace {
        template <uint_t Id>
        struct functor {
            using p_dummy = inout_accessor<0>;

            using param_list = make_param_list<p_dummy>;

            template <typename Evaluation>
            GT_FUNCTION static void apply(Evaluation &eval) {
                eval(p_dummy()) += Id;
            }
        };

        struct stencil_composition : computation_fixture<> {
            stencil_composition() : computation_fixture<>(1, 1, 1) {}
        };

        TEST_F(stencil_composition, conditional_switch) {
            bool p;
            auto comp = make_computation(
                make_multistage(execute::forward(), make_stage<functor<0>>(p_0), make_stage<functor<0>>(p_0)),
                switch_([&p] { return p ? 0 : 5; },
                    case_(0,
                        make_multistage(execute::forward(), make_stage<functor<1>>(p_0), make_stage<functor<1>>(p_0))),
                    case_(5,
                        switch_([] { return 1; },
                            case_(1,
                                make_multistage(execute::forward(),
                                    make_stage<functor<2000>>(p_0),
                                    make_stage<functor<2000>>(p_0))),
                            default_(make_multistage(
                                execute::forward(), make_stage<functor<3000>>(p_0), make_stage<functor<3000>>(p_0))))),
                    default_(
                        make_multistage(execute::forward(), make_stage<functor<7>>(p_0), make_stage<functor<7>>(p_0)))),
                switch_([&p] { return p ? 1 : 2; },
                    case_(2,
                        make_multistage(
                            execute::forward(), make_stage<functor<10>>(p_0), make_stage<functor<10>>(p_0))),
                    case_(1,
                        make_multistage(
                            execute::forward(), make_stage<functor<20>>(p_0), make_stage<functor<20>>(p_0))),
                    default_(make_multistage(
                        execute::forward(), make_stage<functor<30>>(p_0), make_stage<functor<30>>(p_0)))),
                make_multistage(execute::forward(), make_stage<functor<400>>(p_0), make_stage<functor<400>>(p_0)));

            auto dummy = make_storage();

            p = true;
            comp.run(p_0 = dummy);
            verify(make_storage(842.), dummy);

            p = false;
            comp.run(p_0 = dummy);
            verify(make_storage(5662.), dummy);
        }
    } // namespace
} // namespace gridtools
