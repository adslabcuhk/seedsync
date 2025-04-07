/**
 * @file syncECall.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief
 * @version 0.1
 * @date 2024-01-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef SYNC_ECALL_H
#define SYNC_ECALL_H

#include "commonEnclave.h"
#include "ecallStreamBaseHash.h"
#include "ecallStreamChunkHash.h"
#include "ecallStreamChunkIndex.h"
#include "ecallStreamEncode.h"
#include "ecallStreamFeature.h"
#include "ecallStreamWriter.h"
#include "localityCache.h"

#define ENCLAVE_KEY_FILE_NAME "enclave-key"
// #define ENCLAVE_INDEX_INFO_NAME "enclave-index-info"

class EcallStreamChunkHash;
class LocalityCache;
class EcallStreamChunkIndex;
class EcallStreamFeature;
class EcallStreamBaseHash;
class EcallStreamEncode;
class EcallStreamWriter;

namespace SyncEnclave {
// TODO: add phases obj here
extern EcallStreamChunkHash* ecall_streamchunkhash_obj_;
extern LocalityCache* locality_cache_obj_;
extern EcallStreamChunkIndex* ecall_streamchunkindex_obj_;
extern EcallStreamFeature* ecall_streamfeature_obj_;
extern EcallStreamBaseHash* ecall_streambasehash_obj_;
extern EcallStreamEncode* ecall_streamencode_obj_;
extern EcallStreamWriter* ecall_streamwriter_obj_;
}

using namespace SyncEnclave;

/**
 * @brief init the enclave configurations
 *
 * @param enclave_config
 */
void Ecall_Sync_Enclave_Init(SyncEnclaveConfig_t* enclave_config);

/**
 * @brief destroy the sync enclave
 *
 */
void Ecall_Sync_Enclave_Destroy();

/**
 * @brief init the ecall sync
 *
 */
void Ecall_Init_Sync();

/**
 * @brief destroy the sync enclave memory
 *
 */
void Ecall_Destroy_Sync();

/**
 * @brief process the batch of phase-1 in stream mode: read chunk hash from recipe
 *
 * @param filename_buf
 * @param recipe_buf
 * @param recipe_num
 * @param chunk_hash_buf
 */
void Ecall_Stream_Phase1_ProcessBatch(uint8_t* filename_buf, uint32_t currentBatchID, uint8_t* recipe_buf, size_t recipe_num, uint8_t* chunk_hash_buf);

/**
 * @brief process the batch of phase-2 in stream mode: identify uni chunk hash
 *
 * @param fp_list
 * @param fp_num
 * @param uni_fp_list
 * @param uni_fp_num
 * @param req_container
 */
void Ecall_Stream_Phase2_ProcessBatch_NaiveStreamCache(uint8_t* fp_list, size_t fp_num,
    uint8_t* uni_fp_list, size_t* uni_fp_num, Container_t* req_container);

/**
 * @brief process the batch of phase-2 with batch stream cache
 *
 * @param fp_list
 * @param fp_num
 * @param uni_fp_list
 * @param uni_fp_num
 * @param addr_query
 * @param req_container
 */
void Ecall_Stream_Phase2_ProcessBatch_BatchStreamCache(uint8_t* fp_list, size_t fp_num,
    uint8_t* uni_fp_list, size_t* uni_fp_num, OutChunkQuery_t* addr_query,
    ReqContainer_t* req_container);

/**
 * @brief process the batch of phase-3 in stream mode: return features
 *
 * @param unifp_list
 * @param unifp_num
 * @param req_container
 * @param addr_query
 * @param feature_list
 * @param feature_num
 */
void Ecall_Stream_Phase3_ProcessBatch(uint8_t* unifp_list, size_t unifp_num,
    ReqContainer_t* req_container, OutChunkQuery_t* addr_query,
    uint8_t* feature_list, size_t feature_num);

/**
 * @brief process the batch of phase-4 in stream mode: return basehash
 *
 * @param feature_fp_list
 * @param feature_num
 * @param out_list
 * @param out_num
 */
void Ecall_Stream_Phase4_ProcessBatch(uint8_t* feature_fp_list, size_t feature_num,
    uint8_t* out_list, size_t out_num);

/**
 * @brief process the batch of phase-5 in stream mode: return data
 *
 * @param in_buf
 * @param in_size
 * @param req_container
 * @param addr_query
 * @param out_buf
 * @param out_size
 */
void Ecall_Stream_Phase5_ProcessBatch(uint8_t* in_buf, uint32_t in_size,
    ReqContainer_t* req_container, OutChunkQuery_t* addr_query, uint8_t* out_buf,
    uint32_t* out_size);

/**
 * @brief process the batch of phase-6 in stream mode: decode & write
 *
 * @param recv_buf
 * @param recv_size
 * @param req_container
 * @param base_addr_query
 */
void Ecall_Stream_Phase6_ProcessBatch(uint8_t* recv_buf, uint32_t recv_size,
    ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
    Container_t* container_buf, OutChunkQuery_t* update_index);

// void Ecall_Test_Execute(uint8_t* recv_buf, uint32_t recv_size,
//     ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
//     Container_t* container_buf, OutChunkQuery_t* update_index);

// void Ecall_Test_Execute_2();

/**
 * @brief get the sync info from enclave
 *
 * @param sync_info
 */
void Ecall_GetSyncEnclaveInfo(SyncEnclaveInfo_t* sync_info, uint8_t type);

// for debugging
void Ecall_Stream_Phase2_ProcessBatch_NaiveStreamCache_Debug(uint8_t* fp_list, size_t fp_num,
    uint8_t* uni_fp_list, size_t* uni_fp_num, Container_t* req_container,
    OutChunkQuery_t* debug_out_query, OutChunkQueryEntry_t* single_out_query);

void Ecall_Stream_Phase6_ProcessBatch_Debug(uint8_t* recv_buf, uint32_t recv_size,
    ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
    Container_t* container_buf, OutChunkQuery_t* update_index,
    OutChunkQuery_t* debug_check_quary);

#endif
