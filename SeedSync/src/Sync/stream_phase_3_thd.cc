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

#if (PHASE_BREAKDOWN == 1)
struct timeval phase3_stime;
struct timeval phase3_etime;
#endif

/**
 * @brief Construct a new Stream Phase 3 Thd:: Stream Phase 3 Thd object
 * 
 * @param phase_sender_obj 
 * @param inputMQ 
 * @param out_fp_db 
 * @param eid_sgx 
 */
StreamPhase3Thd::StreamPhase3Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase2MQ_t>* inputMQ,
    AbsDatabase* out_fp_db, sgx_enclave_id_t eid_sgx) {
    
    phase_sender_obj_ = phase_sender_obj;
    inputMQ_ = inputMQ;
    out_chunk_db_ = out_fp_db;
    sgx_eid_ = eid_sgx;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
    // tool::Logging(my_name_.c_str(), "stream phase3 size %d\n", sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
    send_batch_buf_.dataBuffer = send_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    send_batch_buf_.header = (NetworkHead_t*) send_batch_buf_.sendBuffer;
    send_batch_buf_.header->currentItemNum = 0;
    send_batch_buf_.header->dataSize = 0;

    recv_buf_ = (uint8_t*)malloc(sync_config.GetMetaBatchSize() * (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
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

    // tool::Logging(my_name_.c_str(), "init StreamPhase3Thd.\n");
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
}

/**
 * @brief the main process
 * 
 */
void StreamPhase3Thd::Run() {
    bool job_done = false;
    StreamPhase2MQ_t tmp_uni_hash;

    // tool::Logging(my_name_.c_str(), "the main thread is running.\n");

    while (true) {

        if (inputMQ_->done_ && inputMQ_->IsEmpty()) {
            job_done = true;
        }

        if (inputMQ_->Pop(tmp_uni_hash)) {
            if (tmp_uni_hash.is_file_end == NOT_FILE_END) {
                // cout<<"pop uni hash"<<endl;
                memcpy(recv_buf_ + recv_offset_, tmp_uni_hash.chunkHash, CHUNK_HASH_SIZE);
                recv_offset_ += CHUNK_HASH_SIZE;
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
                phase_sender_obj_->SendBatch(&send_batch_buf_);

#if (PHASE_BREAKDOWN == 1)
                tool::Logging(my_name_.c_str(), "Process time for phase 3: %f.\n", _phase3_process_time);
#endif                
                job_done = true;
                 
            }
        }

        if (job_done) {
            break;
        }
    }

    // send the end flag
    send_batch_buf_.header->messageType = SYNC_FEATURE_END;
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    // tool::Logging(my_name_.c_str(), "StreamPhase3Thd exits.\n");

    return ;
}

/**
 * @brief process one batch of uni fp list, return the features
 * 
 */
void StreamPhase3Thd::ProcessOneBatch() {
#if (PHASE_BREAKDOWN == 1)    
    gettimeofday(&phase3_stime, NULL);
#endif    

    // reset
    req_containers_.idNum = 0;

    // do ecall: input is recv uni FP list, output is [features + fps]
    send_batch_buf_.header->currentItemNum = recv_num_;
    Ecall_Stream_Phase3_ProcessBatch(sgx_eid_, recv_buf_, recv_num_,
        &req_containers_, &out_chunk_query_, 
        send_batch_buf_.dataBuffer, send_batch_buf_.header->currentItemNum);
    
    // cout<<"feature header "<<send_batch_buf_.header->currentItemNum<<endl;

    // prepare the send buf
    send_batch_buf_.header->messageType = SYNC_FEATURE;
    send_batch_buf_.header->dataSize = send_batch_buf_.header->currentItemNum * (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
    phase_sender_obj_->SendBatch(&send_batch_buf_);
    // reset
    send_batch_buf_.header->dataSize = 0;
    send_batch_buf_.header->currentItemNum = 0;

#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase3_etime, NULL);
    _phase3_process_time += tool::GetTimeDiff(phase3_stime, phase3_etime);
#endif

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