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

#if (PHASE_BREAKDOWN == 1)
struct timeval phase2_stime;
struct timeval phase2_etime;
#endif

/**
 * @brief Construct a new Stream Phase 2 Thd:: Stream Phase 2 Thd object
 * 
 * @param phase_sender_obj 
 * @param inputMQ 
 * @param out_fp_db 
 * @param eid_sgx 
 */
StreamPhase2Thd::StreamPhase2Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase1MQ_t>* inputMQ,
    AbsDatabase* out_fp_db, sgx_enclave_id_t eid_sgx) {
    
    phase_sender_obj_ = phase_sender_obj;
    inputMQ_ = inputMQ;
    out_chunk_db_ = out_fp_db;
    sgx_eid_ = eid_sgx;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * CHUNK_HASH_SIZE * sizeof(uint8_t));
    send_batch_buf_.dataBuffer = send_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    send_batch_buf_.header = (NetworkHead_t*) send_batch_buf_.sendBuffer;
    send_batch_buf_.header->currentItemNum = 0;
    send_batch_buf_.header->dataSize = 0;
    // to record the offset in process batch
    offset_of_remaining_process_buf_ = 0;

    // for process batch
    process_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * CHUNK_HASH_SIZE * sizeof(uint8_t));
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

    // tool::Logging(my_name_.c_str(), "init StreamPhase2Thd.\n");
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
}

/**
 * @brief the main process
 * 
 */
void StreamPhase2Thd::Run() {
    bool job_done = false;
    StreamPhase1MQ_t tmp_chunk_hash;

    // tool::Logging(my_name_.c_str(), "the main thread is running.\n");

    while (true) {

        if (inputMQ_->done_ && inputMQ_->IsEmpty()) {
            job_done = true;
        }

        if (inputMQ_->Pop(tmp_chunk_hash)) {
            if (tmp_chunk_hash.is_file_end == NOT_FILE_END) {
                // cout<<"pop chunk hash"<<endl;
                memcpy(process_batch_buf_.dataBuffer + process_batch_buf_.header->dataSize,
                    tmp_chunk_hash.chunkHash, CHUNK_HASH_SIZE);
                process_batch_buf_.header->dataSize += CHUNK_HASH_SIZE;
                process_batch_buf_.header->currentItemNum ++;

                if (process_batch_buf_.header->currentItemNum == sync_config.GetMetaBatchSize()) {
                    ProcessOneBatch();

                    SendOneBatch();

                    // cout<<"after sendonebatch"<<endl;

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

                    // cout<<"after sendonebatch (file end)"<<endl;

                    // send the tail
                    if (send_batch_buf_.header->currentItemNum != 0) {
                        phase_sender_obj_->SendBatch(&send_batch_buf_);
                        // tool::Logging("phase2 sent tail batch size", "%d\n", send_batch_buf_.header->currentItemNum);
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
                phase_sender_obj_->SendBatch(&send_batch_buf_);

#if (PHASE_BREAKDOWN == 1)
                tool::Logging(my_name_.c_str(), "Process time for phase 2: %f.\n", _phase2_process_time);
#endif                

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
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    // tool::Logging(my_name_.c_str(), "StreamPhase2Thd exits.\n");

    return ;
}

/**
 * @brief process one batch of fp list
 * 
 */
void StreamPhase2Thd::ProcessOneBatch() {
#if (PHASE_BREAKDOWN == 1)    
    gettimeofday(&phase2_stime, NULL);
#endif
    // cout<<"recv fp num "<<process_batch_buf_.header->currentItemNum<<endl;

    size_t uni_fp_num = 0;

    // OutChunkQueryEntry_t single_out_query;
    OutChunkQueryEntry_t* single_out_query = new OutChunkQueryEntry_t;

    // do ecall: input is recv FP list, output is uni_fp_list
#if (RECOVER_CHECK == 0)
    Ecall_Stream_Phase2_ProcessBatch_NaiveStreamCache(sgx_eid_, process_batch_buf_.dataBuffer, 
        process_batch_buf_.header->currentItemNum,
        process_batch_buf_.dataBuffer, &uni_fp_num, 
        &req_container_);
#endif

#if (RECOVER_CHECK == 1)
    Ecall_Stream_Phase2_ProcessBatch_NaiveStreamCache_Debug(sgx_eid_, process_batch_buf_.dataBuffer, 
        process_batch_buf_.header->currentItemNum,
        process_batch_buf_.dataBuffer, &uni_fp_num, 
        &req_container_, &debug_out_query_, single_out_query);
#endif
    
    process_batch_buf_.header->currentItemNum = uni_fp_num;
    
    // check the uni num
    // cout<<"uni fp num "<<process_batch_buf_.header->currentItemNum<<endl;

    // prepare send buf
    process_batch_buf_.header->dataSize = process_batch_buf_.header->currentItemNum * CHUNK_HASH_SIZE;

#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase2_etime, NULL);
    _phase2_process_time += tool::GetTimeDiff(phase2_stime, phase2_etime);
#endif

    delete single_out_query;

    // // debug: check the uni fp list
    // for (size_t i = 0; i < process_batch_buf_.header->currentItemNum; i++) {
    //     uint8_t check_uni_fp[CHUNK_HASH_SIZE];
    //     memcpy(check_uni_fp, process_batch_buf_.dataBuffer + i * CHUNK_HASH_SIZE, CHUNK_HASH_SIZE);
    //     tool::Logging("check uni processbuf fp", "\n");
    //     tool::PrintBinaryArray(check_uni_fp, CHUNK_HASH_SIZE);
    // }

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
    uint32_t remaining_item_num = sync_config.GetMetaBatchSize() - 
        send_batch_buf_.header->currentItemNum;
    
    if (process_batch_buf_.header->currentItemNum == 0) {
        // nothing to send
        cout<<"nothing to send"<<endl;
        return ;
    }

    if (remaining_item_num >= process_batch_buf_.header->currentItemNum) {
        // case-1: the remaining send buf is enough for processed data

        // cout<<"send buf already has "<<send_batch_buf_.header->currentItemNum<<"; remaining "<<remaining_item_num<<endl;
        // cout<<"process buf has "<<process_batch_buf_.header->currentItemNum<<endl;

        memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize,
            process_batch_buf_.dataBuffer, process_batch_buf_.header->dataSize);
        send_batch_buf_.header->dataSize += process_batch_buf_.header->dataSize;
        send_batch_buf_.header->currentItemNum += process_batch_buf_.header->currentItemNum;

        // reset offset
        offset_of_remaining_process_buf_ = 0;
    }
    else {
        // case-2: the remaining send buf is not enough for processed data
        memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize,
            process_batch_buf_.dataBuffer, remaining_item_num * CHUNK_HASH_SIZE);
        send_batch_buf_.header->dataSize += remaining_item_num * CHUNK_HASH_SIZE;
        send_batch_buf_.header->currentItemNum += remaining_item_num;
        // record the sent offset for process batch for next send
        sent_offset_ = remaining_item_num * CHUNK_HASH_SIZE;
        // cout<<"process buf sent offset "<<sent_offset_<<endl;

        // the send buf is full, send
        send_batch_buf_.header->messageType = SYNC_UNI_CHUNK_FP;
        phase_sender_obj_->SendBatch(&send_batch_buf_);

        // tool::Logging("phase2 sent batch size", "%d\n", send_batch_buf_.header->currentItemNum);

        // reset
        send_batch_buf_.header->dataSize = 0;
        send_batch_buf_.header->currentItemNum = 0;

        // copy the remaining data in process batch to send batch
        // cout<<"process buf offset "<<sent_offset_<<endl;
        uint32_t remaining_data_size = process_batch_buf_.header->dataSize - sent_offset_;
        memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize,
            process_batch_buf_.dataBuffer + sent_offset_, 
            remaining_data_size);
        send_batch_buf_.header->dataSize += remaining_data_size;
        send_batch_buf_.header->currentItemNum += remaining_data_size / CHUNK_HASH_SIZE;
        // sent_offset_ = 0;
    }


    if (send_batch_buf_.header->currentItemNum == sync_config.GetMetaBatchSize()) {
        // send the batch
        send_batch_buf_.header->messageType = SYNC_UNI_CHUNK_FP;
        phase_sender_obj_->SendBatch(&send_batch_buf_);
        // tool::Logging("phase2 sent batch size", "%d\n", send_batch_buf_.header->currentItemNum);

        // reset
        send_batch_buf_.header->dataSize = 0;
        send_batch_buf_.header->currentItemNum = 0;
    }

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