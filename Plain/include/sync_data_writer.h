/**
 * @file sync_data_writer.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-03-13
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SYNC_DATA_WRITER_H
#define SYNC_DATA_WRITER_H

#include "configure.h"
#include "sync_configure.h"
#include "chunkStructure.h"
#include "send_thd.h"
#include "absDatabase.h"
#include "sync_io.h"
#include "finesse_util.h"
#include "lz4.h"
#include "cryptoPrimitive.h"
#include "xdelta3.h"
// #include "../build/src/Enclave/storeEnclave_u.h"

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

class SyncDataWriter {
    private:
        string my_name_ = "SyncDataWriter";

        uint8_t* plain_in_buf_;


        // local batch fp index
        unordered_map<string, BatchAddrValue_t> batch_fp_index_;

        // delta chunk pos in plain_in_buf (pending for decoding)
        // std::vector<BatchAddrValue_t> pending_delta_list_;
        std::queue<DeltaBatchAddrValue_t> pending_delta_list_;

        std::vector<EnclaveRecipeEntry_t> base_addr_list_;


        // for feature generation
        FinesseUtil* finesse_util_;
        RabinCtx_t rabin_ctx_;  
        RabinFPUtil* rabin_util_;

        CryptoPrimitive* crypto_util_;
        uint8_t iv_[CRYPTO_BLOCK_SIZE];

        // for delta decode
        int delta_flag_ = XD3_NOCOMPRESS;

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
    
    public:
        uint64_t _total_write_data_size = 0;
        uint64_t _total_write_chunk_num = 0;
                // for logs
        uint64_t _total_recv_size = 0;
        uint64_t _total_write_size = 0;
        // breakdowns
        uint64_t _only_comp_size = 0;
        uint64_t _uncomp_delta_size = 0;
        uint64_t _comp_delta_size = 0;
        uint64_t _uncomp_similar_size = 0;
        uint64_t _comp_similar_size = 0;

        double stream6_process_time_ = 0;

        ReqContainer_t req_containers_;

        // for out query (base chunk)
        AbsDatabase* out_fp_db_;
        OutChunkQuery_t out_chunk_query_;

        // for index update
        OutChunkQuery_t update_index_;

        // for debug
        OutChunkQuery_t debug_index_;

        // for container buf
        Container_t container_buf_;
        // Container_t* container_buf_;
        // PtrContainer_t container_buf_;

        SyncIO* sync_io_;

        /**
         * @brief Construct a new Sync Data Writer object
         * 
         * @param sgx_eid 
         * @param out_fp_db 
         */
        SyncDataWriter(SyncIO* sync_io, AbsDatabase* out_fp_db);
        
        /**
         * @brief Destroy the Sync Data Writer object
         * 
         */
        ~SyncDataWriter();

        /**
         * @brief process a batch of recv data
         * 
         * @param recv_buf 
         * @param recv_size 
         */
        void ProcessOneBatch(uint8_t* recv_buf, uint32_t recv_size);

        /**
         * @brief process the tail batch
         * 
         */
        void ProcessTailBatch();

        /**
         * @brief process plain batch
         * 
         * @param recv_buf 
         * @param recv_size 
         * @param req_container 
         * @param base_addr_query 
         * @param container_buf 
         * @param update_index 
         */
        void ProcessPlainBatch(uint8_t* recv_buf, uint32_t recv_size,
            ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
            Container_t* container_buf, OutChunkQuery_t* update_index,
            OutChunkQuery_t* debug_check_quary);
};


#endif