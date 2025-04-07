/**
 * @file stream_phase_5_thd.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STREAM_PHASE_5_THD_H
#define STREAM_PHASE_5_THD_H

// #include "send_thd.h"
#include "phase_sender.h"
#include "messageQueue.h"   
#include "sync_configure.h"
#include "configure.h"
#include "absDatabase.h"
#include "cryptoPrimitive.h"
#include "sync_io.h"
#include "xdelta3.h"
#include "lz4.h"
// #include "../build/src/Enclave/storeEnclave_u.h"

extern SyncConfigure sync_config;
extern Configure config;

class StreamPhase5Thd {
    private:
        string my_name_ = "StreamPhase5Thd";

        // for sending
        // SendThd* send_thd_obj_;
        PhaseSender* phase_sender_obj_;
        
        // for Ecall
        // sgx_enclave_id_t sgx_eid_;

        // for input MQ
        MessageQueue<StreamPhase4MQ_t>* inputMQ_;

        // for out query
        AbsDatabase* out_fp_db_;
        OutChunkQuery_t out_chunk_query_;
        
        SyncIO* sync_io_;

        uint8_t* plain_in_buf_;
        uint32_t out_offset_;

        // for delta encoding
        int delta_flag_ = XD3_NOCOMPRESS;

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

        /**
         * @brief process one batch of uni fp list, return the features
         * 
         */
        void ProcessOneBatch();

        /**
         * @brief insert the batch into send MQ
         * 
         * @param send_buf 
         */
        void SendBatch(SendMsgBuffer_t& send_buf);

        /**
         * @brief process plain batch
         * 
         * @param in_buf 
         * @param in_size 
         * @param req_container 
         * @param addr_query 
         * @param out_buf 
         * @param out_size 
         */
        void ProcessPlainBatch(uint8_t* in_buf, uint32_t in_size, 
            ReqContainer_t* req_container, OutChunkQuery_t* addr_query, 
            uint8_t* out_buf, uint32_t* out_size);

    public:
        double stream5_process_time_ = 0;
        // for logs
        uint64_t _total_similar_num = 0;
        uint64_t _total_similar_chunk_size = 0;
        uint64_t _total_base_chunk_size = 0;
        uint64_t _total_delta_size = 0;
        uint64_t _total_comp_delta_size = 0;        

        uint64_t total_similar_size_ = 0;
        uint64_t total_base_size_ = 0;
        uint64_t total_delta_size_ = 0;

        SendMsgBuffer_t send_batch_buf_;
        ReqContainer_t req_containers_;

        /**
         * @brief Construct a new Stream Phase 5 Thd object
         * 
         * @param phase_sender_obj 
         * @param inputMQ 
         * @param out_fp_db 
         * @param eid_sgx 
         */
        StreamPhase5Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase4MQ_t>* inputMQ, 
            AbsDatabase* out_fp_db, SyncIO* sync_io);

        /**
         * @brief Destroy the Stream Phase 5 Thd object
         * 
         */
        ~StreamPhase5Thd();

        /**
         * @brief the main process
         * 
         */
        void Run();
};

#endif