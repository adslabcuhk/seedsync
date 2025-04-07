/**
 * @file stream_phase_4_thd.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/stream_phase_4_thd.h"

#if (PHASE_BREAKDOWN == 1)
struct timeval phase4_stime;
struct timeval phase4_etime;
#endif

/**
 * @brief Construct a new Stream Phase 4 Thd:: Stream Phase 4 Thd object
 * 
 * @param phase_sender_obj 
 * @param inputMQ 
 * @param eid_sgx 
 */
StreamPhase4Thd::StreamPhase4Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase3MQ_t>* inputMQ,
    sgx_enclave_id_t eid_sgx) {
    
    // send_thd_obj_ = send_thd_obj;
    phase_sender_obj_ = phase_sender_obj;
    inputMQ_ = inputMQ;
    sgx_eid_ = eid_sgx;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sync_config.GetDataBatchSize() * sizeof(StreamPhase4MQ_t));
    send_batch_buf_.dataBuffer = send_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    send_batch_buf_.header = (NetworkHead_t*) send_batch_buf_.sendBuffer;
    send_batch_buf_.header->currentItemNum = 0;
    send_batch_buf_.header->dataSize = 0;

    // tool::Logging(my_name_.c_str(), "init StreamPhase4Thd.\n");
}

/**
 * @brief Destroy the Stream Phase 4 Thd object
 * 
 */
StreamPhase4Thd::~StreamPhase4Thd() {
    free(send_batch_buf_.sendBuffer);
}

/**
 * @brief the main process
 * 
 */
void StreamPhase4Thd::Run() {
    bool job_done = false;
    StreamPhase3MQ_t tmp_entry;

    // tool::Logging(my_name_.c_str(), "the main thread is running.\n");

    while (true) {
        
        if (inputMQ_->done_ && inputMQ_->IsEmpty()) {
            job_done = true;
        }

        if (inputMQ_->Pop(tmp_entry)) {
            if (tmp_entry.is_file_end == NOT_FILE_END) {
                // cout<<"pop features"<<endl;
                memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize, 
                    tmp_entry.features, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                send_batch_buf_.header->dataSize += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize,
                    tmp_entry.chunkHash, CHUNK_HASH_SIZE);
                send_batch_buf_.header->dataSize += CHUNK_HASH_SIZE;
                send_batch_buf_.header->currentItemNum ++;

                if (send_batch_buf_.header->currentItemNum == sync_config.GetDataBatchSize()) {
                    ProcessOneBatch();
                    send_batch_buf_.header->currentItemNum = 0;
                    send_batch_buf_.header->dataSize = 0;
                }
                
            } 
            else if (tmp_entry.is_file_end == FILE_END) {
                // cout<<"file end phase-4"<<endl;

                if (send_batch_buf_.header->currentItemNum != 0) {
                    ProcessOneBatch();
                    send_batch_buf_.header->currentItemNum = 0;
                    send_batch_buf_.header->dataSize = 0;
                }

                // send the file end flag
                send_batch_buf_.header->messageType = FILE_END_BASE_HASH;
                phase_sender_obj_->SendBatch(&send_batch_buf_);

#if (PHASE_BREAKDOWN == 1)
                tool::Logging(my_name_.c_str(), "Process time for phase 4: %f.\n", _phase4_process_time);
#endif
                // job_done = true;
            }
        }

        if (job_done) {
            break;
        }
    }

    // send the end flag
    send_batch_buf_.header->messageType = SYNC_BASE_HASH_END_FLAG;
    // this->SendBatch(send_batch_buf_);
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    // tool::Logging(my_name_.c_str(), "StreamPhase4Thd exits.\n");

    return ;
}

/**
 * @brief process one batch of uni fp list, return the features
 * 
 */
void StreamPhase4Thd::ProcessOneBatch() {
#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase4_stime, NULL);
#endif

    // ecall
    uint32_t out_item_num = 0;
    Ecall_Stream_Phase4_ProcessBatch(sgx_eid_, send_batch_buf_.dataBuffer, 
        send_batch_buf_.header->currentItemNum, 
        send_batch_buf_.dataBuffer, 
        out_item_num);

    // tmp fix
    out_item_num = send_batch_buf_.header->currentItemNum;

    // prepare the send buf
    send_batch_buf_.header->messageType = SYNC_BASE_HASH;
    send_batch_buf_.header->dataSize = out_item_num * sizeof(StreamPhase4MQ_t);
    phase_sender_obj_->SendBatch(&send_batch_buf_);
    // reset
    send_batch_buf_.header->dataSize = 0;
    send_batch_buf_.header->currentItemNum = 0;


#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase4_etime, NULL);
    _phase4_process_time += tool::GetTimeDiff(phase4_stime, phase4_etime);
#endif

    return ;
}

/**
 * @brief Set the Done Flag object
 * 
 */
void StreamPhase4Thd::SetDoneFlag() {
    inputMQ_->done_ = true;
    return ;
}

/**
 * @brief insert the batch into send MQ
 * 
 * @param send_buf 
 */
// void StreamPhase4Thd::SendBatch(SendMsgBuffer_t& send_buf) {
//     uint8_t flag = META_POOL;
//     while (true) {
//         if (send_thd_obj_->InsertSendMQ(flag, send_buf)) {
//             break;
//         }
//     }

//     return ;
// }