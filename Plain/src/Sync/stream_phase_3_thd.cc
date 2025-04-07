/**
 * @file stream_phase_3_thd.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/stream_phase_3_thd.h"

struct timeval stream3_stime;
struct timeval stream3_etime;

/**
 * @brief Construct a new Stream Phase 3 Thd:: Stream Phase 3 Thd object
 * 
 * @param phase_sender_obj 
 * @param inputMQ 
 * @param out_fp_db 
 * @param eid_sgx 
 */
StreamPhase3Thd::StreamPhase3Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase2MQ_t>* inputMQ,
    AbsDatabase* out_fp_db, SyncIO* sync_io) {
    
    // send_thd_obj_ = send_thd_obj;
    phase_sender_obj_ = phase_sender_obj;
    inputMQ_ = inputMQ;
    out_chunk_db_ = out_fp_db;
    // sgx_eid_ = eid_sgx;
    sync_io_ = sync_io;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
    cout << "stream phase3 size " << sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)) << endl;
    send_batch_buf_.dataBuffer = send_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    send_batch_buf_.header = (NetworkHead_t*) send_batch_buf_.sendBuffer;
    send_batch_buf_.header->currentItemNum = 0;
    send_batch_buf_.header->dataSize = 0;

    recv_buf_ = (uint8_t*)malloc(sync_config.GetMetaBatchSize() * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
    recv_num_ = 0;
    recv_offset_ = 0;

    // for chunk outquery
    out_chunk_query_.OutChunkQueryBase = (OutChunkQueryEntry_t*) malloc(sizeof(OutChunkQueryEntry_t) * 
        sync_config.GetMetaBatchSize());
    out_chunk_query_.queryNum = 0;

    // init req container buffer
    req_containers_.idBuffer = (uint8_t*) malloc(CONTAINER_CAPPING_VALUE * 
        CONTAINER_ID_LENGTH);
    req_containers_.containerArray = (uint8_t**) malloc(CONTAINER_CAPPING_VALUE * 
        sizeof(uint8_t*));
    req_containers_.idNum = 0;
    for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
        req_containers_.containerArray[i] = (uint8_t*) malloc(sizeof(uint8_t) * 
            MAX_CONTAINER_SIZE_WITH_META);
    }

    plain_unifp_list_ = (uint8_t*)malloc(sync_config.GetMetaBatchSize() * 
        CHUNK_HASH_HMAC_SIZE);

    tool::Logging(my_name_.c_str(), "init StreamPhase3Thd.\n");
}

/**
 * @brief Destroy the Stream Phase 3 Thd object
 * 
 */
StreamPhase3Thd::~StreamPhase3Thd() {
    free(send_batch_buf_.sendBuffer);
    free(recv_buf_);
    free(out_chunk_query_.OutChunkQueryBase);

    free(req_containers_.idBuffer);
    for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
        free(req_containers_.containerArray[i]);
    }
    free(req_containers_.containerArray);

    free(plain_unifp_list_);
}

/**
 * @brief the main process
 * 
 */
void StreamPhase3Thd::Run() {
    bool job_done = false;
    StreamPhase2MQ_t tmp_uni_hash;

    tool::Logging(my_name_.c_str(), "the main thread is running.\n");

    while (true) {

        if (inputMQ_->done_ && inputMQ_->IsEmpty()) {
            job_done = true;
        }

        if (inputMQ_->Pop(tmp_uni_hash)) {
            if (tmp_uni_hash.is_file_end == NOT_FILE_END) {
                // cout<<"pop uni hash"<<endl;
                memcpy(recv_buf_ + recv_offset_, tmp_uni_hash.chunkHash, CHUNK_HASH_HMAC_SIZE);
                recv_offset_ += CHUNK_HASH_HMAC_SIZE;
                recv_num_ ++;

                if (recv_num_ == sync_config.GetMetaBatchSize()) {
                    ProcessOneBatch();
                    recv_num_ = 0;
                    recv_offset_ = 0;
                }
            } 
            else if (tmp_uni_hash.is_file_end == FILE_END) {
                // cout<<"file end phase-3"<<endl;

                if (recv_num_ != 0) {
                    ProcessOneBatch();
                    recv_num_ = 0;
                    recv_offset_ = 0;
                }

                // send the file end flag
                send_batch_buf_.header->messageType = FILE_END_FEATURE;
                // this->SendBatch(send_batch_buf_);
                phase_sender_obj_->SendBatch(&send_batch_buf_);

                cout<<"process time phase-3 "<<stream3_process_time_<<endl;

                job_done = true;
                 
            }
        }

        if (job_done) {
            break;
        }
    }

    // send the end flag
    send_batch_buf_.header->messageType = SYNC_FEATURE_END;
    // this->SendBatch(send_batch_buf_);
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    tool::Logging(my_name_.c_str(), "StreamPhase3Thd exits.\n");

    return ;
}

/**
 * @brief process one batch of uni fp list, return the features
 * 
 */
void StreamPhase3Thd::ProcessOneBatch() {
    gettimeofday(&stream3_stime, NULL);

    // do ecall: input is recv uni FP list, output is [features + fps]
    send_batch_buf_.header->currentItemNum = recv_num_;
    // Ecall_Stream_Phase3_ProcessBatch(sgx_eid_, recv_buf_, recv_num_,
    //     &req_containers_, &out_chunk_query_, 
    //     send_batch_buf_.dataBuffer, send_batch_buf_.header->currentItemNum);

    ProcessPlainBatch(recv_buf_, recv_num_,
        &req_containers_, &out_chunk_query_, 
        send_batch_buf_.dataBuffer, send_batch_buf_.header->currentItemNum);
    
    // cout<<"feature header "<<send_batch_buf_.header->currentItemNum<<endl;

    // prepare the send buf
    send_batch_buf_.header->messageType = SYNC_FEATURE;
    send_batch_buf_.header->dataSize = send_batch_buf_.header->currentItemNum * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
    // this->SendBatch(send_batch_buf_);
    phase_sender_obj_->SendBatch(&send_batch_buf_);
    // reset
    send_batch_buf_.header->dataSize = 0;
    send_batch_buf_.header->currentItemNum = 0;

    gettimeofday(&stream3_etime, NULL);
    stream3_process_time_ += tool::GetTimeDiff(stream3_stime, stream3_etime);

    return ;
}

/**
 * @brief process plain batch
 * 
 * @param unifp_list 
 * @param unifp_num 
 * @param req_container 
 * @param addr_query 
 * @param feature_list 
 * @param feature_num 
 */
void StreamPhase3Thd::ProcessPlainBatch(uint8_t* unifp_list, uint32_t unifp_num,
    ReqContainer_t* req_container, OutChunkQuery_t* addr_query,
    uint8_t* feature_list, size_t feature_num) {

    // decrypt the batch with session key first
    // crypto_util_->DecryptWithKey(cipher_ctx_, unifp_list, unifp_num * CHUNK_HASH_HMAC_SIZE,
    //     session_key_, plain_unifp_list_);
    // debug
    memcpy(plain_unifp_list_, unifp_list, unifp_num * CHUNK_HASH_HMAC_SIZE);

    uint8_t* id_buf = req_container->idBuffer;
    uint8_t** container_array = req_container->containerArray;

    string tmp_container_id;
    unordered_map<string, uint32_t> tmp_container_map;
    tmp_container_map.reserve(CONTAINER_CAPPING_VALUE);

    // uint64_t chunk_features[SUPER_FEATURE_PER_CHUNK];
    uint32_t write_feature_offset = 0;

    // query the chunk index to get the chunk Addr
    OutChunkQueryEntry_t* tmp_query_entry = addr_query->OutChunkQueryBase;
    uint32_t query_num = 0;
    uint32_t fplist_offset = 0;

    // SyncEnclave::Logging("unifp num ", "%d\n", unifp_num);

    for (size_t i = 0; i < unifp_num; i++) {
        // crypto_util_->IndexAESCMCEnc(cipher_ctx_, plain_unifp_list_ + fplist_offset,
        //     CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
        memcpy(tmp_query_entry->chunkHash, plain_unifp_list_ + fplist_offset,
            CHUNK_HASH_HMAC_SIZE);
        fplist_offset += CHUNK_HASH_HMAC_SIZE;
        query_num ++;
        tmp_query_entry ++;
    }
    // reset offset
    fplist_offset = 0;

    addr_query->queryNum = query_num;
    // Ocall_QueryChunkIndex(addr_query);
    sync_io_->SyncIO_QueryChunkIndex((void*)addr_query);
    
    // point back to base
    tmp_query_entry = addr_query->OutChunkQueryBase;
    RecipeEntry_t dec_query_entry;
    EnclaveRecipeEntry_t tmp_local_addr_entry;
    for (size_t i = 0; i < addr_query->queryNum; i++) {
        // crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->value, 
        //     sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
        //     (uint8_t*)&dec_query_entry);
        memcpy((uint8_t*)&dec_query_entry, (uint8_t*)&tmp_query_entry->value, 
            sizeof(RecipeEntry_t));
        
        // SyncEnclave::Logging("addr offset ", "%d\n", dec_query_entry.offset);
        // SyncEnclave::Logging("addr len ", "%d\n", dec_query_entry.length);
        // Ocall_PrintfBinary(dec_query_entry.containerName, CONTAINER_ID_LENGTH);

        _total_unique_num ++;
        _total_unique_size += dec_query_entry.length;
        
        // get the chunk addr
        tmp_container_id.assign((char*)dec_query_entry.containerName, 
            CONTAINER_ID_LENGTH);
        // prepare the local addr entry
        tmp_local_addr_entry.offset = dec_query_entry.offset;
        tmp_local_addr_entry.length = dec_query_entry.length;
        
        auto find_cont = tmp_container_map.find(tmp_container_id);
        if (find_cont == tmp_container_map.end()) {
            // unique container for load buffer
            // SyncEnclave::Logging("uni container","\n");
            tmp_local_addr_entry.containerID = req_container->idNum;
            tmp_container_map[tmp_container_id] = req_container->idNum;
            memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
                tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
            
            // uint8_t check[CONTAINER_ID_LENGTH];
            // memcpy(check, id_buf + req_container->idNum * CONTAINER_ID_LENGTH, CONTAINER_ID_LENGTH);
            // Ocall_PrintfBinary(check, CONTAINER_ID_LENGTH);
            
            req_container->idNum ++;
        }
        else {
            // exist in load buffer, get the local id
            // SyncEnclave::Logging("exist container","\n");
            tmp_local_addr_entry.containerID = find_cont->second;
        }
        local_addr_list_.push_back(tmp_local_addr_entry);

        // SyncEnclave::Logging("req container num ","%d\n", req_container->idNum);
        // SyncEnclave::Logging("local addr num ","%d\n", local_addr_list_.size());
        if (req_container->idNum == CONTAINER_CAPPING_VALUE) {
            // fetch containers
            // Ocall_SyncGetReqContainer((void*)req_container);
            sync_io_->SyncIO_SyncGetReqContainer((void*)req_container);
            // SyncEnclave::Logging("after ocall req cont","\n");
            // read chunk from the encrypted container buffer
            for (size_t k = 0; k < local_addr_list_.size(); k++) {
                // // get chunk data one by one
                // uint32_t local_id = local_addr_list_[k].containerID;
                // uint32_t local_offset = local_addr_list_[k].offset;
                // uint32_t local_size = local_addr_list_[k].length;
                // // get the metaoffset of the container
                // uint32_t meta_offset = 0;
                // memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
                // meta_offset += sizeof(uint32_t);
                // uint8_t* local_chunk_data = container_array[local_id] + local_offset + meta_offset;
                
                // get the features from metadata session
                uint32_t local_id = local_addr_list_[k].containerID;
                string req_fp;
                req_fp.assign((char*)plain_unifp_list_ + fplist_offset, CHUNK_HASH_HMAC_SIZE);
                fplist_offset += CHUNK_HASH_HMAC_SIZE;
                // find the features based on fp
                uint32_t meta_offset = 0;
                uint32_t meta_num = 0;
                memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
                meta_num = meta_offset / (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                uint8_t* meta_session = container_array[local_id] + sizeof(uint32_t);

                // point to the first fp
                uint32_t read_meta_offset = SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
                uint8_t check_hash_req[CHUNK_HASH_HMAC_SIZE];
                uint8_t check_hash_read[CHUNK_HASH_HMAC_SIZE];
                for (size_t j = 0; j < meta_num; j++) {
                    // get the chunk fp in meta session one by one
                    string read_fp;
                    read_fp.assign((char*)meta_session + read_meta_offset, CHUNK_HASH_HMAC_SIZE);

                    if (read_fp == req_fp) {
                        // get the features
                        size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);

                        // // debug
                        // memcpy((char*)check_features, meta_session +
                        //     read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                        // memcpy(check_hash_read, read_fp.c_str(), CHUNK_HASH_HMAC_SIZE);
                        // memcpy(check_hash_req, req_fp.c_str(), CHUNK_HASH_HMAC_SIZE);

                        // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
                        //     SyncEnclave::Logging("check feature ", "%d\t", check_features[j]);
                        // }
                        // Ocall_PrintfBinary(check_hash_req, CHUNK_HASH_HMAC_SIZE);
                        // Ocall_PrintfBinary(check_hash_read, CHUNK_HASH_HMAC_SIZE);

                        // reuse input buffer
                        memcpy((char*)unifp_list + write_feature_offset, 
                            meta_session + read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                        write_feature_offset += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                        memcpy((char*)unifp_list + write_feature_offset, &read_fp[0], CHUNK_HASH_HMAC_SIZE);
                        write_feature_offset += CHUNK_HASH_HMAC_SIZE;
                        break;
                    }

                    // point to the next fp
                    read_meta_offset += (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                }
            }

            // reset
            req_container->idNum = 0;
            tmp_container_map.clear();
            local_addr_list_.clear();
        }
        // move on
        tmp_query_entry ++;
    }

    // deal with tail
    if (req_container->idNum != 0) {
        // fetch containers
        // SyncEnclave::Logging("fetch containers","%d\n", req_container->idNum);
        // for (size_t i = 0; i < req_container->idNum; i++) {
        //     Ocall_PrintfBinary(id_buf + i * CONTAINER_ID_LENGTH, CONTAINER_ID_LENGTH);
        // }
        // Ocall_SyncGetReqContainer((void*)req_container);
        sync_io_->SyncIO_SyncGetReqContainer((void*)req_container);
        // SyncEnclave::Logging("after ocall req cont","\n");
        // read chunk from the encrypted container buffer
        for (size_t k = 0; k < local_addr_list_.size(); k++) {
            // // get chunk data one by one
            // uint32_t local_id = local_addr_list_[k].containerID;
            // uint32_t local_offset = local_addr_list_[k].offset;
            // uint32_t local_size = local_addr_list_[k].length;
            // // get the metaoffset of the container
            // uint32_t meta_offset = 0;
            // memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
            // meta_offset += sizeof(uint32_t);
            // uint8_t* local_chunk_data = container_array[local_id] + local_offset + meta_offset;
            
            // get the features from metadata session
            uint32_t local_id = local_addr_list_[k].containerID;
            string req_fp;
            req_fp.assign((char*)plain_unifp_list_ + fplist_offset, CHUNK_HASH_HMAC_SIZE);
            fplist_offset += CHUNK_HASH_HMAC_SIZE;
            // find the features based on fp
            uint32_t meta_offset = 0;
            uint32_t meta_num = 0;
            memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
            // SyncEnclave::Logging("metaoffset ","%d\n", meta_offset);
            meta_num = meta_offset / (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
            // SyncEnclave::Logging("metanum ","%d\n", meta_num);
            // SyncEnclave::Logging("entrysize ","%d\n", (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));

            uint8_t* meta_session = container_array[local_id] + sizeof(uint32_t);

            // point to the first fp
            uint32_t read_meta_offset = SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
            // // point to the first feature
            // uint32_t read_feature_offset = 0;
            // uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
            // uint8_t check_hash_req[CHUNK_HASH_HMAC_SIZE];
            // uint8_t check_hash_read[CHUNK_HASH_HMAC_SIZE];
            for (size_t j = 0; j < meta_num; j++) {
                // get the chunk fp in meta session one by one
                string read_fp;
                read_fp.assign((char*)meta_session + read_meta_offset, CHUNK_HASH_HMAC_SIZE);
                // SyncEnclave::Logging("read fp offset ","%d\n", read_meta_offset);

                if (read_fp == req_fp) {
                    // get the features
                    // size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                    size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                    // SyncEnclave::Logging("read feature offset ","%d\n", read_feature_offset);

                    // // debug
                    // memcpy((char*)check_features, meta_session +
                    //     read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                    // memcpy(check_hash_read, read_fp.c_str(), CHUNK_HASH_HMAC_SIZE);
                    // memcpy(check_hash_req, req_fp.c_str(), CHUNK_HASH_HMAC_SIZE);

                    // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
                    //     SyncEnclave::Logging("check feature ", "%d\t", check_features[j]);
                    // }
                    // Ocall_PrintfBinary(check_hash_req, CHUNK_HASH_HMAC_SIZE);
                    // Ocall_PrintfBinary(check_hash_read, CHUNK_HASH_HMAC_SIZE);

                    // reuse input buffer
                    memcpy((char*)unifp_list + write_feature_offset, 
                        meta_session + read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                    write_feature_offset += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                    memcpy((char*)unifp_list + write_feature_offset, &read_fp[0], CHUNK_HASH_HMAC_SIZE);
                    write_feature_offset += CHUNK_HASH_HMAC_SIZE;

                    break;
                }

                // point to the next fp
                read_meta_offset += (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                // SyncEnclave::Logging("skipsize ","%d\n", (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
            }
        }

        // reset
        req_container->idNum = 0;
        tmp_container_map.clear();
        local_addr_list_.clear(); 
    }

    // encrypt the feature list with session key
    // crypto_util_->EncryptWithKey(cipher_ctx_, unifp_list, write_feature_offset, 
    //     session_key_, feature_list);
    // debug
    memcpy(feature_list, unifp_list, write_feature_offset);

    // feature_num = write_feature_offset / (SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + CHUNK_HASH_HMAC_SIZE);
    // SyncEnclave::Logging("feature num in header ", "%d, %d, %d\n",write_feature_offset, feature_num, write_feature_offset / (SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + CHUNK_HASH_HMAC_SIZE));

    return ;
}

/**
 * @brief insert the batch into send MQ
 * 
 * @param send_buf 
 */
// void StreamPhase3Thd::SendBatch(SendMsgBuffer_t& send_buf) {
//     uint8_t flag = META_POOL;
//     while (true) {
//         if (send_thd_obj_->InsertSendMQ(flag, send_buf)) {
//             break;
//         }
//     }

//     return ;
// }