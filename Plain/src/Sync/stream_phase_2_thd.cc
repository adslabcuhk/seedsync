/**
 * @file stream_phase_2_thd.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-24
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/stream_phase_2_thd.h"

struct timeval stream2_stime;
struct timeval stream2_etime;

/**
 * @brief Construct a new Stream Phase 2 Thd:: Stream Phase 2 Thd object
 * 
 * @param phase_sender_obj 
 * @param inputMQ 
 * @param out_fp_db 
 * @param eid_sgx 
 */
StreamPhase2Thd::StreamPhase2Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase1MQ_t>* inputMQ,
    AbsDatabase* out_fp_db, SyncIO* sync_io, LocalityCache* locality_cache) {
    
    // send_thd_obj_ = send_thd_obj;
    phase_sender_obj_ = phase_sender_obj;
    inputMQ_ = inputMQ;
    out_chunk_db_ = out_fp_db;
    // sgx_eid_ = eid_sgx;
    sync_io_ = sync_io;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * CHUNK_HASH_HMAC_SIZE * sizeof(uint8_t));
    send_batch_buf_.dataBuffer = send_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    send_batch_buf_.header = (NetworkHead_t*) send_batch_buf_.sendBuffer;
    send_batch_buf_.header->currentItemNum = 0;
    send_batch_buf_.header->dataSize = 0;

    // for process batch
    process_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * CHUNK_HASH_HMAC_SIZE * sizeof(uint8_t));
    process_batch_buf_.dataBuffer = process_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    process_batch_buf_.header = (NetworkHead_t*) process_batch_buf_.sendBuffer;
    process_batch_buf_.header->currentItemNum = 0;
    process_batch_buf_.header->dataSize = 0;
    // the sent offset in process batch
    sent_offset_ = 0;


    // for chunk outquery
    out_chunk_query_.OutChunkQueryBase = (OutChunkQueryEntry_t*) malloc(sizeof(OutChunkQueryEntry_t) * 
        sync_config.GetMetaBatchSize());
    out_chunk_query_.queryNum = 0;

    // init req container buffer
    batch_req_containers_.idBuffer = (uint8_t*) malloc(CONTAINER_CAPPING_VALUE * 
        CONTAINER_ID_LENGTH);
    batch_req_containers_.containerArray = (uint8_t**) malloc(CONTAINER_CAPPING_VALUE * 
        sizeof(uint8_t*));
    batch_req_containers_.idNum = 0;
    for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
        batch_req_containers_.containerArray[i] = (uint8_t*) malloc(sizeof(uint8_t) * 
            MAX_CONTAINER_SIZE);
    }
    // add the size array
    batch_req_containers_.sizeArray = (uint32_t*) malloc(sizeof(uint32_t) * CONTAINER_CAPPING_VALUE);

#if (RECOVER_CHECK == 1)
    // for debugging
    debug_out_query_.OutChunkQueryBase = (OutChunkQueryEntry_t*) malloc(sizeof(OutChunkQueryEntry_t) * 
        sync_config.GetMetaBatchSize());
    debug_out_query_.queryNum = 0;
#endif

    plain_uni_fp_list_ = (uint8_t*)malloc(2 * sync_config.GetMetaBatchSize() * CHUNK_HASH_HMAC_SIZE);
    // enclave_locality_cache_ = new LocalityCache();
    enclave_locality_cache_ = locality_cache;

    tool::Logging(my_name_.c_str(), "init StreamPhase2Thd.\n");
}

/**
 * @brief Destroy the Stream Phase 2 Thd object
 * 
 */
StreamPhase2Thd::~StreamPhase2Thd() {
    free(send_batch_buf_.sendBuffer);
    free(process_batch_buf_.sendBuffer);
    free(out_chunk_query_.OutChunkQueryBase);

    free(batch_req_containers_.idBuffer);
    for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
        free(batch_req_containers_.containerArray[i]);
    }
    free(batch_req_containers_.containerArray);
    free(batch_req_containers_.sizeArray);

#if (RECOVER_CHECK == 1)
    free(debug_out_query_.OutChunkQueryBase);
#endif

    // delete enclave_locality_cache_;

    free(plain_uni_fp_list_);
}

/**
 * @brief the main process
 * 
 */
void StreamPhase2Thd::Run() {
    bool job_done = false;
    StreamPhase1MQ_t tmp_chunk_hash;

    tool::Logging(my_name_.c_str(), "the main thread is running.\n");
    
    while (true) {

        if (inputMQ_->done_ && inputMQ_->IsEmpty()) {
            job_done = true;
        }

        if (inputMQ_->Pop(tmp_chunk_hash)) {
            if (tmp_chunk_hash.is_file_end == NOT_FILE_END) {
                // cout<<"pop chunk hash"<<endl;
                memcpy(process_batch_buf_.dataBuffer + process_batch_buf_.header->dataSize,
                    tmp_chunk_hash.chunkHash, CHUNK_HASH_HMAC_SIZE);
                process_batch_buf_.header->dataSize += CHUNK_HASH_HMAC_SIZE;
                process_batch_buf_.header->currentItemNum ++;

                if (process_batch_buf_.header->currentItemNum == sync_config.GetMetaBatchSize()) {
                    ProcessOneBatch();

                    SendOneBatch();

                    // reset the process batch buffer
                    process_batch_buf_.header->dataSize = 0;
                    process_batch_buf_.header->currentItemNum = 0;
                }
            }
            else if (tmp_chunk_hash.is_file_end == FILE_END) {
                // cout<<"file end phase-2"<<endl;
                if (process_batch_buf_.header->currentItemNum != 0) {
                    ProcessOneBatch();

                    SendOneBatch();

                    // send the tail
                    if (send_batch_buf_.header->currentItemNum != 0) {
                        phase_sender_obj_->SendBatch(&send_batch_buf_);
                        send_batch_buf_.header->dataSize = 0;
                        send_batch_buf_.header->currentItemNum = 0;
                    }

                    // reset the batch buffer
                    process_batch_buf_.header->dataSize = 0;
                    process_batch_buf_.header->currentItemNum = 0;
                }

                // send the tail
                if (send_batch_buf_.header->currentItemNum != 0) {
                    phase_sender_obj_->SendBatch(&send_batch_buf_);
                    send_batch_buf_.header->dataSize = 0;
                    send_batch_buf_.header->currentItemNum = 0;
                }

                // send the end flag
                send_batch_buf_.header->messageType = FILE_END_UNI_CHUNK_FP;
                // this->SendBatch(send_batch_buf_);
                phase_sender_obj_->SendBatch(&send_batch_buf_);

                cout<<"stream2 process time "<<stream2_process_time_<<endl;

                // job_done = true;
            }
        }

        if (job_done) {
            break;
        }
    }

    // do not deal with tail (tails have been handle in FILE_END)

    // send the end flag
    send_batch_buf_.header->messageType = SYNC_UNI_CHUNK_FP_END_FLAG;
    // this->SendBatch(send_batch_buf_);
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    tool::Logging(my_name_.c_str(), "StreamPhase2Thd exits.\n");

    return ;
}

/**
 * @brief process one batch of fp list
 * 
 */
void StreamPhase2Thd::ProcessOneBatch() {
    gettimeofday(&stream2_stime, NULL);

    // cout<<"recv fp num "<<process_batch_buf_.header->currentItemNum<<endl;

    size_t uni_fp_num = 0;

    // do ecall: input is recv FP list, output is uni_fp_list
// #if (RECOVER_CHECK == 0)
// #if (NAIVE_CACHE == 1)
//     Ecall_Stream_Phase2_ProcessBatch_NaiveStreamCache(sgx_eid_, process_batch_buf_.dataBuffer, 
//         process_batch_buf_.header->currentItemNum,
//         process_batch_buf_.dataBuffer, &uni_fp_num, 
//         &req_container_);
// #endif

// #if (BATCH_CACHE == 1)
//     Ecall_Stream_Phase2_ProcessBatch_BatchStreamCache(sgx_eid_, process_batch_buf_.dataBuffer, 
//         process_batch_buf_.header->currentItemNum,
//         process_batch_buf_.dataBuffer, &uni_fp_num, 
//         &out_chunk_query_, &batch_req_containers_);
// #endif        

// #endif

// #if (RECOVER_CHECK == 1)

// #if (NAIVE_CACHE == 1)
//     Ecall_Stream_Phase2_ProcessBatch_NaiveStreamCache_Debug(sgx_eid_, process_batch_buf_.dataBuffer, 
//         process_batch_buf_.header->currentItemNum,
//         process_batch_buf_.dataBuffer, &uni_fp_num, 
//         &req_container_, &debug_out_query_);
// #endif

// #endif

    this->ProcessPlainBatch(process_batch_buf_.dataBuffer, 
        process_batch_buf_.header->currentItemNum,
        process_batch_buf_.dataBuffer, &uni_fp_num, 
        &req_container_, &debug_out_query_);
    
    process_batch_buf_.header->currentItemNum = uni_fp_num;
    
    // check the uni num
    // cout<<"uni fp num "<<process_batch_buf_.header->currentItemNum<<endl;

    // prepare send buf
    process_batch_buf_.header->dataSize = process_batch_buf_.header->currentItemNum * CHUNK_HASH_HMAC_SIZE;
    // // this->SendBatch(send_batch_buf_);
    // phase_sender_obj_->SendBatch(&send_batch_buf_);
    // // reset
    // send_batch_buf_.header->dataSize = 0;
    // send_batch_buf_.header->currentItemNum = 0;

    gettimeofday(&stream2_etime, NULL);
    stream2_process_time_ += tool::GetTimeDiff(stream2_stime, stream2_etime);

    return ;
}

/**
 * @brief Set the Done Flag object
 * 
 */
void StreamPhase2Thd::SetDoneFlag() {
    inputMQ_->done_ = true;
    return ;
}

/**
 * @brief send one batch of fp list
 * 
 */
void StreamPhase2Thd::SendOneBatch() {
    // if (send_batch_buf_.header->currentItemNum == sync_config.GetMetaBatchSize()) {
    //     // send the batch
    //       send_batch_buf_->header->messageType = SYNC_UNI_CHUNK_FP;
    //     phase_sender_obj_->SendBatch(&send_batch_buf_);
    //     // reset
    //     send_batch_buf_.header->dataSize = 0;
    //     send_batch_buf_.header->currentItemNum = 0;
    // }

    uint32_t remaining_item_num = sync_config.GetMetaBatchSize() - 
        send_batch_buf_.header->currentItemNum;
    
    if (remaining_item_num >= process_batch_buf_.header->currentItemNum) {
        // case-1: the remaining send buf is enough for processed data
        memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize,
            process_batch_buf_.dataBuffer, process_batch_buf_.header->dataSize);
        send_batch_buf_.header->dataSize += process_batch_buf_.header->dataSize;
        send_batch_buf_.header->currentItemNum += process_batch_buf_.header->currentItemNum;
    }
    else {
        // case-2: the remaining send buf is not enough for processed data
        memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize,
            process_batch_buf_.dataBuffer, remaining_item_num * CHUNK_HASH_HMAC_SIZE);
        send_batch_buf_.header->dataSize += remaining_item_num * CHUNK_HASH_HMAC_SIZE;
        send_batch_buf_.header->currentItemNum += remaining_item_num;
        // record the sent offset for process batch for next send
        sent_offset_ = remaining_item_num * CHUNK_HASH_HMAC_SIZE;

        // the send buf is full, send
        send_batch_buf_.header->messageType = SYNC_UNI_CHUNK_FP;
        phase_sender_obj_->SendBatch(&send_batch_buf_);
        // reset
        send_batch_buf_.header->dataSize = 0;
        send_batch_buf_.header->currentItemNum = 0;

        // copy the remaining data in process batch to send batch
        uint32_t remaining_data_size = process_batch_buf_.header->dataSize - sent_offset_;
        memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize,
            process_batch_buf_.dataBuffer + sent_offset_, 
            remaining_data_size);
        send_batch_buf_.header->dataSize += remaining_data_size;
        send_batch_buf_.header->currentItemNum += remaining_data_size / CHUNK_HASH_HMAC_SIZE;
        sent_offset_ = 0;
    }


    if (send_batch_buf_.header->currentItemNum == sync_config.GetMetaBatchSize()) {
        // send the batch
        send_batch_buf_.header->messageType = SYNC_UNI_CHUNK_FP;
        phase_sender_obj_->SendBatch(&send_batch_buf_);
        // reset
        send_batch_buf_.header->dataSize = 0;
        send_batch_buf_.header->currentItemNum = 0;
    }

    return ;
}

/**
 * @brief process the plain batch
 * 
 * @param fp_list 
 * @param fp_num 
 * @param uni_fp_list 
 * @param uni_fp_num 
 * @param req_container 
 * @param debug_out_query 
 */
void StreamPhase2Thd::ProcessPlainBatch(uint8_t* fp_list, size_t fp_num,
    uint8_t* uni_fp_list, size_t* uni_fp_num, Container_t* req_container,
    OutChunkQuery_t* debug_out_query) {

    // SyncEnclave::Logging(my_name_.c_str(), "in ecallStreamChunkIndex.\n");
    // decrypt the fp list with session key
    // SyncEnclave::Logging("fp_num ", "%d\n", fp_num);
    // crypto_util_->DecryptWithKey(cipher_ctx_, fp_list, fp_num * CHUNK_HASH_HMAC_SIZE,
    //     session_key_, plain_uni_fp_list_);
    memcpy(plain_uni_fp_list_, fp_list, fp_num * CHUNK_HASH_HMAC_SIZE);

    // SyncEnclave::Logging("", "decrypted fp list.\n");

    // // prepare the req container
    // uint8_t* id_buf = req_container->idBuffer;
    // uint8_t** container_array = req_container->containerArray;
    // unordered_map<string, uint32_t> tmp_container_map;
    // tmp_container_map.reserve(CONTAINER_CAPPING_VALUE);

    // record the current reuse pos in plain_uni_fp_list_
    uint32_t cur_reuse_offset = 0;

    // // prepare the out query
    // OutChunkQueryEntry_t* tmp_query_entry = out_chunk_query->OutChunkQueryBase;
    // uint32_t out_query_num = 0;

    // for outquery
    OutChunkQueryEntry_t out_query_entry;
    uint32_t global_uni_num = 0;
    uint32_t enclave_uni_num = 0;

    OutChunkQueryEntry_t* tmp_debug_query_entry = debug_out_query->OutChunkQueryBase;

    // check the fp index in enclave cache
    for (size_t i = 0; i < fp_num; i++) {
        string fp_str;
        fp_str.assign((char*)plain_uni_fp_list_ + i * CHUNK_HASH_HMAC_SIZE, CHUNK_HASH_HMAC_SIZE);
        string req_containerID;
        uint64_t req_features[SUPER_FEATURE_PER_CHUNK];

        // // debug
        // uint8_t check_fp[CHUNK_HASH_HMAC_SIZE];
        // memcpy(check_fp, (uint8_t*)&fp_str[0], CHUNK_HASH_HMAC_SIZE);
        // SyncEnclave::Logging("check fp: ", "\n");
        // Ocall_PrintfBinary(check_fp, CHUNK_HASH_HMAC_SIZE);

        // tool::Logging("check the fp", "%d\n", i);
        // tool::PrintBinaryArray((uint8_t*)fp_str.c_str(), CHUNK_HASH_HMAC_SIZE);

        // SyncEnclave::Logging("before enclave cache query", "\n");

        if (!enclave_locality_cache_->QueryFP(fp_str, req_containerID, req_features)) {
            // not exist in enclave cache (cache unique)
            enclave_uni_num ++;
            _enclave_unique_num ++;

            // debug
            // SyncEnclave::Logging("enclave unique: ", "%d\n", enclave_uni_num);
            // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_HMAC_SIZE);

            // // quary the out index immediately without batching
            // crypto_util_->IndexAESCMCEnc(cipher_ctx_, (uint8_t*)&fp_str[0],
            //     CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, out_query_entry.chunkHash);
            memcpy(out_query_entry.chunkHash, (uint8_t*)&fp_str[0], CHUNK_HASH_HMAC_SIZE);

            sync_io_->SyncIO_SingleQueryChunkIndex((void*)&out_query_entry);

            if (out_query_entry.dedupFlag == DUPLICATE) {
                // case-1: global duplicate: load the container in enclave
                // SyncEnclave::Logging("global duplicate: ", "load container in cache.\n");
                _global_duplicate_num ++;

                // TODO: reduce the following redundant dec later
                // decrypt the value
                RecipeEntry_t dec_query_entry;
                // crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&out_query_entry.value, 
                //     sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
                //         (uint8_t*)&dec_query_entry);
                memcpy((uint8_t*)&dec_query_entry, (uint8_t*)&out_query_entry.value, sizeof(RecipeEntry_t));
                // get the corresponding container id for this duplicate chunk
                memcpy(req_container->containerID, dec_query_entry.containerName, CONTAINER_ID_LENGTH);
                // ????
                // Ocall_PrintfBinary((uint8_t*)req_container->containerID, CONTAINER_ID_LENGTH);
                // Ocall_PrintfBinary(dec_query_entry.containerName, CONTAINER_ID_LENGTH);

                sync_io_->SyncIO_SingleGetReqContainer((void*)req_container);

                if (req_container->currentSize != 0) {
                    // insert the enclave cache
                    // SyncEnclave::Logging("before insert reqcont ", "%d,%d",req_container->currentSize, req_container->currentMetaSize);
                    // enclave_locality_cache_->InsertCache(req_containerID, req_container->body,
                    //     MAX_CONTAINER_SIZE + MAX_META_SIZE);
                    enclave_locality_cache_->InsertCache(req_container);
                }
                else {
                    // debug
                    // tool::Logging("reqContainer curSize = 0", "\n");
                }
            }
            else {
                // case-2: global unique: first send it in plain_uni_list, write the chunk in container later
                // SyncEnclave::Logging("global unique", "\n");

                memcpy(plain_uni_fp_list_ + cur_reuse_offset, (uint8_t*)&fp_str[0], CHUNK_HASH_HMAC_SIZE);
                cur_reuse_offset += CHUNK_HASH_HMAC_SIZE;

                // SyncEnclave::Logging("global unique: ", "%d\n", global_uni_num + 1);
                // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_HMAC_SIZE);

                // TODO: insert the entry with unique chunkhash + null addr
                

                global_uni_num ++;
                _global_unique_num ++;

                // TODO: prepare the unique chunk fp list here for debugging
                memcpy(tmp_debug_query_entry->chunkHash, (uint8_t*)&fp_str[0], CHUNK_HASH_HMAC_SIZE);
                tmp_debug_query_entry ++;
            }



            // // step-1: Ocall to load container
            // auto find_cont = tmp_container_map.find(req_containerID);
            // if (find_cont == tmp_container_map.end()) {
            //     // unique container for load buffer
            //     tmp_container_map[req_containerID] = req_container->idNum;
            //     memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
            //         req_containerID.c_str(), CONTAINER_ID_LENGTH);
            //     req_container->idNum++;
            // }

            // if (req_container->idNum == CONTAINER_CAPPING_VALUE) {
            //     // fetch containers according to load buffer
            //     Ocall_SyncGetReqContainer((void*)req_container);

            //     // step-2: insert the enclave cache
            //     for (size_t j = 0; j < req_container->idNum; j++) {
            //         string insert_containerID;
            //         insert_containerID.assign((char*)id_buf + j * CONTAINER_ID_LENGTH,
            //             CONTAINER_ID_LENGTH);
                    
            //         enclave_locality_cache_->InsertCache(insert_containerID, container_array[j],
            //             MAX_CONTAINER_SIZE + MAX_META_SIZE);
            //     }

            //     // reset
            //     req_container->idNum = 0;
            //     tmp_container_map.clear();
            // }

            // // // step-3: prepare the out query
            // // crypto_util_->IndexAESCMCEnc(cipher_ctx_, (uint8_t*)&fp_str[0],
            // //     CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
            // // tmp_query_entry ++;

            // // step-3: prepare the send buffer
            // memcpy(plain_uni_fp_list_ + cur_reuse_offset, fp_str.c_str(), CHUNK_HASH_HMAC_SIZE);
            // cur_reuse_offset += CHUNK_HASH_HMAC_SIZE;
        }
        else {
            // a duplicate chunk to enclave cache
            // SyncEnclave::Logging("enclave duplicate ", "\n");
            _enclave_duplicate_num ++;
        }
    }

    // // deal with tail
    // if (req_container->idNum != 0) {
    //     // fetch containers according to load buffer
    //     Ocall_SyncGetReqContainer((void*)req_container);

    //     // step-2: insert the enclave cache
    //     for (size_t j = 0; j < req_container->idNum; j++) {
    //         string insert_containerID;
    //         insert_containerID.assign((char*)id_buf + j * CONTAINER_ID_LENGTH,
    //             CONTAINER_ID_LENGTH);
            
    //         enclave_locality_cache_->InsertCache(insert_containerID, container_array[j],
    //             MAX_CONTAINER_SIZE + MAX_META_SIZE);
    //     }

    //     // reset
    //     req_container->idNum = 0;
    //     tmp_container_map.clear();
    // }

    // // issue out query
    // out_chunk_query->queryNum = out_query_num;

    // encrypt the output uni fp list
    // debug
    // crypto_util_->EncryptWithKey(cipher_ctx_, plain_uni_fp_list_, cur_reuse_offset, 
    //     session_key_, uni_fp_list);
    memcpy(uni_fp_list, plain_uni_fp_list_, cur_reuse_offset);
    (*uni_fp_num) = global_uni_num;

    debug_out_query->queryNum = global_uni_num;

    sync_io_->SyncIO_InsertDebugIndex((void*)debug_out_query);

    // // debug: check the uni fp list
    // uint32_t checkoffset = 0;
    // SyncEnclave::Logging("check uni fp list", "\n");
    // for (size_t i = 0; i < global_uni_num; i++) {
    //     uint8_t check_uni_fp[CHUNK_HASH_HMAC_SIZE];
    //     memcpy(check_uni_fp, plain_uni_fp_list_ + checkoffset, CHUNK_HASH_HMAC_SIZE);
    //     checkoffset += CHUNK_HASH_HMAC_SIZE;
    //     Ocall_PrintfBinary(check_uni_fp, CHUNK_HASH_HMAC_SIZE);
    // }

    // SyncEnclave::Logging("after enc batch", "\n");

    return ;
}

/**
 * @brief insert the batch into send MQ
 * 
 * @param send_buf 
 */
// void StreamPhase2Thd::SendBatch(SendMsgBuffer_t& send_buf) {
//     uint8_t flag = META_POOL;
//     while (true) {
//         if (send_thd_obj_->InsertSendMQ(flag, send_buf)) {
//             break;
//         }
//     }

//     return ;
// }
