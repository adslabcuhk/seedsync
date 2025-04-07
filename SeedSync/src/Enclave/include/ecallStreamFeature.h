/**
 * @file ecallStreamFeature.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-06-05
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ECALL_STREAM_FEATURE_H
#define ECALL_STREAM_FEATURE_H

#include "commonEnclave.h"
#include "ecallEnc.h"
#include "localityCache.h"

class EcallCrypto;

class EcallStreamFeature {
    private:
        string my_name_ = "EcallStreamFeature";

        // for crypto
        EcallCrypto* crypto_util_;
        EVP_CIPHER_CTX* cipher_ctx_;
        EVP_MD_CTX* md_ctx_;
        // uint8_t iv_[CRYPTO_BLOCK_SIZE];

        // // for locality cache  
        // LocalityCache* enclave_locality_cache_;

        // dec buffer
        uint8_t* plain_unifp_list_;

        uint8_t session_key_[CHUNK_HASH_SIZE];

        std::vector<EnclaveRecipeEntry_t> local_addr_list_;

    public:
        // for logs
        uint64_t _total_unique_num = 0;
        uint64_t _total_unique_size = 0;

        /**
         * @brief Construct a new Ecall Stream Feature object
         * 
         */
        EcallStreamFeature();

        /**
         * @brief Destroy the Ecall Stream Feature object
         * 
         */
        ~EcallStreamFeature();

        /**
         * @brief process the batch of feature req
         * 
         * @param unifp_list 
         * @param unifp_num 
         * @param req_container 
         * @param feature_list 
         * @param feature_num 
         */
        void ProcessBatch(uint8_t* unifp_list, uint32_t unifp_num,
            ReqContainer_t* req_container, OutChunkQuery_t* addr_query,
            uint8_t* feature_list, size_t feature_num);

};

#endif