/**
 * @file ecallStreamEncode.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-06-08
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ENCLAVE_ECALLSTREAMENCODE_H
#define ENCLAVE_ECALLSTREAMENCODE_H

#include "commonEnclave.h"
#include "ecallEnc.h"
// #include "../../../include/lruCache.h"
#include "xdelta3.h"
#include "ecallLz4.h"

class EcallCrypto;

class EcallStreamEncode {
    private:
        string my_name_ = "EcallStreamEncode";

        // for delta encoding
        int delta_flag_ = XD3_NOCOMPRESS;

        // for crypto
        EcallCrypto* crypto_util_;
        EVP_CIPHER_CTX* cipher_ctx_;
        EVP_MD_CTX* md_ctx_;
        uint8_t iv_[CRYPTO_BLOCK_SIZE];

        uint8_t session_key_[CHUNK_HASH_SIZE];

        // dec buffer
        uint8_t* plain_in_buf_;
        uint32_t out_offset_;

        std::vector<SyncRecipeEntry_t> local_addr_list_;

        /**
         * @brief perform delta encoding
         * 
         * @param base_chunk 
         * @param base_size 
         * @param input_chunk 
         * @param input_size 
         * @param delta_chunk 
         * @param delta_size 
         * @return true 
         * @return false 
         */
        bool DeltaEncode(uint8_t* base_chunk, uint32_t base_size,
            uint8_t* input_chunk, uint32_t input_size, uint8_t* delta_chunk,
            uint32_t& delta_size, uint8_t& chunk_type);

        /**
         * @brief process a similar chunk
         * 
         * @param base_chunk 
         * @param base_size 
         * @param input_chunk 
         * @param input_size 
         */
        void PocessSimChunk(uint8_t* base_chunk_data, uint32_t base_size,
            uint8_t* base_hash, uint8_t* sim_chunk_data, uint32_t sim_size);

        /**
         * @brief process a non-similar chunk
         * 
         * @param input_chunk 
         * @param input_size 
         */
        void ProcessNonSimChunk(uint8_t* input_chunk, uint32_t input_size);
    
    public:
        // for logs
        uint64_t _total_similar_num;
        uint64_t _total_similar_chunk_size;
        uint64_t _total_base_chunk_size;
        uint64_t _total_delta_size;
        uint64_t _total_comp_delta_size;

        /**
         * @brief Construct a new Ecall Stream Encode object
         * 
         */
        EcallStreamEncode();

        /**
         * @brief Destroy the Ecall Stream Encode object
         * 
         */
        ~EcallStreamEncode();

        /**
         * @brief process a batch of data
         * 
         * @param in_buf 
         * @param in_size 
         * @param req_container 
         * @param addr_query 
         * @param out_buf 
         * @param out_size 
         */
        void ProcessBatch(uint8_t* in_buf, uint32_t in_size, 
            ReqContainer_t* req_container, OutChunkQuery_t* addr_query, 
            uint8_t* out_buf, uint32_t* out_size);

};

#endif