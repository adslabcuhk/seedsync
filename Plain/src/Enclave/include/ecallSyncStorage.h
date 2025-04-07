/**
 * @file ecallSyncStorage.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-02-23
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ECALL_SYNC_STORE_H
#define ECALL_SYNC_STORE_H

#include "commonEnclave.h"
#include "ecallEnc.h"
#include "ecallLz4.h"

class EcallCrypto;

class EcallSyncStorage {
    private:
        string my_name_ = "EcallSyncStorage";

        // for crypto
        EcallCrypto* crypto_util_;
        EVP_CIPHER_CTX* cipher_ctx_;
        EVP_MD_CTX* md_ctx_;
        uint8_t iv_[CRYPTO_BLOCK_SIZE];

        // ifstream read_hdl_;
        // ofstream write_hdl_;

        // for rw lock

        // for current offset in the tmp file
        uint64_t cur_idx_;

    public:
        /**
         * @brief Construct a new Ecall Sync Storage object
         * 
         */
        EcallSyncStorage();

        /**
         * @brief Destroy the Ecall Sync Storage object
         * 
         */
        ~EcallSyncStorage();

        /**
         * @brief write the chunk batch, waiting for process of the next phases
         * 
         * @param chunk_data 
         * @param chunk_num 
         */
        void WriteTmpChunkBatch(uint8_t* chunk_data, uint32_t chunk_num);

        /**
         * @brief read the pre-persisted chunk batches
         * 
         * @param chunk_data 
         * @param chunk_num 
         */
        void ReadTmpChunkBatch(uint8_t* chunk_data, uint32_t& chunk_num);
};

#endif