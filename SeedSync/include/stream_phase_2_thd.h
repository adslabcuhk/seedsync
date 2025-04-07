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

#include "phase_sender.h"
#include "messageQueue.h"
#include "sync_configure.h"
#include "configure.h"
#include "absDatabase.h"
#include "cryptoPrimitive.h"

#include "../build/src/Enclave/storeEnclave_u.h"

extern SyncConfigure sync_config;
extern Configure config;

class StreamPhase2Thd {
    private:
        string my_name_ = "StreamPhase2Thd";

        // for sending
        // SendThd* send_thd_obj_;
        PhaseSender* phase_sender_obj_;

        // for Ecall
        sgx_enclave_id_t sgx_eid_;

        // for input MQ
        MessageQueue<StreamPhase1MQ_t>* inputMQ_;

        // for out query
        AbsDatabase* out_chunk_db_;
        OutChunkQuery_t out_chunk_query_;

        // for ocall read
        ReqContainer_t batch_req_containers_;
        Container_t req_container_;
        SendMsgBuffer_t send_batch_buf_;
        uint64_t offset_of_remaining_process_buf_;
        SendMsgBuffer_t process_batch_buf_;
        uint32_t sent_offset_;

        // for debug
        OutChunkQuery_t debug_out_query_;

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

    public:
        double _phase2_process_time = 0;

        /**
         * @brief Construct a new Stream Phase 2 Thd object
         * 
         * @param phase_sender_obj 
         * @param inputMQ 
         * @param out_fp_db 
         * @param eid_sgx 
         */
        StreamPhase2Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase1MQ_t>* inputMQ,
            AbsDatabase* out_fp_db, sgx_enclave_id_t eid_sgx);

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