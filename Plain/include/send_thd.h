/**
 * @file send_thd.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-01-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SEND_THD_H
#define SEND_THD_H

#include "sync_configure.h"
#include "sslConnection.h"
#include "lckFreeMQ.h"
#include "messageQueue.h"
#include "cryptoPrimitive.h"

extern SyncConfigure sync_config;

// static const uint32_t META_POOL_SIZE = 64; // the number of meta batches in memory pool
// static const uint32_t DATA_POOL_SIZE = 16; // the number of data batches in memory pool

class SendThd {
    private:
        string my_name_ = "SendThd";
        
        // network connection
        SSLConnection* send_channel_;
        pair<int, SSL*> send_conn_record_;

        // config
        uint64_t send_meta_batch_size_ = 0;
        uint64_t send_data_batch_size_ = 0;
        // send batch buffer
        SendMsgBuffer_t send_buf_;
        SendMsgBuffer_t enc_send_buf_; // do encryption with sessionKey before sending

        // memory pool for metadata traffic
        uint8_t** metadata_pool_;
        std::list<uint32_t> meta_valid_idx_; // the avalible index number in current meta pool
        // memory pool for data traffic
        uint8_t** data_pool_;
        std::list<uint32_t> data_valid_idx_; // the avalible index number in current data pool
        
        // for MQ
        // !!!! MQ size should be the sum of (META_POOL_SIZE + DATA_POOL_SIZE)
        LockfreeMQ<SendMQEntry_t>* send_MQ_;
        // lock for multiple threads to access the memory pool
        // mutex* send_lck_;
        std::mutex mutex_;
        // boost::shared_mutex mtx;

        // for secure channel encryption
        CryptoPrimitive* crypto_util_;
        uint8_t session_key_[CHUNK_HASH_HMAC_SIZE];
        EVP_CIPHER_CTX* cipher_ctx_;
        EVP_MD_CTX* md_ctx_;

        // total send batch num
        uint64_t batch_num_ = 0;

    public:

        /**
         * @brief Construct a new Send Thd object
         * 
         * @param send_channel 
         */
        SendThd(SSLConnection* send_channel, pair<int, SSL*> send_conn_record, 
            uint8_t* session_key, size_t session_key_size);

        SendThd(SSLConnection* send_channel, pair<int, SSL*> send_conn_record, 
            LockfreeMQ<SendMQEntry_t>* send_mq);
        
        SendThd(SSLConnection* send_channel, pair<int, SSL*> send_conn_record);

        /**
         * @brief Destroy the Send Thd object
         * 
         */
        ~SendThd();

        /**
         * @brief the main process of SendThd
         * 
         */
        void Run();
        
        /**
         * @brief send the sync login for connection estabulishment
         * 
         */
        void SyncLogin();

        /**
         * @brief send the login response
         * 
         */
        void LoginResponse();

        /**
         * @brief send the batch of data
         * 
         * @param send_msg_buf 
         */
        void SendBatch(SendMsgBuffer_t& send_msg_buf);

        void SendBatchtst(SendMsgBuffer_t& send_msg_buf);

        /**
         * @brief send the batch of data
         * 
         * @param send_buf 
         * @param send_size 
         */
        void SendBatch(uint8_t* send_buf, uint32_t send_size);

        // void SetSendLck(std::mutex* lck) {
        //     send_lck_ = lck;
        // }

        /**
         * @brief Insert into the send MQ and memory pool
         * 
         * @param flag 
         * @param send_buf 
         * @return true 
         * @return false 
         */
        bool InsertSendMQ(uint8_t flag, SendMsgBuffer_t& send_buf);
};

#endif