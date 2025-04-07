/**
 * @file stream_phase_3_thd.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-37
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STREAM_PHASE_3_THD_H
#define STREAM_PHASE_3_THD_H

// #include "send_thd.h"
#include "phase_sender.h"
#include "messageQueue.h"
#include "sync_configure.h"
#include "configure.h"
#include "absDatabase.h"
#include "cryptoPrimitive.h"
#include "sync_io.h"
// #include "../build/src/Enclave/storeEnclave_u.h"

extern SyncConfigure sync_config;
extern Configure config;

class StreamPhase3Thd {
    private:
        string my_name_ = "StreamPhase3Thd";

        // for sending
        // SendThd* send_thd_obj_;
        PhaseSender* phase_sender_obj_;
        
        // for Ecall
        // sgx_enclave_id_t sgx_eid_;

        // for input MQ
        MessageQueue<StreamPhase2MQ_t>* inputMQ_;

        // for out query
        // the global chunk index should include chunk feature in value
        AbsDatabase* out_chunk_db_;
        OutChunkQuery_t out_chunk_query_;

        uint8_t* plain_unifp_list_;

        SyncIO* sync_io_;

        std::vector<EnclaveRecipeEntry_t> local_addr_list_;

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
         * @param unifp_list 
         * @param unifp_num 
         * @param req_container 
         * @param addr_query 
         * @param feature_list 
         * @param feature_num 
         */
        void ProcessPlainBatch(uint8_t* unifp_list, uint32_t unifp_num,
            ReqContainer_t* req_container, OutChunkQuery_t* addr_query,
            uint8_t* feature_list, size_t feature_num);

    public:
        double stream3_process_time_ = 0;
        uint64_t _total_unique_num = 0;
        uint64_t _total_unique_size = 0;

        SendMsgBuffer_t send_batch_buf_;
        ReqContainer_t req_containers_;
        uint8_t* recv_buf_;
        uint32_t recv_num_;
        uint32_t recv_offset_;

        /**
         * @brief Construct a new Stream Phase 3 Thd object
         * 
         * @param phase_sender_obj 
         * @param inputMQ 
         * @param out_fp_db 
         * @param eid_sgx 
         */
        StreamPhase3Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase2MQ_t>* inputMQ,
            AbsDatabase* out_fp_db, SyncIO* sync_io);

        /**
         * @brief Destroy the Stream Phase 3 Thd object
         * 
         */
        ~StreamPhase3Thd();

        /**
         * @brief the main process
         * 
         */
        void Run();
};

#endif