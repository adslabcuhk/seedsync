/**
 * @file stream_phase_2_thd.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-24
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STREAM_PHASE_2_THD_H
#define STREAM_PHASE_2_THD_H

// #include "send_thd.h"
#include "phase_sender.h"
#include "messageQueue.h"
#include "sync_configure.h"
#include "configure.h"
#include "absDatabase.h"
#include "cryptoPrimitive.h"
#include "locality_cache.h"
#include "sync_io.h"

// #include "../build/src/Enclave/storeEnclave_u.h"

extern SyncConfigure sync_config;
extern Configure config;

class StreamPhase2Thd {
    private:
        string my_name_ = "StreamPhase2Thd";

        // for sending
        // SendThd* send_thd_obj_;
        PhaseSender* phase_sender_obj_;

        // for Ecall
        // sgx_enclave_id_t sgx_eid_;

        // for input MQ
        MessageQueue<StreamPhase1MQ_t>* inputMQ_;

        // for out query
        AbsDatabase* out_chunk_db_;
        OutChunkQuery_t out_chunk_query_;

        // uni chunk fp buffer
        uint8_t* plain_uni_fp_list_;

        // in-enclave cache
        LocalityCache* enclave_locality_cache_;

        SyncIO* sync_io_;

        /**
         * @brief process one batch of fp list
         * 
         */
        void ProcessOneBatch();

        /**
         * @brief send one batch of fp list
         * 
         */
        void SendOneBatch();

        // /**
        //  * @brief insert the batch into send MQ
        //  * 
        //  * @param send_buf 
        //  */
        // void SendBatch(SendMsgBuffer_t& send_buf);

        /**
         * @brief process the plain batch
         * 
         * @param fp_list 
         * @param fp_num 
         * @param uni_fp_list 
         * @param uni_fp_num 
         * @param req_container 
         * @param debug_out_query 
         */
        void ProcessPlainBatch(uint8_t* fp_list, size_t fp_num,
            uint8_t* uni_fp_list, size_t* uni_fp_num, Container_t* req_container,
            OutChunkQuery_t* debug_out_query);

    public:
        double stream2_process_time_ = 0;
        // for logs
        uint64_t _enclave_duplicate_num = 0;
        uint64_t _enclave_unique_num = 0;
        uint64_t _global_duplicate_num = 0;
        uint64_t _global_unique_num = 0;

        // for ocall read
        ReqContainer_t batch_req_containers_;
        Container_t req_container_;
        SendMsgBuffer_t send_batch_buf_;
        SendMsgBuffer_t process_batch_buf_;
        uint32_t sent_offset_;

        // for debug
        OutChunkQuery_t debug_out_query_;

        /**
         * @brief Construct a new Stream Phase 2 Thd object
         * 
         * @param phase_sender_obj 
         * @param inputMQ 
         * @param out_fp_db 
         * @param eid_sgx 
         */
        StreamPhase2Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase1MQ_t>* inputMQ,
            AbsDatabase* out_fp_db, SyncIO* sync_io, LocalityCache* locality_cache);

        /**
         * @brief Destroy the Stream Phase 2 Thd object
         * 
         */
        ~StreamPhase2Thd();

        /**
         * @brief the main process
         * 
         */
        void Run();

        /**
         * @brief Set the Done Flag object
         * 
         */
        void SetDoneFlag();

};

#endif