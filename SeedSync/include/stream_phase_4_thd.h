/**
 * @file stream_phase_4_thd.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STREAM_PHASE_4_THD_H
#define STREAM_PHASE_4_THD_H

#include "phase_sender.h"
#include "messageQueue.h"
#include "sync_configure.h"
#include "configure.h"
#include "absDatabase.h"
#include "cryptoPrimitive.h"

#include "../build/src/Enclave/storeEnclave_u.h"

extern SyncConfigure sync_config;
extern Configure config;

class StreamPhase4Thd {
    private:
        string my_name_ = "StreamPhase4Thd";

        // for sending
        // SendThd* send_thd_obj_;
        PhaseSender* phase_sender_obj_;
        
        // for Ecall
        sgx_enclave_id_t sgx_eid_;

        // for input MQ
        MessageQueue<StreamPhase3MQ_t>* inputMQ_;
        
        SendMsgBuffer_t send_batch_buf_;

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
        double _phase4_process_time = 0;

        /**
         * @brief Construct a new Stream Phase 4 Thd object
         * 
         * @param phase_sender_obj 
         * @param inputMQ 
         * @param eid_sgx 
         */
        StreamPhase4Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase3MQ_t>* inputMQ,
            sgx_enclave_id_t eid_sgx);

        /**
         * @brief Destroy the Stream Phase 4 Thd object
         * 
         */
        ~StreamPhase4Thd();

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