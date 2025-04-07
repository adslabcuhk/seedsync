/**
 * @file phase_sender.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-07-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef PHASE_SENDER_H
#define PHASE_SENDER_H

#include "sslConnection.h"
#include "chunkStructure.h"

class PhaseSender {
    private:
        string my_name_ = "PhaseSender";
        
        // network connection
        SSLConnection* send_channel_;
        pair<int, SSL*> send_conn_record_;

        uint8_t phase_id_;
    
    public:
        /**
         * @brief Construct a new Phase Sender object
         * 
         * @param send_channel 
         * @param send_conn_record 
         * @param phase_id 
         */
        PhaseSender(SSLConnection* send_channel, pair<int, SSL*> send_conn_record, uint8_t phase_id);

        /**
         * @brief Destroy the Phase Sender object
         * 
         */
        ~PhaseSender();

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
        void SendBatch(SendMsgBuffer_t* send_msg_buf);

        /**
         * @brief connect
         * 
         */
        void LoginConnect();
};

#endif