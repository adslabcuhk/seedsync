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

class StreamPhase4Thd {
    private:
        string my_name_ = "StreamPhase4Thd";

        // for sending
        // SendThd* send_thd_obj_;
        PhaseSender* phase_sender_obj_;
        
        // for Ecall
        // sgx_enclave_id_t sgx_eid_;

        // for input MQ
        MessageQueue<StreamPhase3MQ_t>* inputMQ_;

        // in-enclave cache
        LocalityCache* enclave_locality_cache_;

        SyncIO* sync_io_;

        // dec buf
        uint8_t* plain_feature_fp_list_;

        // enc buf
        uint8_t* plain_out_list_;

        unordered_map<uint64_t, string> batch_feature_index_;


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
         * @param feature_fp_list 
         * @param feature_num 
         * @param out_list 
         * @param out_num 
         */
        void ProcessPlainBatch(uint8_t* feature_fp_list, size_t feature_num, 
            uint8_t* out_list, size_t out_num);

        /**
         * @brief detect similar chunk within the local batch
         * 
         * @param features 
         * @param chunkHash 
         * @param baseHash 
         * @return true 
         * @return false 
         */
        bool LocalDetect(uint64_t* features, uint8_t* chunkHash, uint8_t* baseHash);

        /**
         * @brief query the local feature index
         * 
         * @param feature 
         * @param baseHash 
         * @return true 
         * @return false 
         */
        bool QueryLocalFeature(uint64_t feature, string& baseHash);

    public:
        double stream4_process_time_ = 0;
        // for logs
        uint64_t _enclave_similar_num = 0;
        uint64_t _batch_simlar_num = 0;
        uint64_t _non_similar_num = 0;

        SendMsgBuffer_t send_batch_buf_;

        /**
         * @brief Construct a new Stream Phase 4 Thd object
         * 
         * @param phase_sender_obj 
         * @param inputMQ 
         * @param eid_sgx 
         */
        StreamPhase4Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase3MQ_t>* inputMQ,
            SyncIO* sync_io, LocalityCache* locality_cache);

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