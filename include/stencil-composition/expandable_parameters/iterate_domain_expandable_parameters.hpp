#pragma once
#include "../iterate_domain.hpp"

/** @file iterate_domain for expandable parameters*/

namespace gridtools {

    template < typename T >
    struct is_iterate_domain;

    /**
       @brief iterate_domain specific for when expandable parameters are used

       In expandable parameter computations the user function is repeated a specific amount of time in
       each stencil. The parameters are stored in a storage list, and consecutive elements of the list
       are accessed in each user function.
       This struct "decorates" the base iterate_domain instance with a static const integer ID, which
       records the current position in the storage list, and reimplements the operator() in order to
       access the storage list at the correct offset.

       \tparam IterateDomain base iterate_domain class. Might be e.g. iterate_domain_host or iterate_domain_cuda
       \tparam Position the current position in the expandable parameters list
     */
    template < typename IterateDomain, ushort_t Position >
    struct iterate_domain_expandable_parameters : public IterateDomain {

        GRIDTOOLS_STATIC_ASSERT(is_iterate_domain< IterateDomain >::value, "wrong type");
        static const ushort_t ID = Position - 1;
        typedef IterateDomain super;
        typedef IterateDomain iterate_domain_t;

#ifdef CXX11_ENABLED
        // user protections
        template < typename... T >
        GT_FUNCTION iterate_domain_expandable_parameters(T const &... other_)
            : super(other_...) {
            GRIDTOOLS_STATIC_ASSERT((sizeof...(T) == 1), "The eval() is called with the wrong arguments");
        }
#endif

        template < typename T, ushort_t Val >
        GT_FUNCTION iterate_domain_expandable_parameters(iterate_domain_expandable_parameters< T, Val > const &other_)
            : super(other_) {
            GRIDTOOLS_STATIC_ASSERT((sizeof(T)),
                "The \'eval\' argument to the Do() method gets copied somewhere! You have to pass it by reference.");
        }

        using super::operator();

        /**
       @brief set the offset in the storage_list and forward to the base class

       when the vector_accessor is passed to the iterate_domain we know we are accessing an
       expandable parameters list. Accepts rvalue arguments (accessors constructed in-place)

       \param arg the vector accessor
     */
        // rvalue
        template < uint_t ACC_ID, enumtype::intend Intent, typename Extent, uint_t Size >
        GT_FUNCTION typename super::iterate_domain_t::template accessor_return_type<
            accessor< ACC_ID, Intent, Extent, Size > >::type
        operator()(vector_accessor< ACC_ID, Intent, Extent, Size > const &arg) const {
            typedef typename super::template accessor_return_type< accessor< ACC_ID, Intent, Extent, Size > >::type
                return_t;
// check that if the storage is written the accessor is inout

#ifdef CUDA8
            GRIDTOOLS_STATIC_ASSERT(is_extent< Extent >::value, "wrong type");
            const typename alias< accessor< ACC_ID, Intent, Extent, Size >, dimension< Size - 1 > >::template set< ID >
                tmp_(arg.offsets());
#else
            accessor< ACC_ID, Intent, Extent, Size > tmp_(arg);
            tmp_.template set< 1 >(ID);
#endif
            return super::operator()(tmp_);
        }


        /** @brief method called in the Do methods of the functors. */
        template < typename... Arguments, template < typename... Args > class Expression >
        GT_FUNCTION auto operator()(Expression< Arguments... > const &arg) const
            -> decltype(evaluation::value(*this, arg)) {
            // arg.to_string();
            GRIDTOOLS_STATIC_ASSERT((is_expr< Expression< Arguments... > >::value), "invalid expression");
            return evaluation::value((*this), arg);
        }

        /** @brief method called in the Do methods of the functors.
            partial specializations for double (or float)*/
        template < typename Argument,
            template < typename Arg1, typename Arg2 > class Expression,
            typename FloatType,
            typename boost::enable_if< typename boost::is_floating_point< FloatType >::type, int >::type = 0 >
        GT_FUNCTION auto operator()(Expression< Argument, FloatType > const &arg) const
            -> decltype(evaluation::value_scalar(*this, arg)) {
            GRIDTOOLS_STATIC_ASSERT((is_expr< Expression< Argument, FloatType > >::value), "invalid expression");
            return evaluation::value_scalar((*this), arg);
        }

        /** @brief method called in the Do methods of the functors.
            partial specializations for int. Here we do not use the typedef int_t, because otherwise the interface would
           be polluted with casting
            (the user would have to cast all the numbers (-1, 0, 1, 2 .... ) to int_t before using them in the
           expression)*/
        template < typename Argument,
            template < typename Arg1, typename Arg2 > class Expression,
            typename IntType,
            typename boost::enable_if< typename boost::is_integral< IntType >::type, int >::type = 0 >
        GT_FUNCTION auto operator()(Expression< Argument, IntType > const &arg) const
            -> decltype(evaluation::value_int((*this), arg)) {

            GRIDTOOLS_STATIC_ASSERT((is_expr< Expression< Argument, IntType > >::value), "invalid expression");
            return evaluation::value_int((*this), arg);
        }

        template < typename Argument, template < typename Arg1, int Arg2 > class Expression, int exponent >
        GT_FUNCTION auto operator()(Expression< Argument, exponent > const &arg) const
            -> decltype(evaluation::value_int((*this), arg)) {

            GRIDTOOLS_STATIC_ASSERT((is_expr< Expression< Argument, exponent > >::value), "invalid expression");
            return evaluation::value_int((*this), arg);
        }

    };

    template < typename T >
    struct is_iterate_domain_expandable_parameters : boost::mpl::false_ {};

    template < typename T, ushort_t Val >
    struct is_iterate_domain_expandable_parameters< iterate_domain_expandable_parameters< T, Val > >
        : boost::mpl::true_ {};

    template < typename T, ushort_t Val >
    struct is_iterate_domain< iterate_domain_expandable_parameters< T, Val > > : boost::mpl::true_ {};
}
