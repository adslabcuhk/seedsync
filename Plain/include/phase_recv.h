/**
 * @file phase_recv.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-07-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef PHASE_RECV_H
#define PHASE_RECV_H

#include "sslConnection.h"
#include "messageQueue.h"
#include "sync_configure.h"
#include "sync_data_writer.h"
#include "phase_sender.h"
#include "stream_phase_1_thd.h"
#include "stream_phase_2_thd.h"
#include "stream_phase_3_thd.h"
#include "stream_phase_4_thd.h"
#include "stream_phase_5_thd.h"
// #include "../build/src/Enclave/storeEnclave_u.h"

extern SyncConfigure sync_config;

class PhaseRecv {
    private:
        string my_name_ = "PhaseRecv";

        uint8_t phase_id_;

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
        MessageQueue<StreamPhase5MQ_t>* stream_p5_MQ_;

        // for writer
        SyncDataWriter* sync_data_writer_obj_;

        // end flag
        bool end_flag_ = false;

        PhaseSender* phase_sender_obj_;

        StreamPhase1Thd* stream_phase_1_obj_;
        string sync_file_name_;

        bool is_first_file = true;

        // for log collection
        StreamPhase2Thd* stream_phase_2_obj_;
        StreamPhase4Thd* stream_phase_4_obj_;
        StreamPhase3Thd* stream_phase_3_obj_;
        StreamPhase5Thd* stream_phase_5_obj_;

        SyncEnclaveInfo_t* sync_enclave_info_;
        // sgx_enclave_id_t eid_sgx_;


    public:
        /**
         * @brief Construct a new Phase Recv object
         * 
         * @param recv_channel 
         * @param recv_conn_record 
         * @param p1_mq 
         */
        PhaseRecv(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record, 
            MessageQueue<StreamPhase1MQ_t>* mq, uint8_t phase_id);

        /**
         * @brief Construct a new Phase Recv object
         * 
         * @param recv_channel 
         * @param recv_conn_record 
         * @param p3_mq 
         */
        PhaseRecv(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record, 
            MessageQueue<StreamPhase3MQ_t>* p3_mq, uint8_t phase_id);

        /**
         * @brief Construct a new Phase Recv object
         * 
         * @param recv_channel 
         * @param recv_conn_record 
         * @param p4_mq 
         */
        PhaseRecv(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record, 
            MessageQueue<StreamPhase4MQ_t>* p4_mq, uint8_t phase_id);

        /**
         * @brief Construct a new Phase Recv object
         * 
         * @param recv_channel 
         * @param recv_conn_record 
         * @param p5_mq 
         */
        PhaseRecv(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record, 
            MessageQueue<StreamPhase5MQ_t>* p5_mq, uint8_t phase_id);

        /**
         * @brief Destroy the Phase Recv object
         * 
         */
        ~PhaseRecv();   

        /**
         * @brief Set the Sync Data Writer object
         * 
         * @param sync_data_writer_obj 
         */
        void SetSyncDataWriter(SyncDataWriter* sync_data_writer_obj);

        /**
         * @brief the main process of phase recv
         * 
         */
        void Run();
        
        /**
         * @brief Set the Done Flag object
         * 
         */
        void SetDoneFlag();

        /**
         * @brief Set the Sender Obj object
         * 
         * @param phase_sender 
         */
        void SetSenderObj(PhaseSender* phase_sender);

        /**
         * @brief Set the Phase 1 Obj object
         * 
         * @param stream_phase_1_obj 
         */
        void SetPhase1Obj(StreamPhase1Thd* stream_phase_1_obj, string& filename);

        /**
         * @brief Set the First Flag object
         * 
         * @param flag 
         */
        void SetFirstFlag(bool is_first) {
            // is_first_file = is_first;

            // return ;
        }

        /**
         * @brief Set the Phase Obj For Dest Log object
         * 
         * @param stream_phase_2_obj 
         * @param stream_phase_4_obj 
         */
        void SetPhaseObjForDestLog(StreamPhase2Thd* stream_phase_2_obj, 
            StreamPhase4Thd* stream_phase_4_obj);
        
        /**
         * @brief Set the Phase Obj For Src Log object
         * 
         * @param stream_phase_5_obj 
         */
        void SetPhaseObjForSrcLog(StreamPhase3Thd* stream_phase_3_obj, StreamPhase5Thd* stream_phase_5_obj);

        // /**
        //  * @brief Set the Sgx Eid object
        //  * 
        //  * @param eid 
        //  */
        // void SetSgxEid(sgx_enclave_id_t eid) {
        //     eid_sgx_ = eid;
        // }
};

#endif