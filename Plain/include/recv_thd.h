/**
 * @file recv_thd.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef RECV_THD_H
#define RECV_THD_H

#include "sslConnection.h"
#include "messageQueue.h"
#include "cryptoPrimitive.h"
#include "sync_configure.h"
#include "sync_data_writer.h"

#include "../build/src/Enclave/storeEnclave_u.h"

extern SyncConfigure sync_config;

extern std::condition_variable cond_v;
extern std::mutex cond_mutex;
extern bool cond_flag;

class RecvThd {
    private:
        string my_name_ = "RecvThd";

        // network connection
        SSLConnection* recv_channel_;
        pair<int, SSL*> recv_conn_record_;

        // config
        uint64_t recv_meta_batch_size_;
        uint64_t recv_data_batch_size_;
        // recv batch buffer
        SendMsgBuffer_t recv_buf_;

        // MQs of sync protocol phases
        MessageQueue<StreamPhase1MQ_t>* stream_p1_MQ_;
        MessageQueue<StreamPhase2MQ_t>* stream_p2_MQ_;
        MessageQueue<StreamPhase3MQ_t>* stream_p3_MQ_;
        MessageQueue<StreamPhase4MQ_t>* stream_p4_MQ_;
        // MessageQueue<StreamPhase5MQ_t>* stream_p5_MQ_;
        // // TODO: add mq here
        // MessageQueue<phase1MQ_t>* p1_MQ_;
        // MessageQueue<phase2MQ_t>* p2_MQ_;
        // MessageQueue<phase3MQ_t>* p3_MQ_;
        // MessageQueue<phase4MQ_t>* p4_MQ_;
        // MessageQueue<phase5MQ_t>* p5_MQ_;
        // MessageQueue<phase6MQ_t>* p6_MQ_;
        // MessageQueue<phase7MQ_t>* p7_MQ_;
        // MessageQueue<phase8MQ_t>* p8_MQ_;
        // MessageQueue<sendDelta_t>* sync_data_writer_MQ_;

        // for writer
        SyncDataWriter* sync_data_writer_obj_;

    public:
        /**
         * @brief Construct a new Recv Thd object
         * 
         * @param recv_channel 
         * @param recv_conn_record 
         * @param p1_mq 
         * @param p2_mq 
         * @param p3_mq 
         * @param p4_mq
         */
        RecvThd(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record,
            MessageQueue<StreamPhase1MQ_t>* p1_mq, MessageQueue<StreamPhase2MQ_t>* p2_mq,
            MessageQueue<StreamPhase3MQ_t>* p3_mq, MessageQueue<StreamPhase4MQ_t>* p4_mq,
            SyncDataWriter* sync_data_writer_obj);
        
        /**
         * @brief Destroy the Recv Thd object
         * 
         */
        ~RecvThd();

        /**
         * @brief the main process of recv thd
         * 
         */
        void Run();


        // void SetMQ(MessageQueue<phase1MQ_t>* p1_mq, MessageQueue<phase2MQ_t>* p2_mq);
};

#endif