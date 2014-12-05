
#pragma once
#include <boost/fusion/include/size.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/print.hpp>
#include "expressions.h"
#ifndef CXX11_ENABLED
#include <boost/typeof/typeof.hpp>
#endif

/**@file
   @brief file handling the access to the storage.
   This file implements some of the innermost data access operations of the library and thus it must be highly optimized.
   The naming convention used below distinguishes from the following two concepts:

   - a parameter: is a non-space dimension (e.g. time) such that derivatives are taken in the equations along this dimension.
   - a dimension: with an abuse of notation will denote any physical scalar field contained in the given storage, e.g. a velocity component, the pressure, or the energy. I.e. it is an extra dimension which can appear in the equations only derived in space or with respect to the parameter mentioned above.
   - a storage: is an instance of the storage class, and can contain one or more fields and dimensions. Every dimension consists of one or several snaphsots of the scalar fields
   (e.g. if the time T is the current dimension, 3 snapshots can be the fields at t, t+1, t+2)
   - a data snapshot: is a pointer to one single snapshot. The snapshots are arranged in the storages on a 1D array, regardless of the dimension and snapshot they refer to. The arg_type (or arg_decorator) class is
   responsible of computing the correct offests (relative to the given dimension) and address the storages correctly.

   The access to the storage is performed in the following steps:
   - the addresses of the first element of all the data fields in the storages involved in this stencil are saved in an array (m_storage_pointers)
   - the index of the storages is saved in another array (m_index)
   - when the functor gets called, the 'offsets' become visible (in the perfect worls they could possibly be known at compile time). In particular the index is moved to point to the correct address, and the correct data snapshot is selected.

   Graphical (ASCII) illustration:

   \verbatim
   ################## Storage #################
   #                ___________\              #
   #                  width    /              #
   #              | |*|*|*|*|*|*|    dim_1    #
   #   dimensions | |*|*|*|          dim_2    #
   #              v |*|*|*|*|*|      dim_3    #
   #	                       		      #
   #                 ^ ^ ^ ^ ^ ^	      #
   #                 | | | | | |	      #
   #                 snapshots		      #
   #					      #
   ################## Storage #################
   \endverbatim

*/

namespace gridtools {

    namespace iterate_domain_aux {

        /**@brief static function incrementing the iterator with the stride on the vertical direction*/
        template<uint_t ID>
        struct increment_k {

            template<typename LocalArgs>
            GT_FUNCTION
            static void apply(LocalArgs& local_args, uint_t factor, uint_t* index) {
		// k direction does does not have bolcks
		boost::fusion::at_c<ID>(local_args)->template increment<2>(factor, (uint_t)0, &index[ID]);
                increment_k<ID-1>::apply(local_args,  factor, index);
            }
        };

        /**@brief specialization to stop the recursion*/
        template<>
        struct increment_k<0> {
            template<typename LocalArgs>
            GT_FUNCTION
            static void apply(LocalArgs& local_args, uint_t factor, uint_t* index) {
                boost::fusion::at_c<0>(local_args)->template increment<2>(factor, (uint_t)0, index);
            }
        };

        /**@brief static function decrementin the iterator with the stride on the vertical direction*/
        template<uint_t ID>
        struct decrement_k {
            template<typename LocalArgs>
            GT_FUNCTION
            static void apply(LocalArgs& local_args, uint_t factor, uint_t* index) {
                boost::fusion::at_c<ID>(local_args)->template decrement<2>(factor, (uint_t)0, &index[ID]);
                decrement_k<ID-1>::apply(local_args, factor, index);
            }
        };

        /**@brief specialization to stop the recursion*/
        template<>
        struct decrement_k<0> {
            template<typename LocalArgs>
            GT_FUNCTION
            static void apply(LocalArgs& local_args, uint_t factor, uint_t* index) {
                boost::fusion::at_c<0>(local_args)->template decrement<2>(factor, (uint_t)0, index);
            }
        };
    } // namespace iterate_domain_aux

    /**@brief recursively assigning the 'raw' data pointers to the m_data_pointers array.
       It enhances the performances, but principle it could be avoided.
       The 'raw' datas are the one or more data fields contained in each storage class
    */
	template<uint_t Number>
	struct assign_raw_data{
        template<typename Left , typename Right >
	    GT_FUNCTION
	    static void assign(Left* l, Right const* r){
            l[Number]=r[Number].get();
            assign_raw_data<Number-1>::assign(l, r);
        }
	};

	/**@brief stopping the recursion*/
	template<>
	struct assign_raw_data<0>{
        template<typename Left , typename Right >
	    GT_FUNCTION
	    static void assign(Left* l, Right const* r){
            l[0]=r[0].get();
        }
	};

	/**@brief this struct counts the total number of data fields are neceassary for this functor (i.e. number of storage instances times number of fields per storage)
	   TODO code repetition in the _traits class
*/
	template <typename StoragesVector, int_t index>
    struct total_storages{
	    //the index must not exceed the number of storages
	    BOOST_STATIC_ASSERT(index<boost::mpl::size<StoragesVector>::type::value);
	    static const uint_t count=total_storages<StoragesVector, index-1>::count+
            boost::remove_pointer<typename boost::remove_reference<typename boost::mpl::at_c<StoragesVector, index >::type>::type>
            ::type::field_dimensions;
    };

	/**@brief partial specialization to stop the recursion*/
	template <typename StoragesVector>
    struct total_storages<StoragesVector, 0 >{
        static const uint_t count=boost::remove_pointer<typename boost::remove_reference<typename  boost::mpl::at_c<StoragesVector, 0 >::type>::type>::type::field_dimensions;
	};

	namespace{

        /**@brief assigning all the storage pointers to the m_data_pointers array*/
	    template<uint_t ID, uint_t Coordinate>
            struct assign_index{
            template<typename Storage>
            /**@brief does the actual assignment
               This method is responsible of computing the index for the memory access at
               the location (i,j,k). Such index is shared among all the fields contained in the
               same storage class instance, and it is not shared among different storage instances.
            */
            GT_FUNCTION
            static void assign(Storage & r, uint_t id, uint_t block, uint_t* index){
                //if the following fails, the ID is larger than the number of storage types,
		//or the index was not properly initialized to 0,
		//or you know what you are doing (then comment out the assert)
                boost::fusion::at_c<ID>(r)->template increment<Coordinate>(id, block, &index[ID]);
                assign_index<ID-1, Coordinate>::assign(r,id,block,index);
            }
        };

        /**usual specialization to stop the recursion*/
	    template<uint_t Coordinate>
            struct assign_index<0, Coordinate>{
            template<typename Storage>
            GT_FUNCTION
            static void assign( Storage & r, uint_t id, uint_t block, uint_t* index/* , ushort_t* lru */){

                boost::fusion::at_c<0>(r)->template increment<Coordinate>(id, block, &index[0]);
            }
        };



	    /**@brief assigning all the storage pointers to the m_data_pointers array*/
	    template<uint_t ID>
		struct set_index_recur{
		template<typename Storage>
		/**@brief does the actual assignment
		   This method is responsible of computing the index for the memory access at
		   the location (i,j,k). Such index is shared among all the fields contained in the
		   same storage class instance, and it is not shared among different storage instances.
		*/
		GT_FUNCTION
		static void set(Storage & r, uint_t id, uint_t* index){
		    //if the following fails, the ID is larger than the number of storage types,
		    //or the index was not properly initialized to 0,
		    //or you know what you are doing (then comment out the assert)
		    boost::fusion::at_c<ID>(r)->set_index(id, &index[ID]);
		    set_index_recur<ID-1>::set(r,id,index);
		}
	    };

	    /**usual specialization to stop the recursion*/
	    template<>
		struct set_index_recur<0>{
		template<typename Storage>
		GT_FUNCTION
		static void set( Storage & r, uint_t id, uint_t* index/* , ushort_t* lru */){

		    boost::fusion::at_c<0>(r)->set_index(id, &index[0]);
		}
	    };


        /**@brief assigning all the storage pointers to the m_data_pointers array*/
        template<uint_t ID, typename LocalArgTypes>
            struct assign_storage{
            template<typename Left, typename Right>
            GT_FUNCTION
            /**@brief does the actual assignment
               This method is also responsible of computing the index for the memory access at
               the location (i,j,k). Such index is shared among all the fields contained in the
               same storage class instance, and it is not shared among different storage instances.
            */
            static void assign(Left& l, Right & r){
#ifdef CXX11_ENABLED
                typedef typename std::remove_pointer< typename std::remove_reference<decltype(boost::fusion::at_c<ID>(r))>::type>::type storage_type;
#else
                typedef typename boost::remove_pointer< typename boost::remove_reference<BOOST_TYPEOF(boost::fusion::at_c<ID>(r))>::type>::type storage_type;
#endif
                //if the following fails, the ID is larger than the number of storage types
                BOOST_STATIC_ASSERT(ID < boost::mpl::size<LocalArgTypes>::value);
		// std::cout<<"ID is: "<<ID-1<<"n_width is: "<< storage_type::n_width-1 << "current index is "<< total_storages<LocalArgTypes, ID-1>::count <<std::endl;
                assign_raw_data<storage_type::field_dimensions-1>::
                    assign(&l[total_storages<LocalArgTypes, ID-1>::count], boost::fusion::at_c<ID>(r)->fields());
                assign_storage<ID-1, LocalArgTypes>::assign(l,r); //tail recursion
            }
        };

        /**usual specialization to stop the recursion*/
        template<typename LocalArgTypes>
            struct assign_storage<0, LocalArgTypes>{
            template<typename Left, typename Right>
            GT_FUNCTION
            static void assign(Left & l, Right & r){
#ifdef CXX11_ENABLED
                typedef typename std::remove_pointer< typename std::remove_reference<decltype(boost::fusion::at_c<0>(r))>::type>::type storage_type;
#else
                typedef typename boost::remove_pointer< typename boost::remove_reference<BOOST_TYPEOF(boost::fusion::at_c<0>(r))>::type>::type storage_type;
#endif
		// std::cout<<"ID is: "<<0<<"n_width is: "<< storage_type::n_width-1 << "current index is "<< 0 <<std::endl;
                assign_raw_data<storage_type::field_dimensions-1>::
                    assign(&l[0], boost::fusion::at_c<0>(r)->fields());
            }
        };
    }

/**@brief class handling the computation of the */
    template <typename LocalDomain>
    struct iterate_domain {
        typedef typename LocalDomain::local_args_type local_args_type;
        static const uint_t N_STORAGES=boost::mpl::size<local_args_type>::value;
        static const uint_t N_DATA_POINTERS=total_storages< local_args_type
                                                            , boost::mpl::size<typename LocalDomain::mpl_storages>::type::value-1 >::count;
        /**@brief constructor of the iterate_domain struct
           It assigns the storage pointers to the first elements of the data fields (for all the data_fields present in the current evaluation), and the
           indexes to access the data fields (one index per storage instance, so that one index might be shared among several data fileds)
        */
        GT_FUNCTION
        iterate_domain(LocalDomain const& local_domain)
            : local_domain(local_domain)
#ifdef CXX11_ENABLED
            , m_index{0}, m_data_pointer{0}/* , m_lru{0} */
#endif
            {
#ifndef CXX11_ENABLED
		for(uint_t it=0; it<N_STORAGES; ++it)
		    m_index[it]=0;//necessary because the index gets INCREMENTED, not SET
#endif
                //                                                      double**        &storage
                assign_storage< N_STORAGES-1, local_args_type >::assign(m_data_pointer, local_domain.local_args);
            }

        /**@brief method for incrementing the index when moving forward along the k direction */
	GT_FUNCTION
	void set_index(uint_t index)
	    {
                set_index_recur< N_STORAGES-1>::set(local_domain.local_args, index, &m_index[0]);
	    }

        /**@brief method for incrementing the index when moving forward along the k direction */
	template <ushort_t Coordinate>
	GT_FUNCTION
	void increment_ij(uint_t index, uint_t block)
	    {
                assign_index< N_STORAGES-1, Coordinate>::assign(local_domain.local_args, 1, block, &m_index[0]);
	    }

        /**@brief method for incrementing the index when moving forward along the k direction */
	template <ushort_t Coordinate>
	GT_FUNCTION
	void assign_ij(uint_t index, uint_t block)
	    {
                assign_index< N_STORAGES-1, Coordinate>::assign(local_domain.local_args, index, block, &m_index[0]);
	    }

        /**@brief method for incrementing the index when moving forward along the k direction */
        GT_FUNCTION
        void increment() {
            iterate_domain_aux::increment_k<N_STORAGES-1>::apply( local_domain.local_args, 1, &m_index[0]);
        }

        /**@brief method for decrementing the index when moving backward along the k direction*/
        GT_FUNCTION
        void decrement() {
            iterate_domain_aux::decrement_k<N_STORAGES-1>::apply( local_domain.local_args, 1, &m_index[0]);
        }

        /**@brief method to set the first index in k (when iterating backwards or in the k-parallel case this can be different from zero)*/
        GT_FUNCTION
        void set_k_start(uint_t from)
            {
                iterate_domain_aux::increment_k<N_STORAGES-1>::apply(local_domain.local_args, from, &m_index[0]);
            }

        template <typename T>
        GT_FUNCTION
        void info(T const &x) const {
            local_domain.info(x);
        }


        /**@brief returns the value of the memory at the given address, plus the offset specified by the arg placeholder
           \param arg placeholder containing the storage ID and the offsets
           \param storage_pointer pointer to the first element of the specific data field used
         */
        template <typename ArgType, typename StoragePointer>
        GT_FUNCTION
        typename boost::mpl::at<typename LocalDomain::esf_args, typename ArgType::index_type>::type::value_type& get_value(ArgType const& arg , StoragePointer & storage_pointer) const
            {

		assert(boost::fusion::at<typename ArgType::index_type>(local_domain.local_args)->size() >  m_index[ArgType::index_type::value]
                       +(boost::fusion::at<typename ArgType::index_type>(local_domain.local_args))
                       ->offset(arg.i(),arg.j(),arg.k()));

		assert( m_index[ArgType::index_type::value]
		       +(boost::fusion::at<typename ArgType::index_type>(local_domain.local_args))
                       ->offset(arg.i(),arg.j(),arg.k()) >= 0);

                return *(storage_pointer
                         +(m_index[ArgType::index_type::value])
                         +(boost::fusion::at<typename ArgType::index_type>(local_domain.local_args))
                         ->offset(arg.i(),arg.j(),arg.k()));
            }


        /**@brief local class instead of using the inline (cond)?a:b syntax, because in the latter both branches get compiled (generating sometimes a compile-time overflow) */
        template <bool condition, typename LocalD, typename ArgType>
        struct current_storage;

        template < typename LocalD, typename ArgType>
        struct current_storage<true, LocalD, ArgType>{
            static const uint_t value=0;
        };

        template < typename LocalD, typename ArgType>
        struct current_storage<false, LocalD, ArgType>{
            static const uint_t value=(total_storages< typename LocalD::local_args_type, ArgType::index_type::value-1 >::count);
        };

        /** @brief method called in the Do methods of the functors.
            specialization for the arg_type placeholders
         */
        template <uint_t Index, typename Range>
        GT_FUNCTION
        typename boost::mpl::at<typename LocalDomain::esf_args, typename arg_type<Index, Range>::index_type>::type::value_type&
        operator()(arg_type<Index, Range> const& arg) const {

            return get_value(arg, m_data_pointer[current_storage<(arg_type<Index, Range>::index_type::value==0), LocalDomain, arg_type<Index, Range> >::value]);
        }



        /** @brief method called in the Do methods of the functors.
            Specialization for the arg_decorator placeholder (i.e. for extended storages, containg multiple snapshots of data fields with the same dimension and memory layout)*/
        template <typename ArgType>
        GT_FUNCTION
        typename boost::mpl::at<typename LocalDomain::esf_args, typename ArgType::index_type>::type::value_type&
        operator()(gridtools::arg_decorator<ArgType> const& arg) const {

#ifdef CXX11_ENABLED
            typedef typename std::remove_reference<decltype(*boost::fusion::at<typename ArgType::index_type>(local_domain.local_args))>::type storage_type;
#else
            typedef typename boost::remove_reference<BOOST_TYPEOF( (*boost::fusion::at<typename ArgType::index_type>(local_domain.local_args)) )>::type storage_type;
#endif

            //if the following assertion fails you have specified a dimension for the extended storage
            //which does not correspond to the size of the extended placeholder for that storage
            /* BOOST_STATIC_ASSERT(storage_type::n_dimensions==ArgType::n_args); */

	    //for the moment the extra dimensionality of the storage is limited to max 2
	    //(3 space dim + 2 extra= 5, which gives n_args==4)
	    BOOST_STATIC_ASSERT(gridtools::arg_decorator<ArgType>::n_args<=4);
            return get_value(arg, m_data_pointer[storage_type::get_index(
				     (   gridtools::arg_decorator<ArgType>::extra_args <= 1 ? // static if
					 arg.template n<gridtools::arg_decorator<ArgType>::extra_args>() //offset for the current dimension
					 :
					 arg.template n<gridtools::arg_decorator<ArgType>::extra_args>() //offset for the current dimension
					 + arg.template n<gridtools::arg_decorator<ArgType>::extra_args-1>()
					 *storage_type::super::super::n_width //stride of the current dimension inside the vector of storages
					 ))//+ the offset of the other extra dimension
						 + current_storage<(ArgType::index_type::value==0), LocalDomain, ArgType>::value]);
        }

#ifdef CXX11_ENABLED
/**\section binding_expressions (Expressions Bindings)
   @brief these functions get called by the operator () in gridtools::iterate_domain, i.e. in the functor Do method defined at the application level
   They evalueate the operator passed as argument, by recursively evaluating its arguments
   @{
*/
    /** plus evaluation*/
    template <typename ArgType1, typename ArgType2>
    GT_FUNCTION
    auto value(expr_plus<ArgType1, ArgType2> const& arg) const -> decltype((*this)(arg.first_operand) + (*this)(arg.second_operand)) {return (*this)(arg.first_operand) + (*this)(arg.second_operand);}

    /** minus evaluation*/
    template <typename ArgType1, typename ArgType2>
    GT_FUNCTION
    auto value(expr_minus<ArgType1, ArgType2> const& arg) const -> decltype((*this)(arg.first_operand) - (*this)(arg.second_operand)) {return (*this)(arg.first_operand) - (*this)(arg.second_operand);}

    /** multiplication evaluation*/
    template <typename ArgType1, typename ArgType2>
    GT_FUNCTION
    auto value(expr_times<ArgType1, ArgType2> const& arg) const -> decltype((*this)(arg.first_operand) * (*this)(arg.second_operand)) {return (*this)(arg.first_operand) * (*this)(arg.second_operand);}

    /** division evaluation*/
    template <typename ArgType1, typename ArgType2>
    GT_FUNCTION
    auto value(expr_divide<ArgType1, ArgType2> const& arg) const -> decltype((*this)(arg.first_operand) / (*this)(arg.second_operand)) {return (*this)(arg.first_operand) / (*this)(arg.second_operand);}

    // template <typename ArgType1, typename ArgType2>
    // GT_FUNCTION
    // auto value(expr_exp<ArgType1, ArgType2> const& arg) const -> decltype((*this)(arg.first_operand) ^ (*this)(arg.second_operand)) {return (*this)(arg.first_operand) ^ (*this)(arg.second_operand);}

    /**\subsection specialization (Partial Specializations)
       partial specializations for double (or float)
       @{*/
    /** sum with scalar evaluation*/
	template <typename ArgType1, typename FloatType, typename boost::enable_if<typename boost::is_floating_point<FloatType>::type, int >::type=0 >
    GT_FUNCTION
    auto value_scalar(expr_plus<ArgType1, FloatType> const& arg) const -> decltype((*this)(arg.first_operand) + arg.second_operand) {return (*this)(arg.first_operand) + arg.second_operand;}

    /** subtract with scalar evaluation*/
	template <typename ArgType1, typename FloatType, typename boost::enable_if<typename boost::is_floating_point<FloatType>::type, int >::type=0 >
    GT_FUNCTION
    auto value_scalar(expr_minus<ArgType1, FloatType> const& arg) const -> decltype((*this)(arg.first_operand) - arg.second_operand) {return (*this)(arg.first_operand) - arg.second_operand;}

    /** multiply with scalar evaluation*/
	template <typename ArgType1, typename FloatType, typename boost::enable_if<typename boost::is_floating_point<FloatType>::type, int >::type=0 >
    GT_FUNCTION
    auto value_scalar(expr_times<ArgType1, FloatType> const& arg) const -> decltype((*this)(arg.first_operand) * arg.second_operand) {return (*this)(arg.first_operand) * arg.second_operand;}

    /** divide with scalar evaluation*/
	template <typename ArgType1, typename FloatType, typename boost::enable_if<typename boost::is_floating_point<FloatType>::type, int >::type=0 >
    GT_FUNCTION
    auto value_scalar(expr_divide<ArgType1, FloatType> const& arg) const -> decltype((*this)(arg.first_operand) / arg.second_operand) {return (*this)(arg.first_operand) / arg.second_operand;}

    // template <typename ArgType1>
    // GT_FUNCTION
    // float_type value_scalar(expr_exp<ArgType1, float_type> const& arg) {return std::pow((*this)(arg.first_operand), arg.second_operand);}
#endif

    /**
       @}
       \subsection specialization2 (Partial Specializations)
       @brief partial specializations for integer
       Here we do not use the typedef int_t, because otherwise the interface would be polluted with casting
       (the user would have to cast all the numbers (-1, 0, 1, 2 .... ) to int_t before using them in the expression)
       @{*/
    /** integer power evaluation*/
        template <typename ArgType1, typename IntType, typename boost::enable_if<typename boost::is_integral<IntType>::type, int >::type=0 >
        GT_FUNCTION
        auto value_int(expr_exp<ArgType1, IntType> const& arg) const -> decltype(std::pow((*this)(arg.first_operand), arg.second_operand)) {return std::pow((*this)(arg.first_operand), arg.second_operand);}
/**@}@}*/


#ifdef CXX11_ENABLED

/** @brief method called in the Do methods of the functors. */
        template <typename FirstArg, typename SecondArg, template<typename Arg1, typename Arg2> class Expression >
        GT_FUNCTION
        auto operator() (Expression<FirstArg, SecondArg> const& arg) const ->decltype(this->value(arg)) {
	    //arg.to_string();
            return value(arg);
        }

/** @brief method called in the Do methods of the functors.
    partial specializations for double (or float)*/
        template <typename Arg, template<typename Arg1, typename Arg2> class Expression, typename FloatType, typename boost::enable_if<typename boost::is_floating_point<FloatType>::type, int >::type=0 >
        GT_FUNCTION
        auto operator() (Expression<Arg, FloatType> const& arg) const ->decltype(this->value_scalar(arg)) {
            return value_scalar(arg);
        }

/** @brief method called in the Do methods of the functors.
    partial specializations for int. Here we do not use the typedef int_t, because otherwise the interface would be polluted with casting
    (the user would have to cast all the numbers (-1, 0, 1, 2 .... ) to int_t before using them in the expression)*/
        template <typename Arg, template<typename Arg1, typename Arg2> class Expression, typename IntType, typename boost::enable_if<typename boost::is_integral<IntType>::type, int >::type=0 >
        GT_FUNCTION
        auto operator() (Expression<Arg, IntType> const& arg) const ->decltype(this->value_int(arg)) {
            return value_int(arg);
        }
#endif

    private:
        // iterate_domain remembers the state. This is necessary when we do finite differences and don't want to recompute all the iterators (but simply use the ones available for the current iteration storage for all the other storages)
        LocalDomain const& local_domain;
        uint_t m_index[N_STORAGES];

	typedef typename boost::remove_pointer<typename boost::mpl::at_c<typename LocalDomain::mpl_storages, 0>::type>::type::value_type float_t;
        float_t* m_data_pointer[N_DATA_POINTERS];//the storages could have different types(?)

	//It would be nice if the m_data_pointer was a tuple. Performance penalty? nvcc support?
    	// template<typename Tuple1, typename Tuple2>
	// struct concat_tuple{
	//     typedef decltype(std::tuple_cat(std::declval<Tuple1>(), std::declval<Tuple2>())) type;
	// };

	// typedef typename boost::mpl::fold<
	//     typename LocalDomain::mpl_storages,
	//     std::tuple<>,
	//     concat_tuple<boost::mpl::_1, std::tuple<boost::mpl::_2> > >::type tuple_type;
};

} // namespace gridtools
