/**
 * @file recv_thd.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/recv_thd.h"

struct timeval recv_stime;
struct timeval recv_etime;
struct timeval recv_etime2;

/**
 * @brief Construct a new Recv Thd object
 * 
 * @param recv_channel 
 * @param recv_conn_record 
 * @param p1_mq 
 * @param p2_mq 
 * @param p3_mq 
 * @param p4_mq 
 * @param p5_mq 
 */
RecvThd::RecvThd(SSLConnection* recv_channel, pair<int, SSL*> recv_conn_record,
    MessageQueue<StreamPhase1MQ_t>* p1_mq, MessageQueue<StreamPhase2MQ_t>* p2_mq,
    MessageQueue<StreamPhase3MQ_t>* p3_mq, MessageQueue<StreamPhase4MQ_t>* p4_mq,
    SyncDataWriter* sync_data_writer_obj) {

    recv_channel_ = recv_channel;
    recv_conn_record_ = recv_conn_record;

    // TODO: add MQs
    stream_p1_MQ_ = p1_mq;
    stream_p2_MQ_ = p2_mq;
    stream_p3_MQ_ = p3_mq;
    stream_p4_MQ_ = p4_mq;
    // p6_MQ_ = p6_mq;
    // p7_MQ_ = p7_mq;
    // p8_MQ_ = p8_mq;
    // sync_data_writer_MQ_ = writer_mq;

    sync_data_writer_obj_ = sync_data_writer_obj;

    // init recv buffer
    recv_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sync_config.GetDataBatchSize() * sizeof(Msg_t));
    recv_buf_.header = (NetworkHead_t*) recv_buf_.sendBuffer;
    recv_buf_.header->dataSize = 0;
    recv_buf_.header->currentItemNum = 0;
    recv_buf_.dataBuffer = recv_buf_.sendBuffer + sizeof(NetworkHead_t);

    tool::Logging(my_name_.c_str(), "init the RecvThd.\n");

    gettimeofday(&recv_stime, NULL);
}

/**
 * @brief Destroy the Recv Thd object
 * 
 */
RecvThd::~RecvThd() {
    free(recv_buf_.sendBuffer);
}

/**
 * @brief the main process of recv thd
 * 
 */
void RecvThd::Run() {
    // recv data -> parse the protocol header -> pass data to phase MQs
    uint32_t recv_size = 0;
    string client_ip;
    SSL* client_ssl = recv_conn_record_.second;

    tool::Logging(my_name_.c_str(), "the main thread is running.\n");
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
        else {
            // recv data, parse protocol header
            switch (recv_buf_.header->messageType) {
                // case SYNC_FILE_NAME: {
                //     // recv filename list, return:
                //     // 1. unique file name for request (pull)
                //     // 2. segment fp list for sync (push)

            //     // push filenames to MQ of phase-1
            //     cout<<"sync file name"<<endl;
            //     cout<<"current item num "<<recv_buf_.header->currentItemNum<<endl;
            //     phase1MQ_t tmp_entry;
            //     int offset = 0;
            //     for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
            //         memcpy(tmp_entry.fileName, recv_buf_.dataBuffer + offset,
            //             CHUNK_HASH_HMAC_SIZE * 2);
            //         offset += (CHUNK_HASH_HMAC_SIZE * 2);
            //         p1_MQ_->Push(tmp_entry);
            //         cout<<"p1 MQ ++"<<endl;
            //     }

            //     break;
            // }
            // case SYNC_FILE_NAME_END_FLAG: {
            //     // recv the end flag, set MQ
            //     cout<<"sync filename end, set the MQ end flag"<<endl;
            //     p1_MQ_->done_ = true;

            //     break;
            // }
            // case SYNC_UNI_FILE_NAME: {
            //     // recv uni filename list, return:
            //     // 1. segment fp list
            //     cout<<"sync uni file name"<<endl;
            //     phase2MQ_t tmp_entry;
            //     int offset = 0;
            //     for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
            //         memcpy(tmp_entry.fileName, recv_buf_.dataBuffer + offset,
            //             CHUNK_HASH_HMAC_SIZE * 2);
            //         offset += (CHUNK_HASH_HMAC_SIZE * 2);
            //         cout<<tmp_entry.fileName<<endl;
            //         p2_MQ_->Push(tmp_entry);
            //         cout<<"p2 MQ ++"<<endl;
            //     }

            //     break;
            // }
            // case SYNC_UNI_FILE_NAME_END_FLAG: {
            //     // recv the end flag, set MQ
            //     cout<<"uni sync filename end, set the MQ end flag"<<endl;
            //     // p2_MQ_->done_ = true;

            //     break;
            // }
            // case SYNC_FILE_START: {
            //     // start uploading the seg fp of a file
            //     // recv the network header + file name
            //     // TODO: for file recipe reconstruction

            //     break;
            // }
            // case SYNC_SEG_FP: {
            //     // recv seg FP list, return:
            //     // 1. unique seg FP list
            //     cout<<"sync seg fp list"<<endl;
            //     // push seg FPs to MQ of phase-3
            //     phase3MQ_t tmp_entry;
            //     int offset = 0;
            //     for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
            //         memcpy(tmp_entry.segHash, recv_buf_.dataBuffer + offset,
            //             CHUNK_HASH_HMAC_SIZE);
            //         offset += CHUNK_HASH_HMAC_SIZE;
            //         tmp_entry.is_find_end = NOT_FILE_END;
            //         p3_MQ_->Push(tmp_entry);
            //         cout<<"p3 MQ ++"<<endl;
            //     }

            //     break;
            // }
            // case SYNC_FILE_END: {
            //     // indicate the end of the seg fp of a file
            //     // recv the network header
            //     // TODO: for file recipe reconstruction

            //     break;
            // }
            // case FILE_END_SEG_FP: {
            //     // p3_MQ_->file_done_ = true;
            //     phase3MQ_t tmp_entry;
            //     tmp_entry.is_find_end = FILE_END;
            //     p3_MQ_->Push(tmp_entry);
            //     cout<<"file end seg fp"<<endl;
            //     break;
            // }
            // case SYNC_SEG_FP_END_FLAG_COND : {
            //     // the seg fp end from the receiver cloud
            //     // recv the ready from the other cloud ->
            //     // triggle seg fp gen in current cloud
            //     cout<<"sync seg fp cond"<<endl;
            //     std::unique_lock<std::mutex> lck(cond_mutex);
            //     cond_flag = true;
            //     cond_v.notify_one();
            //     cout<<"set notify "<<cond_flag<<endl;

            //     cout<<"seg fp list end, set MQ job done flag"<<endl;
            //     p3_MQ_->done_ = true;

            //     break;
            // }
            // case SYNC_SEG_FP_END_FLAG : {
            //     // the seg fp end from the sync issuer
            //     cout<<"seg fp list end, set MQ job done flag"<<endl;
            //     // p3_MQ_->done_ = true;

            //     break;
            // }
            // case SYNC_UNI_SEG_FP: {
            //     // recv uni seg fp
            //     // query seg index to get the chunk fps of segs
            //     cout<<"sync uni seg fp"<<endl;
            //     // push to MQ of phase-4
            //     phase4MQ_t tmp_entry;
            //     int offset = 0;
            //     for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
            //         memcpy(tmp_entry.segHash, recv_buf_.dataBuffer + offset,
            //             CHUNK_HASH_HMAC_SIZE);
            //         offset += CHUNK_HASH_HMAC_SIZE;
            //         tmp_entry.is_find_end = NOT_FILE_END;
            //         p4_MQ_->Push(tmp_entry);
            //         cout<<"p4 MQ ++"<<endl;
            //     }

            //     break;
            // }
            // case FILE_END_UNI_SEG_FP: {
            //     // p4_MQ_->file_done_ = true;
            //     phase4MQ_t tmp_entry;
            //     tmp_entry.is_find_end = FILE_END;
            //     p4_MQ_->Push(tmp_entry);
            //     cout<<"file end uni seg fp"<<endl;
            //     break;
            // }
            // case SYNC_UNI_SEG_FP_END_FLAG: {
            //     cout<<"uni seg fp end, set MQ job done flag"<<endl;
            //     // p4_MQ_->done_ = true;

            //     break;
            // }
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
                    // cout<<"file end chunk fp"<<endl;
                    break;
                }
                // case SYNC_CHUNK_FP_END_FLAG: {
                //     cout<<"chunk fp end, set MQ job done flag"<<endl;
                //     // p5_MQ_->done_ = true;

                //     break;
                // }
                case SYNC_UNI_CHUNK_FP: {
                    // cout<<"sync uni chunk fp"<<endl;
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
                    // cout<<"file end uni chunk fp"<<endl;
                    break;
                }
                // case SYNC_UNI_CHUNK_FP_END_FLAG: {
                //     cout<<"uni chunk fp end"<<endl;
                //     // p6_MQ_->done_ = true; 

                //     break;
                // }
                case SYNC_FEATURE: {
                    cout<<"sync feature"<<endl;
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
                        cout<<"p3 MQ ++"<<endl;

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
                    break;
                }
                // case SYNC_FEATURE_END: {
                //     cout<<"feature end"<<endl;
                //     // p7_MQ_->done_ = true;

                //     break;
                // }
                // case SYNC_LOCAL_DELTA: {
                //     cout<<"local delta"<<endl;
                //     // TODO: step-1: write delta in storage pool, keep index (hash->basehash) first
                //     // SyncChunk_t tmp_write_data;
                //     sendDelta_t read_entry;
                //     uint32_t offset = 0;

                //     for (int i = 0; i < recv_buf_.header->currentItemNum; i++) {
                //         memcpy((char*)&read_entry, (char*)recv_buf_.dataBuffer + offset, sizeof(sendDelta_t));
                //         read_entry.is_find_end = NOT_FILE_END;
                //         offset += sizeof(sendDelta_t);
                //         sync_data_writer_MQ_->Push(read_entry);
                //         cout<<"write MQ ++"<<endl;
                //     }
                    
                //     // step-2: delta decoding
                // }
                // case FILE_END_LOCAL_DELTA: {
                //     sendDelta_t tmp_entry;
                //     tmp_entry.is_find_end = FILE_END;
                //     sync_data_writer_MQ_->Push(tmp_entry);
                //     cout<<"file end local delta"<<endl;
                //     break;
                // }
                case SYNC_BASE_HASH: {
                    cout<<"sync basehash"<<endl;
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
                        cout<<"p4 MQ ++"<<endl;
                    }

                    break;
                }
                case FILE_END_BASE_HASH: {
                    // p8_MQ_->file_done_ = true;
                    StreamPhase4MQ_t tmp_entry;
                    tmp_entry.is_file_end = FILE_END;
                    stream_p4_MQ_->Push(tmp_entry);
                    cout<<"file end base hash"<<endl;
                    break;
                }
                // case SYNC_BASE_HASH_END_FLAG: {
                //     cout<<"basehash end"<<endl;
                //     // p8_MQ_->done_ = true;

                //     break;
                // }
                case SYNC_DATA: {
                    cout<<"sync data"<<endl;
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

                    // sync_data_writer_obj_->ProcessOneBatch(recv_buf_.dataBuffer, recv_buf_.header->dataSize);

                    cout<<"after recv & write"<<endl;

                    break;
                }
                case FILE_END_SYNC_DATA: {
                    // StreamPhase5MQ_t tmp_entry;
                    // tmp_entry.is_file_end = FILE_END;
                    // stream_p5_MQ_->Push(tmp_entry);
                    cout<<"file end sync data"<<endl;

                    // TODO: deal with the tail of container & update index here

                    gettimeofday(&recv_etime, NULL);

                    double total_time = tool::GetTimeDiff(recv_stime, recv_etime);
                    cout<<"total time (dest)"<<total_time<<endl;

                    break;
                }
                default: {
                    break;
                }
            }
        }
    }

    tool::Logging(my_name_.c_str(), "thread exit.\n");

    return ;
}