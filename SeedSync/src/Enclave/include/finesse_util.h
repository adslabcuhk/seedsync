/**
 * @file finesse_util.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-02-03
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ECALL_FINESSE_UTIL_H
#define ECALL_FINESSE_UTIL_H

// #include "../define.h"
// #include "../configure.h"
#include "rabin_poly.h"
#include "xxhash64.h"
#include "string.h"
// #include "../../../include/chunkStructure.h"
// #include "../../../include/constVar.h"

#include "commonEnclave.h"

using namespace std;

// extern Configure config;

class EcallFinesseUtil {
    private:
        string my_name_ = "EcallFinesseUtil";

        // configure
        uint64_t super_feature_per_chunk_;
        uint64_t feature_per_chunk_;
        uint64_t feature_per_super_feature_;

        RabinFPUtil* rabin_util_;

    public:
        /**
         * @brief Construct a new FinesseUtil object
         * 
         * @param super_feature_per_chunk super feature per chunk 
         * @param feature_per_chunk feature per chunk 
         * @param feature_per_super_feature feature per super feature
         */
        EcallFinesseUtil(uint64_t super_feature_per_chunk, uint64_t feature_per_chunk,
            uint64_t feature_per_super_feature);

        /**
         * @brief Destroy the FinesseUtil object
         * 
         */
        ~EcallFinesseUtil();

        /**
         * @brief compute the features from the chunk
         *
         * @param ctx the rabin ctx
         * @param data the chunk data
         * @param size the chunk size
         * @param features the chunk features
         */
        void ExtractFeature(RabinCtx_t& ctx, uint8_t* data, uint32_t size,
            uint64_t* features);
};

#endif