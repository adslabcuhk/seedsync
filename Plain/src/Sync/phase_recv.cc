/**
 * @file phase_recv.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-07-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/phase_recv.h"

struct timeval recv_stime;
struct timeval recv_etime;
struct timeval recv_etime2;

/**
 * @brief Construct a new Phase Recv object
 * 
 * @param recv_channel 
 * @param recv_conn_record 
 * @param p1_mq 
 */
PhaseRecv::PhaseRecv(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record, 
    MessageQueue<StreamPhase1MQ_t>* mq, uint8_t phase_id) {
    
    recv_channel_ = recv_channel;
    recv_conn_record_ = recv_conn_record;

    recv_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sizeof(StreamPhase1MQ_t) * sync_config.GetMetaBatchSize());
    recv_buf_.dataBuffer = recv_buf_.sendBuffer + sizeof(NetworkHead_t);
    recv_buf_.header = (NetworkHead_t*) recv_buf_.sendBuffer;
    recv_buf_.header->currentItemNum = 0;
    recv_buf_.header->dataSize = 0;

    phase_id_ = phase_id;
    if (phase_id_ == 2) {
        stream_p1_MQ_ = mq;
        tool::Logging(my_name_.c_str(), "init the PhaseRecv for Phase2.\n");
    }
    else if (phase_id_ == 3) {
        stream_p2_MQ_ = mq;
        tool::Logging(my_name_.c_str(), "init the PhaseRecv for Phase3.\n");
    }

    gettimeofday(&recv_stime, NULL);
}

/**
 * @brief Construct a new Phase Recv object
 * 
 * @param recv_channel 
 * @param recv_conn_record 
 * @param p3_mq 
 */
PhaseRecv::PhaseRecv(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record, 
    MessageQueue<StreamPhase3MQ_t>* p3_mq, uint8_t phase_id) {
    recv_channel_ = recv_channel;
    recv_conn_record_ = recv_conn_record;
    stream_p3_MQ_ = p3_mq;
    phase_id_ = phase_id;

    recv_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sizeof(StreamPhase3MQ_t) * sync_config.GetMetaBatchSize());
    recv_buf_.dataBuffer = recv_buf_.sendBuffer + sizeof(NetworkHead_t);
    recv_buf_.header = (NetworkHead_t*) recv_buf_.sendBuffer;
    recv_buf_.header->currentItemNum = 0;
    recv_buf_.header->dataSize = 0;

    tool::Logging(my_name_.c_str(), "init the PhaseRecv for Phase4.\n");
}

/**
 * @brief Construct a new Phase Recv object
 * 
 * @param recv_channel 
 * @param recv_conn_record 
 * @param p4_mq 
 */
PhaseRecv::PhaseRecv(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record, 
    MessageQueue<StreamPhase4MQ_t>* p4_mq, uint8_t phase_id) {
    recv_channel_ = recv_channel;
    recv_conn_record_ = recv_conn_record;
    stream_p4_MQ_ = p4_mq;
    phase_id_ = phase_id;

    recv_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sizeof(StreamPhase4MQ_t) * sync_config.GetDataBatchSize());
    recv_buf_.dataBuffer = recv_buf_.sendBuffer + sizeof(NetworkHead_t);
    recv_buf_.header = (NetworkHead_t*) recv_buf_.sendBuffer;
    recv_buf_.header->currentItemNum = 0;
    recv_buf_.header->dataSize = 0;

    tool::Logging(my_name_.c_str(), "init the PhaseRecv for Phase5.\n");
}

/**
 * @brief Construct a new Phase Recv object
 * 
 * @param recv_channel 
 * @param recv_conn_record 
 * @param p5_mq 
 */
PhaseRecv::PhaseRecv(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record, 
    MessageQueue<StreamPhase5MQ_t>* p5_mq, uint8_t phase_id) {
    recv_channel_ = recv_channel;
    recv_conn_record_ = recv_conn_record;
    stream_p5_MQ_ = p5_mq;
    phase_id_ = phase_id;

    recv_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sizeof(StreamPhase5MQ_t) * sync_config.GetDataBatchSize());
    recv_buf_.dataBuffer = recv_buf_.sendBuffer + sizeof(NetworkHead_t);
    recv_buf_.header = (NetworkHead_t*) recv_buf_.sendBuffer;
    recv_buf_.header->currentItemNum = 0;
    recv_buf_.header->dataSize = 0;

    // for the dest logs
    // sync_enclave_info_ = new SyncEnclaveInfo_t;

    tool::Logging(my_name_.c_str(), "init the PhaseRecv for Phase6.\n");
}

/**
 * @brief Destroy the Phase Recv object
 * 
 */
PhaseRecv::~PhaseRecv() {
    free(recv_buf_.sendBuffer);
    // delete sync_enclave_info_;
}

/**
 * @brief the main process of phase recv
 * 
 */
void PhaseRecv::Run() {
    uint32_t recv_size = 0;
    string client_ip;
    SSL* client_ssl = recv_conn_record_.second;

    tool::Logging(my_name_.c_str(), "the main thread is running.\n");

    if (phase_id_ == 2) {
        tool::Logging(my_name_.c_str(), "for Phase-2 (%d).\n", phase_id_);
        while (true) {
            // recv data
            // cout<<"in while "<<recv_conn_record_.first<<endl;
            if (!recv_channel_->ReceiveData(client_ssl, recv_buf_.sendBuffer, 
                recv_size)) {
                tool::Logging(my_name_.c_str(), 
                    "the other cloud closed socket connect, thread exit now.\n");
                recv_channel_->GetClientIp(client_ip, client_ssl);
                recv_channel_->ClearAcceptedClientSd(client_ssl);
                break;
            }
            switch (recv_buf_.header->messageType) {
                case SYNC_CHUNK_FP: {
                    // cout<<"sync chunk fp"<<endl;
                    StreamPhase1MQ_t tmp_entry;
                    int offset = 0;
                    for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
                        memcpy(tmp_entry.chunkHash, recv_buf_.dataBuffer + offset,
                            CHUNK_HASH_HMAC_SIZE);
                        offset += CHUNK_HASH_HMAC_SIZE;
                        tmp_entry.is_file_end = NOT_FILE_END;
                        stream_p1_MQ_->Push(tmp_entry);
                        // cout<<"p1 MQ ++"<<endl;
                    }

                    break;
                }
                case FILE_END_CHUNK_FP: {
                    StreamPhase1MQ_t tmp_entry;
                    tmp_entry.is_file_end = FILE_END;
                    stream_p1_MQ_->Push(tmp_entry);
                    cout<<"file end chunk fp"<<endl;

                    is_first_file = false;

                    // listen for the next connection
                    recv_conn_record_ = recv_channel_->ListenSSL();
                    client_ssl = recv_conn_record_.second;

                    break;
                }
                case SYNC_LOGIN: {
                    // let the sender sends the login msg
                    // but this requires the send socket being used in two threads, not allowed.
                    // instead, send login msg from outside
                    tool::Logging(my_name_.c_str(), "phase-%d recv login msg.\n", phase_id_);

                    gettimeofday(&recv_stime, NULL);

                    cout<<"bool "<<is_first_file<<endl;
                    if (!is_first_file) {
                        // cout<<"before phase2 sender conn"<<endl;
                        phase_sender_obj_->LoginConnect();
                        // phase_sender_obj_->SyncLogin();
                        // cout<<"after phase2 sender conn"<<endl;
                    }
                    // phase_sender_obj_->LoginConnect();

                    break;
                }
                default: {
                    tool::Logging(my_name_.c_str(), "unknown message type in Phase-%d.\n", phase_id_);
                    break;
                }
            }
        }
    }
    
    if (phase_id_ == 3) {
        tool::Logging(my_name_.c_str(), "for Phase-3 (%d).\n", phase_id_);
        while (true) {
            if (end_flag_) {
                break;
            }
            // recv data
            if (!recv_channel_->ReceiveData(client_ssl, recv_buf_.sendBuffer, 
                recv_size)) {
                tool::Logging(my_name_.c_str(), 
                    "the other cloud closed socket connect, thread exit now.\n");
                recv_channel_->GetClientIp(client_ip, client_ssl);
                recv_channel_->ClearAcceptedClientSd(client_ssl);
                break;
            }
            switch (recv_buf_.header->messageType) {
                case SYNC_UNI_CHUNK_FP: {
                    // cout<<"sync uni chunk fp"<<endl;
                    cout<<"sync uni chunk fp size "<<recv_buf_.header->currentItemNum<<endl;
                    StreamPhase2MQ_t tmp_entry;
                    int offset = 0;
                    for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
                        memcpy(tmp_entry.chunkHash, recv_buf_.dataBuffer + offset,
                            CHUNK_HASH_HMAC_SIZE);
                        offset += CHUNK_HASH_HMAC_SIZE;
                        tmp_entry.is_file_end = NOT_FILE_END;
                        stream_p2_MQ_->Push(tmp_entry);
                        // cout<<"p2 MQ ++"<<endl;
                    }

                    break;
                }
                case FILE_END_UNI_CHUNK_FP: {
                    StreamPhase2MQ_t tmp_entry;
                    tmp_entry.is_file_end = FILE_END;
                    stream_p2_MQ_->Push(tmp_entry);
                    cout<<"file end uni chunk fp"<<endl;
                    end_flag_ = true;

                    break;
                }
                case SYNC_LOGIN: {
                    // let the sender sends the login msg
                    // but this requires the send socket being used in two threads, not allowed.
                    // instead, send login msg from outside
                    tool::Logging(my_name_.c_str(), "phase-%d recv login msg.\n", phase_id_);

                    break;
                }
                default: {
                    tool::Logging(my_name_.c_str(), "unknown message type in Phase-%d.\n", phase_id_);
                    break;
                }
            }
        }
    }


    if (phase_id_ == 4) {
        tool::Logging(my_name_.c_str(), "for Phase-4 (%d).\n", phase_id_);
        while (true) {
            // recv data
            if (!recv_channel_->ReceiveData(client_ssl, recv_buf_.sendBuffer, 
                recv_size)) {
                tool::Logging(my_name_.c_str(), 
                    "the other cloud closed socket connect, thread exit now.\n");
                recv_channel_->GetClientIp(client_ip, client_ssl);
                recv_channel_->ClearAcceptedClientSd(client_ssl);
                break;
            }
            switch (recv_buf_.header->messageType) {
                case SYNC_FEATURE: {
                    // cout<<"sync feature"<<endl;
                    StreamPhase3MQ_t tmp_entry;
                    int offset = 0;
                    for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
                        memcpy((char*)tmp_entry.features, (char*)recv_buf_.dataBuffer + offset,
                            sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
                        offset += (sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
                        memcpy((char*)tmp_entry.chunkHash, (char*)recv_buf_.dataBuffer + offset,
                            CHUNK_HASH_HMAC_SIZE);
                        offset += CHUNK_HASH_HMAC_SIZE;
                        tmp_entry.is_file_end = NOT_FILE_END;
                        stream_p3_MQ_->Push(tmp_entry);
                        // cout<<"p3 MQ ++"<<endl;

                        // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
                        //     tool::Logging("check recv feature ", "%d\t", tmp_entry.features[j]);
                        // }
                        // tool::PrintBinaryArray(tmp_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
                    }
                    
                    break;
                }
                case FILE_END_FEATURE: {
                    // p7_MQ_->file_done_ = true;
                    StreamPhase3MQ_t tmp_entry;
                    tmp_entry.is_file_end = FILE_END;
                    stream_p3_MQ_->Push(tmp_entry);
                    cout<<"file end feature"<<endl;

                    is_first_file = false;

                    // listen for the next connection
                    recv_conn_record_ = recv_channel_->ListenSSL();
                    client_ssl = recv_conn_record_.second;

                    break;
                }
                case SYNC_LOGIN: {
                    // let the sender sends the login msg
                    // but this requires the send socket being used in two threads, not allowed.
                    // instead, send login msg from outside
                    tool::Logging(my_name_.c_str(), "phase-%d recv login msg.\n", phase_id_);
                    // try

                    if (!is_first_file) {
                        phase_sender_obj_->LoginConnect();
                        // phase_sender_obj_->SyncLogin();
                    }

                    break;
                }
                default: {
                    tool::Logging(my_name_.c_str(), "unknown message type in Phase-%d.\n", phase_id_);
                    break;
                }
            }
        }
    }

    if (phase_id_ == 5) {
        tool::Logging(my_name_.c_str(), "for Phase-5 (%d).\n", phase_id_);
        while (true) {
            if (end_flag_) {
                break;
            }

            // recv data
            if (!recv_channel_->ReceiveData(client_ssl, recv_buf_.sendBuffer, 
                recv_size)) {
                tool::Logging(my_name_.c_str(), 
                    "the other cloud closed socket connect, thread exit now.\n");
                recv_channel_->GetClientIp(client_ip, client_ssl);
                recv_channel_->ClearAcceptedClientSd(client_ssl);
                break;
            }
            switch (recv_buf_.header->messageType) {
                case SYNC_BASE_HASH: {
                    // cout<<"sync basehash"<<endl;
                    StreamPhase4MQ_t tmp_entry;
                    int offset = 0;
                    for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
                        // memcpy(tmp_entry.baseHash, recv_buf_.dataBuffer + offset,
                        //     CHUNK_HASH_HMAC_SIZE);
                        // offset += CHUNK_HASH_HMAC_SIZE;
                        memcpy(&tmp_entry, recv_buf_.dataBuffer + offset, sizeof(StreamPhase4MQ_t));
                        offset += sizeof(StreamPhase4MQ_t);
                        tmp_entry.is_file_end = NOT_FILE_END;
                        stream_p4_MQ_->Push(tmp_entry);
                        // cout<<"p4 MQ ++"<<endl;
                    }

                    break;
                }
                case FILE_END_BASE_HASH: {
                    // p8_MQ_->file_done_ = true;
                    StreamPhase4MQ_t tmp_entry;
                    tmp_entry.is_file_end = FILE_END;
                    stream_p4_MQ_->Push(tmp_entry);
                    cout<<"file end base hash"<<endl;

                    end_flag_ = true;

                    break;
                }
                case SYNC_LOGIN: {
                    // let the sender sends the login msg
                    // but this requires the send socket being used in two threads, not allowed.
                    // instead, send login msg from outside
                    tool::Logging(my_name_.c_str(), "phase-%d recv login msg.\n", phase_id_);

                    break;
                }
                case SYNC_LOGIN_RESPONSE: {
                    tool::Logging(my_name_.c_str(), "sync login success! All connections have been established.\n");
                    // can issue the sync request here
                    // stream_phase_1_obj_->SyncRequest(sync_file_name_);

                    break;
                }
                default: {
                    tool::Logging(my_name_.c_str(), "unknown message type in Phase-%d.\n", phase_id_);
                    break;
                }
            }
        }
    }


    if (phase_id_ == 6) {
        tool::Logging(my_name_.c_str(), "for Phase-6 (%d).\n", phase_id_);
        while (true) {
            // recv data
            if (!recv_channel_->ReceiveData(client_ssl, recv_buf_.sendBuffer, 
                recv_size)) {
                tool::Logging(my_name_.c_str(), 
                    "the other cloud closed socket connect, thread exit now.\n");
                recv_channel_->GetClientIp(client_ip, client_ssl);
                recv_channel_->ClearAcceptedClientSd(client_ssl);
                break;
            }
            switch (recv_buf_.header->messageType) {
                case SYNC_DATA: {
                    // cout<<"sync data"<<endl;
                    cout<<"sync data size "<<recv_buf_.header->dataSize<<endl;
                    // StreamPhase5MQ_t tmp_entry;
                    // int offset = 0;
                    // for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
                    //     memcpy(&tmp_entry, recv_buf_.dataBuffer + offset, sizeof(StreamPhase5MQ_t));
                    //     offset += sizeof(StreamPhase5MQ_t);
                    //     tmp_entry.is_file_end = NOT_FILE_END;
                    //     stream_p5_MQ_->Push(tmp_entry);
                    // }

                    // simple version, fix later with memory pool + MQ
                    // sync_data_writer_obj_->ProcessBatch(recv_buf_.dataBuffer, recv_buf_.header->dataSize);

                    sync_data_writer_obj_->ProcessOneBatch(recv_buf_.dataBuffer, recv_buf_.header->dataSize);

                    // cout<<"after recv & write"<<endl;

                    break;
                }
                case FILE_END_SYNC_DATA: {
                    // StreamPhase5MQ_t tmp_entry;
                    // tmp_entry.is_file_end = FILE_END;
                    // stream_p5_MQ_->Push(tmp_entry);
                    // cout<<"file end sync data"<<endl;

                    // IMPORTANT!!!
                    // TODO: deal with the tail of container & update index here
                    sync_data_writer_obj_->ProcessTailBatch();

                    // TODO: print the (per-file) log info here


                    gettimeofday(&recv_etime, NULL);

                    double total_time = tool::GetTimeDiff(recv_stime, recv_etime);
                    cout<<"total time (dest)"<<total_time<<endl;

                    // get the logs from enclave
                    // Ecall_GetSyncEnclaveInfo(eid_sgx_, sync_enclave_info_, DEST_CLOUD);

                    // print the log info
                    string log_file_name = "Dest-Log";
                    std::ofstream log_file_hdl;

                    log_file_hdl.open(log_file_name, ios_base::app | ios_base::out);
                    if (!log_file_hdl.is_open()) {
                        tool::Logging(my_name_.c_str(), "open dest log file error.\n");
                        exit(EXIT_FAILURE);
                    }

                    // log_file_hdl << sync_enclave_info_->enclave_duplicate_num <<", " 
                    //     << sync_enclave_info_->enclave_unique_num << ", "
                    //     << sync_enclave_info_->global_duplicate_num << ", "
                    //     << sync_enclave_info_->global_unique_num << ", "
                    //     << sync_enclave_info_->enclave_similar_num << ", "
                    //     << sync_enclave_info_->batch_similar_num << ", "
                    //     << sync_enclave_info_->non_similar_num << ", "
                    //     << sync_enclave_info_->total_recv_size << ", "
                    //     << sync_enclave_info_->total_write_size << ", "
                    //     << sync_enclave_info_->only_comp_size << ", "
                    //     << sync_enclave_info_->comp_delta_size << ", "
                    //     << sync_enclave_info_->uncomp_delta_size << ", "
                    //     << sync_enclave_info_->uncomp_similar_size << ", "
                    //     << sync_enclave_info_->comp_similar_size << ", "
                    //     << total_time << ", "
                    //     << endl;

                    log_file_hdl << stream_phase_2_obj_->_enclave_duplicate_num << ", "
                        // sync_enclave_info_->enclave_duplicate_num <<", " 
                        << stream_phase_2_obj_->_enclave_unique_num << ", "
                        // << sync_enclave_info_->enclave_unique_num << ", "
                        << stream_phase_2_obj_->_global_duplicate_num << ", " 
                        // << sync_enclave_info_->global_duplicate_num << ", "
                        << stream_phase_2_obj_->_global_unique_num << ", "
                        // << sync_enclave_info_->global_unique_num << ", "
                        << stream_phase_4_obj_->_enclave_similar_num << ", "
                        // << sync_enclave_info_->enclave_similar_num << ", "
                        << stream_phase_4_obj_->_batch_simlar_num << ", "
                        // << sync_enclave_info_->batch_similar_num << ", "
                        << stream_phase_4_obj_->_non_similar_num << ", "
                        // << sync_enclave_info_->non_similar_num << ", "
                        << sync_data_writer_obj_->_total_recv_size << ", "
                        // << sync_enclave_info_->total_recv_size << ", "
                        << sync_data_writer_obj_->_total_write_size << ", "
                        // << sync_enclave_info_->total_write_size << ", "
                        << sync_data_writer_obj_->_only_comp_size << ", "
                        // << sync_enclave_info_->only_comp_size << ", "
                        << sync_data_writer_obj_->_comp_delta_size << ", "
                        // << sync_enclave_info_->comp_delta_size << ", "
                        << sync_data_writer_obj_->_uncomp_delta_size << ", "
                        // << sync_enclave_info_->uncomp_delta_size << ", "
                        << sync_data_writer_obj_->_uncomp_similar_size << ", "
                        // << sync_enclave_info_->uncomp_similar_size << ", "
                        << sync_data_writer_obj_->_comp_similar_size << ", "
                        // << sync_enclave_info_->comp_similar_size << ", "
                        << stream_phase_2_obj_->stream2_process_time_ << ", "
                        << stream_phase_4_obj_->stream4_process_time_ << ", "
                        << sync_data_writer_obj_->stream6_process_time_ << ", "
                        << total_time << ", "
                        << endl;

                    log_file_hdl.close();

                    // if (is_first_file) {
                    //     is_first_file = false;
                    // }
                    // is_first_file = false;
                    // cout<<"set bool as 0 "<<is_first_file<<endl;

                    // listen for the next connection
                    recv_conn_record_ = recv_channel_->ListenSSL();
                    client_ssl = recv_conn_record_.second;

                    // end_flag_ = true;

                    break;
                }
                case SYNC_LOGIN: {
                    // let the sender sends the login msg
                    // but this requires the send socket being used in two threads, not allowed.
                    // instead, send login msg from outside
                    tool::Logging(my_name_.c_str(), "phase-%d recv login msg.\n", phase_id_);

                    break;
                }
                default: {
                    tool::Logging(my_name_.c_str(), "unknown message type in Phase-%d.\n", phase_id_);
                    break;
                }
            }
        }
    }

    tool::Logging(my_name_.c_str(), "thread exit.\n");

    return ;
}


/**
 * @brief Set the Sync Data Writer object
 * 
 * @param sync_data_writer_obj 
 */
void PhaseRecv::SetSyncDataWriter(SyncDataWriter* sync_data_writer_obj) {
    sync_data_writer_obj_ = sync_data_writer_obj;

    return ;
}

/**
 * @brief Set the Done Flag object
 * 
 */
void PhaseRecv::SetDoneFlag() {
    end_flag_ = true;

    return ;
}

/**
 * @brief Set the Sender Obj object
 * 
 * @param phase_sender 
 */
void PhaseRecv::SetSenderObj(PhaseSender* phase_sender) {
    phase_sender_obj_ = phase_sender;
}


/**
 * @brief Set the Phase 1 Obj object
 * 
 * @param stream_phase_1_obj 
 */
void PhaseRecv::SetPhase1Obj(StreamPhase1Thd* stream_phase_1_obj, string& filename) {
    stream_phase_1_obj_ = stream_phase_1_obj;
    sync_file_name_ = filename;

    return;
}


/**
 * @brief Set the Phase Obj For Dest Log object
 * 
 * @param stream_phase_2_obj 
 * @param stream_phase_4_obj 
 */
void PhaseRecv::SetPhaseObjForDestLog(StreamPhase2Thd* stream_phase_2_obj, 
    StreamPhase4Thd* stream_phase_4_obj) {
    stream_phase_2_obj_ = stream_phase_2_obj;
    stream_phase_4_obj_ = stream_phase_4_obj;

    return ;
}

/**
 * @brief Set the Phase Obj For Src Log object
 * 
 * @param stream_phase_5_obj 
 */
void PhaseRecv::SetPhaseObjForSrcLog(StreamPhase3Thd* stream_phase_3_obj, 
    StreamPhase5Thd* stream_phase_5_obj) {
    stream_phase_3_obj_ = stream_phase_3_obj;
    stream_phase_5_obj_ = stream_phase_5_obj;

    return ;
}