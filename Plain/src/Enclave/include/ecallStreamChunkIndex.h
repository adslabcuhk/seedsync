/**
 * @file ecallStreamChunkIndex.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-26 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ECALL_STREAM_CHUNK_INDEX_H
#define ECALL_STREAM_CHUNK_INDEX_H

#include "commonEnclave.h"
#include "ecallEnc.h"

#include "../../../include/constVar.h"
#include "../../../include/chunkStructure.h"
#include "localityCache.h"

class EcallCrypto;

class EcallStreamChunkIndex {
    private:
        string my_name_ = "EcallStreamChunkIndex";

        // for crypto
        EcallCrypto* crypto_util_;
        EVP_CIPHER_CTX* cipher_ctx_;
        EVP_MD_CTX* md_ctx_;

        // session key
        uint8_t session_key_[CHUNK_HASH_HMAC_SIZE];

        // for index query key
        uint8_t query_key_[CHUNK_HASH_HMAC_SIZE];

        // uni chunk fp buffer
        uint8_t* plain_uni_fp_list_;

        // in-enclave cache
        LocalityCache* enclave_locality_cache_;

    public:
        // for logs
        uint64_t _enclave_duplicate_num = 0;
        uint64_t _enclave_unique_num = 0;
        uint64_t _global_duplicate_num = 0;
        uint64_t _global_unique_num = 0;

        /**
         * @brief Construct a new Ecall Stream Chunk Index object
         * 
         * @param enclave_locality_cache 
         */
        EcallStreamChunkIndex(LocalityCache* enclave_locality_cache);

        /**
         * @brief Destroy the Ecall Stream Chunk Index object
         * 
         */
        ~EcallStreamChunkIndex();

        /**
         * @brief process a batch of fp list with naive stream cache
         * 
         * @param fp_list 
         * @param fp_num 
         * @param out_chunk_query
         * @param req_container
         */
        void NaiveStreamCache(uint8_t* fp_list, size_t fp_num, uint8_t* uni_fp_list, 
            size_t* uni_fp_num, Container_t* req_container);
        
        // for debugging
        void NaiveStreamCacheDebug(uint8_t* fp_list, size_t fp_num, uint8_t* uni_fp_list, 
            size_t* uni_fp_num, Container_t* req_container, OutChunkQuery_t* debug_out_query);

        /**
         * @brief process a batch of fp list with batching stream cache
         * 
         * @param fp_list 
         * @param fp_num 
         * @param uni_fp_list 
         * @param uni_fp_num 
         * @param addr_query 
         * @param req_container 
         */
        void BatchStreamCache(uint8_t* fp_list, size_t fp_num, uint8_t* uni_fp_list, 
            size_t* uni_fp_num, OutChunkQuery_t* addr_query, ReqContainer_t* req_container);

        
};

#endif