/**
 * @file stream_phase_1_thd.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-23
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef STREAM_PHASE_1_THD_H
#define STREAM_PHASE_1_THD_H

// #include "send_thd.h"
#include "phase_sender.h"
#include "sync_configure.h"
#include "configure.h"
#include "absDatabase.h"
#include "cryptoPrimitive.h"

// #include "../build/src/Enclave/storeEnclave_u.h"

#include <map>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

extern SyncConfigure sync_config;
extern Configure config;

class StreamPhase1Thd {
    private:
        string my_name_ = "StreamPhase1Thd";

        // // for sending
        // SendThd* send_thd_obj_;

        // string recipe_path_;

        // uint64_t recipe_batch_size_;

        // // for ecall
        // sgx_enclave_id_t eid_sgx_;

        CryptoPrimitive* cryptoObj_;
        EVP_CIPHER_CTX* cipherCtx_;
        EVP_MD_CTX* mdCtx_;

        /**
         * @brief process one file recipe
         * 
         * @param recipe_path 
         */
        void ProcessOneRecipe(string recipe_path);

        /**
         * @brief insert the batch into send MQ
         * 
         * @param send_buf 
         */
        void SendBatch(SendMsgBuffer_t& send_buf);

        /**
         * @brief process the plain batch
         * 
         * @param filename_buf 
         * @param currentBatchID 
         * @param recipe_buf 
         * @param recipe_num 
         * @param chunk_hash_buf 
         */
        void ProcessPlainBatch(uint8_t* filename_buf, uint32_t currentBatchID,
            uint8_t* recipe_buf, size_t recipe_num, uint8_t* chunk_hash_buf);

    public:
        double stream1_process_time_ = 0;
        uint64_t _total_chunk_fp_num = 0;
        uint64_t _total_chunk_size = 0;
        
        SendMsgBuffer_t send_batch_buf_;
        uint8_t* read_recipe_buf_;

        // for sending
        // SendThd* send_thd_obj_;
        PhaseSender* phase_sender_obj_;

        string recipe_path_;

        uint64_t recipe_batch_size_;

        // for ecall
        // sgx_enclave_id_t eid_sgx_;

        /**
         * @brief Construct a new Stream Phase 1 Thd object
         * 
         * @param phase_sender_obj 
         * @param eid_sgx 
         */
        StreamPhase1Thd(PhaseSender* phase_sender_obj);

        /**
         * @brief Destroy the Stream Phase 1 Thd object
         * 
         */
        ~StreamPhase1Thd();

        /**
         * @brief start the sync request by sending chunk list
         * 
         */
        void SyncRequest();

        /**
         * @brief sync a single file
         * 
         * @param file_name 
         */
        void SyncRequest(string& file_name);
};

#endif