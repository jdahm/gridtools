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
#include "backend_select.hpp"
#include "gtest/gtest.h"
#include <gridtools/common/generic_metafunctions/gt_remove_qualifiers.hpp>
#include <gridtools/stencil-composition/stencil-composition.hpp>
#include <gridtools/stencil-composition/stencil-functions/stencil-functions.hpp>
#include <test_helper.hpp>

/**
 * Compile-time test to ensure that types are correct in all call stages
 */

using namespace gridtools;
using namespace gridtools::enumtype;
using namespace gridtools::expressions;

namespace {
    // used to ensure that types are correctly passed between function calls (no implicit conversion)
    template <typename tag>
    struct special_type {};

    struct in1_tag {};
    struct in2_tag {};
    struct out_tag {};
} // namespace

class call_stress_types : public testing::Test {
  protected:
    using storage_info_t = gridtools::storage_traits<backend_t::backend_id_t>::storage_info_t<0, 3>;
    using data_store_in1_t =
        gridtools::storage_traits<backend_t::backend_id_t>::data_store_t<special_type<in1_tag>, storage_info_t>;
    using data_store_in2_t =
        gridtools::storage_traits<backend_t::backend_id_t>::data_store_t<special_type<in2_tag>, storage_info_t>;
    using data_store_out_t =
        gridtools::storage_traits<backend_t::backend_id_t>::data_store_t<special_type<out_tag>, storage_info_t>;

    gridtools::grid<gridtools::axis<1>::axis_interval_t> grid;

    data_store_in1_t in1;
    data_store_in2_t in2;
    data_store_out_t out;

    typedef arg<0, data_store_in1_t> p_in1;
    typedef arg<1, data_store_in2_t> p_in2;
    typedef arg<2, data_store_out_t> p_out;

    call_stress_types()
        : grid(make_grid(1, 1, 1)), in1(storage_info_t{1, 1, 1}), in2(storage_info_t{1, 1, 1}),
          out(storage_info_t{1, 1, 1}) {}
};

namespace {
    struct forced_tag {};

    struct simple_callee_with_forced_return_type {
        typedef in_accessor<0> in;
        typedef inout_accessor<1> out;
        typedef boost::mpl::vector<in, out> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            using out_type = typename remove_qualifiers<decltype(eval(out{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<forced_tag>, out_type>{};

            using in1_type = typename remove_qualifiers<decltype(eval(in{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, in1_type>{};
        }
    };

    struct simple_caller_with_forced_return_type {
        typedef in_accessor<0> in;
        typedef inout_accessor<1> out;
        typedef boost::mpl::vector<in, out> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            auto result =
                call<simple_callee_with_forced_return_type>::return_type<special_type<forced_tag>>::with(eval, in{});

            using result_type = decltype(result);
            (void)ASSERT_TYPE_EQ<special_type<forced_tag>, result_type>{};
        }
    };
} // namespace

TEST_F(call_stress_types, simple_force_return_type) {
    auto comp = gridtools::make_computation<backend_t>(grid,
        p_in1{} = in1,
        p_out{} = out,
        gridtools::make_multistage(
            execute<forward>(), gridtools::make_stage<simple_caller_with_forced_return_type>(p_in1(), p_out())));
    comp.run();
}

namespace {
    struct simple_callee_with_deduced_return_type {
        typedef in_accessor<0> in;
        typedef inout_accessor<1> out;
        typedef boost::mpl::vector<in, out> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            using out_type = typename remove_qualifiers<decltype(eval(out{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, out_type>{};

            using in1_type = typename remove_qualifiers<decltype(eval(in{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, in1_type>{};
        }
    };

    struct simple_caller_with_deduced_return_type {
        typedef in_accessor<0> in;
        typedef inout_accessor<1> out;
        typedef boost::mpl::vector<in, out> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            auto result = call<simple_callee_with_deduced_return_type>::with(eval, in{});

            using result_type = decltype(result);
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, result_type>{};
        }
    };
} // namespace

TEST_F(call_stress_types, simple_deduced_return_type) {
    auto comp = gridtools::make_computation<backend_t>(grid,
        p_in1{} = in1,
        p_out{} = out,
        gridtools::make_multistage(
            execute<forward>(), gridtools::make_stage<simple_caller_with_deduced_return_type>(p_in1(), p_out())));
    comp.run();
}

namespace {
    struct local_tag {};

    struct triple_nesting_with_type_switching_third_stage {
        typedef in_accessor<0> in2;
        typedef in_accessor<1> local;
        typedef inout_accessor<2> out;
        typedef in_accessor<3> in1;
        typedef boost::mpl::vector<in2, local, out, in1> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            using out_type = typename remove_qualifiers<decltype(eval(out{}))>::type;
            // the new convention is that the return type (here "out) is deduced from the first argument in the call
            (void)ASSERT_TYPE_EQ<special_type<in2_tag>, out_type>{};

            using in1_type = typename remove_qualifiers<decltype(eval(in1{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, in1_type>{};

            using in2_type = typename remove_qualifiers<decltype(eval(in2{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in2_tag>, in2_type>{};

            using local_type = typename remove_qualifiers<decltype(eval(local{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<local_tag>, local_type>{};
        }
    };

    struct triple_nesting_with_type_switching_second_stage {
        typedef in_accessor<0> in1;
        typedef inout_accessor<1> out;
        typedef in_accessor<2> in2;
        typedef boost::mpl::vector<in1, out, in2> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            using out_type = typename remove_qualifiers<decltype(eval(out{}))>::type;
            // the expected type differs here in "call" vs "call_proc"
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, out_type>{};

            using in1_type = typename remove_qualifiers<decltype(eval(in1{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, in1_type>{};

            using in2_type = typename remove_qualifiers<decltype(eval(in2{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in2_tag>, in2_type>{};

            special_type<local_tag> local{};

            auto result = call<triple_nesting_with_type_switching_third_stage>::with(eval, in2(), local, in1());
            using result_type = decltype(result);
            (void)ASSERT_TYPE_EQ<special_type<in2_tag>, result_type>{};
        }
    };

    struct triple_nesting_with_type_switching_first_stage {
        typedef in_accessor<0> in1;
        typedef inout_accessor<1> out;
        typedef in_accessor<2> in2;
        typedef boost::mpl::vector<in1, out, in2> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            using out_type = typename remove_qualifiers<decltype(eval(out{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<out_tag>, out_type>{};

            using in1_type = typename remove_qualifiers<decltype(eval(in1{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, in1_type>{};

            using in2_type = typename remove_qualifiers<decltype(eval(in2{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in2_tag>, in2_type>{};

            auto result = call<triple_nesting_with_type_switching_second_stage>::with(eval, in1(), in2());
            using result_type = decltype(result);
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, result_type>{};
        }
    };
} // namespace

TEST_F(call_stress_types, triple_nesting_with_type_switching) {
    auto comp = gridtools::make_computation<backend_t>(grid,
        p_in1{} = in1,
        p_in2{} = in2,
        p_out{} = out,
        gridtools::make_multistage(execute<forward>(),
            gridtools::make_stage<triple_nesting_with_type_switching_first_stage>(p_in1(), p_out(), p_in2())));
    comp.run();
}

namespace {
    struct triple_nesting_with_type_switching_and_call_proc_second_stage {
        typedef in_accessor<0> in1;
        typedef inout_accessor<1> out;
        typedef in_accessor<2> in2;
        typedef boost::mpl::vector<in1, out, in2> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            using out_type = typename remove_qualifiers<decltype(eval(out{}))>::type;
            // in contrast to the example where this is stage is called from "call" (not "call_proc")
            // the type here is different!
            (void)ASSERT_TYPE_EQ<special_type<out_tag>, out_type>{};

            using in1_type = typename remove_qualifiers<decltype(eval(in1{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, in1_type>{};

            using in2_type = typename remove_qualifiers<decltype(eval(in2{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in2_tag>, in2_type>{};

            special_type<local_tag> local{};

            auto result = call<triple_nesting_with_type_switching_third_stage>::with(eval, in2(), local, in1());
            using result_type = decltype(result);
            (void)ASSERT_TYPE_EQ<special_type<in2_tag>, result_type>{};
        }
    };

    struct triple_nesting_with_type_switching_and_call_proc_first_stage {
        typedef in_accessor<0> in1;
        typedef inout_accessor<1> out;
        typedef in_accessor<2> in2;
        typedef boost::mpl::vector<in1, out, in2> arg_list;

        template <typename Evaluation>
        GT_FUNCTION static void Do(Evaluation &eval) {
            using out_type = typename remove_qualifiers<decltype(eval(out{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<out_tag>, out_type>{};

            using in1_type = typename remove_qualifiers<decltype(eval(in1{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in1_tag>, in1_type>{};

            using in2_type = typename remove_qualifiers<decltype(eval(in2{}))>::type;
            (void)ASSERT_TYPE_EQ<special_type<in2_tag>, in2_type>{};

            call_proc<triple_nesting_with_type_switching_and_call_proc_second_stage>::with(eval, in1(), out(), in2());
        }
    };
} // namespace

TEST_F(call_stress_types, triple_nesting_with_type_switching_and_call_proc) {
    auto comp = gridtools::make_computation<backend_t>(grid,
        p_in1{} = in1,
        p_in2{} = in2,
        p_out{} = out,
        gridtools::make_multistage(execute<forward>(),
            gridtools::make_stage<triple_nesting_with_type_switching_and_call_proc_first_stage>(
                p_in1(), p_out(), p_in2())));
    comp.run();
}