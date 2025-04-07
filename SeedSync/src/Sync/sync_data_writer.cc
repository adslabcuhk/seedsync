/**
 * @file sync_data_writer.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-03-13
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/sync_data_writer.h"

#if (PHASE_BREAKDOWN == 1)
struct timeval phase6_stime;
struct timeval phase6_etime;
#endif

/**
 * @brief Construct a new Sync Data Writer object
 * 
 * @param sgx_eid 
 * @param out_fp_db 
 */
SyncDataWriter::SyncDataWriter(sgx_enclave_id_t sgx_eid, AbsDatabase* out_fp_db) {
    sgx_eid_ = sgx_eid;
    out_fp_db_ = out_fp_db;

    // for chunk outquery
    out_chunk_query_.OutChunkQueryBase = (OutChunkQueryEntry_t*) malloc(sizeof(OutChunkQueryEntry_t) * 
        sync_config.GetDataBatchSize());
    out_chunk_query_.queryNum = 0;

    // for chunk outquery
    update_index_.OutChunkQueryBase = (OutChunkQueryEntry_t*) malloc(sizeof(OutChunkQueryEntry_t) * 
        sync_config.GetDataBatchSize());
    update_index_.queryNum = 0;

    // for debug
    debug_index_.OutChunkQueryBase = (OutChunkQueryEntry_t*) malloc(sizeof(OutChunkQueryEntry_t) * 
        sync_config.GetDataBatchSize());
    debug_index_.queryNum = 0;

    // container write buffer
    container_buf_.currentSize = 0;
    container_buf_.currentMetaSize = 0;
    tool::CreateUUID(container_buf_.containerID, CONTAINER_ID_LENGTH);
    // container_buf_.containerID = (char*) malloc(sizeof(uint8_t) * CONTAINER_ID_LENGTH);
    // tool::CreateUUID(container_buf_.containerID, CONTAINER_ID_LENGTH);
    // container_buf_.body = (uint8_t*) malloc(sizeof(uint8_t) * MAX_CONTAINER_SIZE);
    // container_buf_.metadata = (uint8_t*) malloc(sizeof(uint8_t) * 73728);
    // container_buf_.currentSize = 0;
    // container_buf_.currentMetaSize = 0;
    
    // init req container buffer
    req_containers_.idBuffer = (uint8_t*) malloc(CONTAINER_CAPPING_VALUE * 
        CONTAINER_ID_LENGTH);
    req_containers_.containerArray = (uint8_t**) malloc(CONTAINER_CAPPING_VALUE * 
        sizeof(uint8_t*));
    req_containers_.idNum = 0;
    for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
        req_containers_.containerArray[i] = (uint8_t*) malloc(sizeof(uint8_t) * 
            MAX_CONTAINER_SIZE);
    }
    req_containers_.sizeArray = (uint32_t*) malloc(CONTAINER_CAPPING_VALUE * 
        sizeof(uint32_t));

    // tool::Logging(my_name_.c_str(), "init SyncDataWriter.\n");
}

/**
 * @brief Destroy the Sync Data Writer object
 * 
 */
SyncDataWriter::~SyncDataWriter() {
    free(req_containers_.idBuffer);
    for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
        free(req_containers_.containerArray[i]);
    }
    free(req_containers_.containerArray);
    free(req_containers_.sizeArray);

    free(out_chunk_query_.OutChunkQueryBase);
    free(update_index_.OutChunkQueryBase);

    free(debug_index_.OutChunkQueryBase);

    // // delete container_buf_;
    // free(container_buf_.containerID);
    // free(container_buf_.body);
    // free(container_buf_.metadata);
}

/**
 * @brief process a batch of recv data
 * 
 * @param recv_buf 
 * @param recv_size 
 */
void SyncDataWriter::ProcessOneBatch(uint8_t* recv_buf, uint32_t recv_size) {
#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase6_stime, NULL);
#endif

    // do ecall to decode & write & update index
#if (RECOVER_CHECK == 0)
    Ecall_Stream_Phase6_ProcessBatch(sgx_eid_, recv_buf, recv_size, 
        &req_containers_, &out_chunk_query_, &container_buf_, &update_index_);
#endif    

#if (RECOVER_CHECK == 1)
    Ecall_Stream_Phase6_ProcessBatch_Debug(sgx_eid_, recv_buf, recv_size, 
        &req_containers_, &out_chunk_query_, &container_buf_, &update_index_,
        &debug_index_);
#endif
    
    // reset
    update_index_.queryNum = 0;
    out_chunk_query_.queryNum = 0;

#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase6_etime, NULL);
    _phase6_process_time += tool::GetTimeDiff(phase6_stime, phase6_etime);
#endif     

    return ;
}

/**
 * @brief process the tail batch
 * 
 */
void SyncDataWriter::ProcessTailBatch() {
#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase6_stime, NULL);
#endif

    // write the tail container
    if (container_buf_.currentSize != 0) {
        Ocall_WriteSyncContainer(&container_buf_);
    }

    // update the tail metadata
    if (update_index_.queryNum != 0) {
        Ocall_UpdateOutFPIndex((void*)&update_index_);
    }

#if (RECOVER_CHECK == 1)
    // debug
    if (debug_index_.queryNum != 0) {
        Ocall_CheckDebugIndex((void*)&debug_index_);
    }
#endif

#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase6_etime, NULL);
    _phase6_process_time += tool::GetTimeDiff(phase6_stime, phase6_etime);
#endif

    return ;
}