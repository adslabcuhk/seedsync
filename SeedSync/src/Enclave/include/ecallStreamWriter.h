/**
 * @file ecallStreamWriter.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-06-15
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ECALL_STREAM_WRITER_H
#define ECALL_STREAM_WRITER_H

#include "commonEnclave.h"
#include "ecallEnc.h"
#include "xdelta3.h"
#include "ecallLz4.h"
#include "finesse_util.h"

class EcallCrypto;

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint8_t type;
} BatchAddrValue_t;

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint8_t type;
} DeltaBatchAddrValue_t;

class EcallStreamWriter {
    private:
        string my_name_ = "EcallStreamWriter";

        // for delta decode
        int delta_flag_ = XD3_NOCOMPRESS;

        // for crypto
        EcallCrypto* crypto_util_;
        EVP_CIPHER_CTX* cipher_ctx_;
        EVP_MD_CTX* md_ctx_;
        uint8_t iv_[CRYPTO_BLOCK_SIZE];

        uint8_t session_key_[CHUNK_HASH_SIZE];

        // for feature generation
        EcallFinesseUtil* finesse_util_;
        RabinCtx_t rabin_ctx_;  
        RabinFPUtil* rabin_util_;

        // container buffer
        // Container_t* container_buf_;

        // dec buffer
        uint8_t* plain_in_buf_;

        // local batch fp index
        unordered_map<string, BatchAddrValue_t> batch_fp_index_;

        // delta chunk pos in plain_in_buf (pending for decoding)
        // std::vector<BatchAddrValue_t> pending_delta_list_;
        std::queue<DeltaBatchAddrValue_t> pending_delta_list_;

        std::vector<EnclaveRecipeEntry_t> base_addr_list_;

        // for update index
        // OutChunkQuery_t* update_index_;

        /**
         * @brief delta decoding
         * 
         * @param base_chunk 
         * @param base_size 
         * @param delta_chunk 
         * @param delta_size 
         * @param output_chunk 
         * @return uint32_t 
         */
        uint32_t DeltaDecode(uint8_t* base_chunk, uint32_t base_size,
            uint8_t* delta_chunk, uint32_t delta_size, uint8_t* output_chunk);

        /**
         * @brief extract features for a chunk
         * 
         * @param rabin_ctx 
         * @param data 
         * @param size 
         * @param features 
         */
        void ExtractFeature(RabinCtx_t &rabin_ctx, uint8_t* data, 
            uint32_t size, uint64_t* features);

        /**
         * @brief Pick a new IV
         * 
         * @return uint8_t* 
         */
        inline uint8_t* PickNewIV() {
            uint64_t* tmpPtr = (uint64_t*)iv_;
            *tmpPtr += 1;
            return iv_;
        }

        /**
         * @brief save the recovered enc chunk
         * 
         * @param container_buf 
         * @param chunk_data 
         * @param chunk_size 
         * @param chunk_iv 
         * @param chunk_hash 
         * @param features 
         * @param chunk_addr 
         */
        void SaveChunk(Container_t* container_buf, uint8_t* chunk_data, 
            uint32_t chunk_size, uint8_t* chunk_iv, uint8_t* chunk_hash, 
            uint64_t* features, RecipeEntry_t* chunk_addr);

        /**
         * @brief process one recovered chunk
         * 
         * @param container_buf 
         * @param chunk_data 
         * @param chunk_size 
         * @param chunk_addr 
         */
        void ProcessOneChunk(Container_t* container_buf, uint8_t* chunk_data, 
            uint32_t chunk_size, OutChunkQueryEntry_t* chunk_addr);
        
        // for debugging
        void ProcessOneChunk(Container_t* container_buf, uint8_t* chunk_data, 
            uint32_t chunk_size, OutChunkQueryEntry_t* chunk_addr, 
            OutChunkQueryEntry_t* debug_check_entry);


        // void ProcessCompOnlyChunk()

    public:
        // for logs
        uint64_t _total_recv_size = 0;
        uint64_t _total_write_size = 0;
        // breakdowns
        uint64_t _only_comp_size = 0;
        uint64_t _uncomp_delta_size = 0;
        uint64_t _comp_delta_size = 0;
        uint64_t _uncomp_similar_size = 0;
        uint64_t _comp_similar_size = 0;
        // uint64_t _only_delta_size = 0;
        // uint64_t _delta_comp_size = 0;

        // /**
        //  * @brief Construct a new Ecall Stream Writer object
        //  * 
        //  * @param container_buf 
        //  */
        // EcallStreamWriter(Container_t* container_buf, OutChunkQuery_t* update_index);

        /**
         * @brief Construct a new Ecall Stream Writer object
         * 
         */
        EcallStreamWriter();

        /**
         * @brief Destroy the Ecall Stream Writer object
         * 
         */
        ~EcallStreamWriter();

        /**
         * @brief process a batch of recv data
         * 
         * @param recv_buf 
         * @param recv_size 
         * @param req_container 
         * @param base_addr_query
         */
        void ProcessBatch(uint8_t* recv_buf, uint32_t recv_size, 
            ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
            Container_t* container_buf, OutChunkQuery_t* update_index);

        /**
         * @brief process a batch of recv data
         * 
         * @param recv_buf 
         * @param recv_size 
         * @param req_container 
         * @param base_addr_query
         */
        void ProcessBatch(uint8_t* recv_buf, uint32_t recv_size, 
            ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
            Container_t* container_buf, OutChunkQuery_t* update_index, 
            OutChunkQuery_t* debug_check_quary);
};

#endif