/**
 * @file stream_phase_5_thd.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/stream_phase_5_thd.h"

#if (PHASE_BREAKDOWN == 1)
struct timeval phase5_stime;
struct timeval phase5_etime;
#endif

/**
 * @brief Construct a new Stream Phase 5 Thd:: Stream Phase 5 Thd object
 * 
 * @param phase_sender_obj 
 * @param inputMQ 
 * @param out_fp_db 
 * @param eid_sgx 
 */
StreamPhase5Thd::StreamPhase5Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase4MQ_t>* inputMQ, 
    AbsDatabase* out_fp_db, sgx_enclave_id_t eid_sgx) {
    
    phase_sender_obj_ = phase_sender_obj;
    inputMQ_ = inputMQ;
    out_fp_db_ = out_fp_db;
    sgx_eid_ = eid_sgx;

    // for recv batch
    recv_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * sizeof(StreamPhase4MQ_t));
    recv_batch_buf_.dataBuffer = recv_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    recv_batch_buf_.header = (NetworkHead_t*) recv_batch_buf_.sendBuffer;
    recv_batch_buf_.header->currentItemNum = 0;
    recv_batch_buf_.header->dataSize = 0;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetDataBatchSize() * (MAX_CHUNK_SIZE + sizeof(uint32_t) + sizeof(uint8_t) + CHUNK_HASH_SIZE) * sizeof(uint8_t));
    // cout << "phase-5 allocate " << sizeof(NetworkHead_t) + sync_config.GetDataBatchSize() * (MAX_CHUNK_SIZE + sizeof(uint32_t) + sizeof(uint8_t) + CHUNK_HASH_SIZE) << endl;
    send_batch_buf_.dataBuffer = send_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    send_batch_buf_.header = (NetworkHead_t*) send_batch_buf_.sendBuffer;
    send_batch_buf_.header->currentItemNum = 0;
    send_batch_buf_.header->dataSize = 0;

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

    // tool::Logging(my_name_.c_str(), "init StreamPhase5Thd.\n");
}

/**
 * @brief Destroy the Stream Phase 5 Thd object
 * 
 */
StreamPhase5Thd::~StreamPhase5Thd() {
    free(recv_batch_buf_.sendBuffer);
    free(send_batch_buf_.sendBuffer);
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
void StreamPhase5Thd::Run() {
    bool job_done = false;
    StreamPhase4MQ_t tmp_entry;

    // tool::Logging(my_name_.c_str(), "The main thread is running.\n");

    while (true) {
        if (inputMQ_->done_ && inputMQ_->IsEmpty()) {
            job_done = true;
        }

        if (inputMQ_->Pop(tmp_entry)) {
            if (tmp_entry.is_file_end == NOT_FILE_END) {
                // cout<<"pop chunk hash info"<<endl;
                memcpy(recv_batch_buf_.dataBuffer + recv_batch_buf_.header->dataSize, 
                    &tmp_entry, sizeof(StreamPhase4MQ_t));
                recv_batch_buf_.header->dataSize += sizeof(StreamPhase4MQ_t);
                recv_batch_buf_.header->currentItemNum ++;

                if (recv_batch_buf_.header->currentItemNum == sync_config.GetDataBatchSize()) {
                    ProcessOneBatch();
                    recv_batch_buf_.header->currentItemNum = 0;
                    recv_batch_buf_.header->dataSize = 0;
                    send_batch_buf_.header->currentItemNum = 0;
                    send_batch_buf_.header->dataSize = 0;
                }

            }
            else if (tmp_entry.is_file_end == FILE_END) {
                // cout<<"file end phase-5"<<endl;

                if (recv_batch_buf_.header->currentItemNum != 0) {
                    ProcessOneBatch();
                    recv_batch_buf_.header->currentItemNum = 0;
                    recv_batch_buf_.header->dataSize = 0;
                    send_batch_buf_.header->currentItemNum = 0;
                    send_batch_buf_.header->dataSize = 0;
                }

                // send the end flag
                send_batch_buf_.header->messageType = FILE_END_SYNC_DATA;
                phase_sender_obj_->SendBatch(&send_batch_buf_);

#if (PHASE_BREAKDOWN == 1)
                tool::Logging(my_name_.c_str(), "Process time for phase 5: %f.\n", _phase5_process_time);
#endif   

                sleep(10);
                
                job_done = true;
            }
        }

        if (job_done) {
            break;
        }
    }

    // send the end flag
    send_batch_buf_.header->messageType = SYNC_DATA_END_FLAG;
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    // tool::Logging(my_name_.c_str(), "StreamPhase5Thd exits.\n");

    return ;
}

/**
 * @brief process one batch of uni fp list, return the features
 * 
 */
void StreamPhase5Thd::ProcessOneBatch() {
#if (PHASE_BREAKDOWN == 1)    
    gettimeofday(&phase5_stime, NULL);
#endif    
    // do ecall
    Ecall_Stream_Phase5_ProcessBatch(sgx_eid_, recv_batch_buf_.dataBuffer, 
        recv_batch_buf_.header->dataSize, &req_containers_, &out_chunk_query_, 
        send_batch_buf_.dataBuffer, &send_batch_buf_.header->dataSize);

    // try parallel ecall
#if (PARALLEL_ECALL == 1)
    uint64_t half_size = send_batch_buf_.header->dataSize / 2;

    // execute the first half
    std::future<_status_t> batch_1 = std::async(Ecall_Stream_Phase5_ProcessBatch, sgx_eid_, send_batch_buf_.dataBuffer, 
        send_batch_buf_.header->dataSize, &req_containers_, &out_chunk_query_, 
        send_batch_buf_.dataBuffer, &send_batch_buf_.header->dataSize);
    
    // execute the second half
    std::future<_status_t> batch_2 = std::async(Ecall_Stream_Phase5_ProcessBatch, sgx_eid_, send_batch_buf_.dataBuffer,
        send_batch_buf_.header->dataSize, &req_containers_, &out_chunk_query_,
        send_batch_buf_.dataBuffer, &send_batch_buf_.header->dataSize);

    // wait for both sub-batch to finish
    _status_t status_1 = batch_1.get();
    _status_t status_2 = batch_2.get();
#endif

    // prepare the send batch
    send_batch_buf_.header->messageType = SYNC_DATA;
    phase_sender_obj_->SendBatch(&send_batch_buf_);

#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase5_etime, NULL);
    _phase5_process_time += tool::GetTimeDiff(phase5_stime, phase5_etime);
#endif

    return ;    
}

/**
 * @brief insert the batch into send MQ
 * 
 * @param send_buf 
 */
// void StreamPhase5Thd::SendBatch(SendMsgBuffer_t& send_buf) {
//     uint8_t flag = DATA_POOL;
//     while (true) {
//         if (send_thd_obj_->InsertSendMQ(flag, send_buf)) {
//             break;
//         }
//     }

//     return ;
// }