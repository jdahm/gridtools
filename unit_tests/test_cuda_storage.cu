/*
 * File:   test_domain.cpp
 * Author: mbianco
 *
 * Created on February 14, 2014, 4:18 PM
 *
 * Test cuda_storage features
 */

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <common/gpu_clone.h>
#include <storage/hybrid_pointer.h>
#include <stencil-composition/backend.h>
#include <common/layout_map.h>
#include <common/defs.h>

#ifdef __CUDACC__
template <typename T>
__global__
void add_on_gpu(T * ptr, uint_t d1, uint_t d2, uint_t d3) {
    for (uint_t i = 0; i < d1; ++i) {
        for (uint_t j = 0; j < d2; ++j) {
            for (uint_t k = 0; k < d3; ++k) {
                (*ptr)(i,j,k) = -i-j-k;
            }
        }
    }
}
#endif

using namespace gridtools;
using namespace enumtype;
bool test_cuda_storage() {

    typedef gridtools::backend<Cuda, Naive>::storage_type<float_type, gridtools::layout_map<0,1,2> > ::type storage_type;

    uint_t d1 = 3;
    uint_t d2 = 3;
    uint_t d3 = 3;

    storage_type data(d1,d2,d3,-1, ("data"));

    for (uint_t i = 0; i < d1; ++i) {
        for (uint_t j = 0; j < d2; ++j) {
            for (uint_t k = 0; k < d3; ++k) {
                data(i,j,k) = i+j+k;
#ifndef NDEBUG
                std::cout << data(i,j,k) << " ";
#endif
            }
#ifndef NDEBUG
            std::cout << std::endl;
#endif
        }
#ifndef NDEBUG
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }

    data.h2d_update();
    data.clone_to_gpu();

#ifdef __CUDACC__
    add_on_gpu<<<1,1>>>(data.gpu_object_ptr, d1, d2, d3);
    cudaDeviceSynchronize();
#endif
    data.d2h_update();

    bool same = true;
    for (uint_t i = 0; i < d1; ++i) {
        for (uint_t j = 0; j < d2; ++j) {
            for (uint_t k = 0; k < d3; ++k) {
#ifndef NDEBUG
                std::cout << data(i,j,k) << " ";
#endif
                if (data(i,j,k) != -i-j-k)
                    same = false;
            }
#ifndef NDEBUG
            std::cout << std::endl;
#endif
        }
#ifndef NDEBUG
        std::cout << std::endl;
        std::cout << std::endl;
#endif
    }

    return same;
}
