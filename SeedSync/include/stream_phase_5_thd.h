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

#include "phase_sender.h"
#include "messageQueue.h"   
#include "sync_configure.h"
#include "configure.h"
#include "absDatabase.h"
#include "cryptoPrimitive.h"
#include <future>

#include "../build/src/Enclave/storeEnclave_u.h"

extern SyncConfigure sync_config;
extern Configure config;

class StreamPhase5Thd {
    private:
        string my_name_ = "StreamPhase5Thd";

        // for sending
        // SendThd* send_thd_obj_;
        PhaseSender* phase_sender_obj_;
        
        // for Ecall
        sgx_enclave_id_t sgx_eid_;

        // for input MQ
        MessageQueue<StreamPhase4MQ_t>* inputMQ_;

        // for out query
        AbsDatabase* out_fp_db_;
        OutChunkQuery_t out_chunk_query_;

        uint64_t total_similar_size_ = 0;
        uint64_t total_base_size_ = 0;
        uint64_t total_delta_size_ = 0;

        SendMsgBuffer_t recv_batch_buf_;
        SendMsgBuffer_t send_batch_buf_;
        ReqContainer_t req_containers_;

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

    public:
        double _phase5_process_time = 0;

        /**
         * @brief Construct a new Stream Phase 5 Thd object
         * 
         * @param phase_sender_obj 
         * @param inputMQ 
         * @param out_fp_db 
         * @param eid_sgx 
         */
        StreamPhase5Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase4MQ_t>* inputMQ, 
            AbsDatabase* out_fp_db, sgx_enclave_id_t eid_sgx);

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