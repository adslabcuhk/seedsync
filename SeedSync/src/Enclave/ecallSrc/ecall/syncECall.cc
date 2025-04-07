/**
 * @file syncECall.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief
 * @version 0.1
 * @date 2024-01-05
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../../include/syncECall.h"

using namespace SyncEnclave;
using namespace Enclave;

/**
 * @brief init the enclave configurations
 *
 * @param enclave_config
 */
void Ecall_Sync_Enclave_Init(SyncEnclaveConfig_t* enclave_config)
{
    using namespace SyncEnclave;
    // get the enclave key and query key from peristsed file
    enclave_key_ = (uint8_t*)malloc(sizeof(uint8_t) * CHUNK_HASH_SIZE);
    index_query_key_ = (uint8_t*)malloc(sizeof(uint8_t) * CHUNK_HASH_SIZE);

    size_t read_size = 0;
    Ocall_InitReadSealedFile(&read_size, ENCLAVE_KEY_FILE_NAME);
    if (read_size == 0) {
#if DEBUG_FLAG == 1
        // SyncEnclave::Logging("SyncEnclave Init", "Key file does not exist.\n");
#endif
    } else {
        // key file exists
        uint8_t* read_buf = (uint8_t*)malloc(CHUNK_HASH_SIZE * 2);
        Ocall_ReadSealedData(ENCLAVE_KEY_FILE_NAME, read_buf,
            CHUNK_HASH_SIZE * 2);
        memcpy(enclave_key_, read_buf, CHUNK_HASH_SIZE);
        memcpy(index_query_key_, read_buf + CHUNK_HASH_SIZE, CHUNK_HASH_SIZE);
        free(read_buf);
    }
    Ocall_CloseReadSealedFile(ENCLAVE_KEY_FILE_NAME);

    // config
    SyncEnclave::send_chunk_batch_size_ = enclave_config->sendChunkBatchSize;
    SyncEnclave::send_meta_batch_size_ = enclave_config->sendMetaBatchSize;
    SyncEnclave::send_recipe_batch_size_ = enclave_config->sendRecipeBatchSize;
    SyncEnclave::enclave_cache_item_num_ = enclave_config->enclaveCacheItemNum;
    // SyncEnclave::max_seg_index_entry_size_ =

#if RECIPE_HMAC == 1 && RECIPE_HMAC_PERSIST == 1
    bool loadRecipeStatus = Enclave::LoadRecipeIndex();
    if (!loadRecipeStatus) {
        // SyncEnclave::Logging("SyncEnclave Init", "Load Recipe HMAC Index failed.\n");
    } else {
        uint32_t recipeNum = Enclave::fileRecipeNameToHMACIndex_.size();
        // SyncEnclave::Logging("SyncEnclave Init", "Load Recipe HMAC Index success, recipe num: %d.\n", recipeNum);
        for (auto it = Enclave::fileRecipeNameToHMACIndex_.begin(); it != Enclave::fileRecipeNameToHMACIndex_.end(); it++) {
            // SyncEnclave::Logging("SyncEnclave Init", "Recipe path: %s.\n", it->first.c_str());
        }
    }
#endif
    return;
}

/**
 * @brief destroy the sync enclave
 *
 */
void Ecall_Sync_Enclave_Destroy()
{
    using namespace SyncEnclave;
    free(enclave_key_);
    free(index_query_key_);
    return;
}

/**
 * @brief init the ecall sync
 *
 */
void Ecall_Init_Sync()
{
    ecall_streamchunkhash_obj_ = new EcallStreamChunkHash();
    locality_cache_obj_ = new LocalityCache();
    ecall_streamchunkindex_obj_ = new EcallStreamChunkIndex(locality_cache_obj_);
    ecall_streamfeature_obj_ = new EcallStreamFeature();
    ecall_streambasehash_obj_ = new EcallStreamBaseHash(locality_cache_obj_);
    ecall_streamencode_obj_ = new EcallStreamEncode();
    ecall_streamwriter_obj_ = new EcallStreamWriter();
    // TODO: add protocol objs here
    return;
}

/**
 * @brief destroy the sync enclave memory
 *
 */
void Ecall_Destroy_Sync()
{
    if (ecall_streamchunkhash_obj_) {
        delete ecall_streamchunkhash_obj_;
    }
    if (locality_cache_obj_) {
        delete locality_cache_obj_;
    }
    if (ecall_streamchunkindex_obj_) {
        delete ecall_streamchunkindex_obj_;
    }
    if (ecall_streamfeature_obj_) {
        delete ecall_streamfeature_obj_;
    }
    if (ecall_streambasehash_obj_) {
        delete ecall_streambasehash_obj_;
    }
    if (ecall_streamencode_obj_) {
        delete ecall_streamencode_obj_;
    }
    if (ecall_streamwriter_obj_) {
        delete ecall_streamwriter_obj_;
    }

    return;
}

/**
 * @brief process the batch of phase-1 in stream mode: read chunk hash from recipe
 *
 * @param filename_buf
 * @param recipe_buf
 * @param recipe_num
 * @param chunk_hash_buf
 */
void Ecall_Stream_Phase1_ProcessBatch(uint8_t* filename_buf, uint32_t currentBatchID,
    uint8_t* recipe_buf, size_t recipe_num, uint8_t* chunk_hash_buf)
{

    ecall_streamchunkhash_obj_->ProcessBatch(filename_buf, currentBatchID, recipe_buf,
        recipe_num, chunk_hash_buf);

    return;
}

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
    uint8_t* uni_fp_list, size_t* uni_fp_num, Container_t* req_container)
{

    ecall_streamchunkindex_obj_->NaiveStreamCache(fp_list, fp_num, uni_fp_list, uni_fp_num,
        req_container);

    return;
}

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
    ReqContainer_t* req_container)
{

    ecall_streamchunkindex_obj_->BatchStreamCache(fp_list, fp_num, uni_fp_list, uni_fp_num,
        addr_query, req_container);
}

// for debugging
void Ecall_Stream_Phase2_ProcessBatch_NaiveStreamCache_Debug(uint8_t* fp_list, size_t fp_num,
    uint8_t* uni_fp_list, size_t* uni_fp_num, Container_t* req_container,
    OutChunkQuery_t* debug_out_query, OutChunkQueryEntry_t* single_out_query)
{

    ecall_streamchunkindex_obj_->NaiveStreamCacheDebug(fp_list, fp_num, uni_fp_list, uni_fp_num,
        req_container, debug_out_query, single_out_query);

    return;
}

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
    uint8_t* feature_list, size_t feature_num)
{

    ecall_streamfeature_obj_->ProcessBatch(unifp_list, unifp_num, req_container,
        addr_query, feature_list, feature_num);

    return;
}

/**
 * @brief process the batch of phase-4 in stream mode: return basehash
 *
 * @param feature_fp_list
 * @param feature_num
 * @param out_list
 * @param out_num
 */
void Ecall_Stream_Phase4_ProcessBatch(uint8_t* feature_fp_list, size_t feature_num,
    uint8_t* out_list, size_t out_num)
{

    ecall_streambasehash_obj_->ProcessBatch(feature_fp_list, feature_num, out_list, out_num);

    return;
}

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
    uint32_t* out_size)
{

    ecall_streamencode_obj_->ProcessBatch(in_buf, in_size, req_container, addr_query,
        out_buf, out_size);

    return;
}

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
    Container_t* container_buf, OutChunkQuery_t* update_index)
{

    // // SyncEnclave::Logging("before streamwriter process batch", "\n");
    ecall_streamwriter_obj_->ProcessBatch(recv_buf, recv_size, req_container,
        base_addr_query, container_buf, update_index);

    return;
}

// for debugging
void Ecall_Stream_Phase6_ProcessBatch_Debug(uint8_t* recv_buf, uint32_t recv_size,
    ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
    Container_t* container_buf, OutChunkQuery_t* update_index,
    OutChunkQuery_t* debug_check_quary)
{

    ecall_streamwriter_obj_->ProcessBatch(recv_buf, recv_size, req_container,
        base_addr_query, container_buf, update_index, debug_check_quary);

    return;
}

/**
 * @brief get the sync info from enclave
 *
 * @param sync_info
 */
void Ecall_GetSyncEnclaveInfo(SyncEnclaveInfo_t* sync_info, uint8_t type)
{

    if (type == SOURCE_CLOUD) {
        // for source cloud logs
        sync_info->total_chunk_num = ecall_streamchunkhash_obj_->_total_chunk_fp_num;
        sync_info->total_chunk_size = ecall_streamchunkhash_obj_->_total_chunk_size;

        sync_info->total_unique_num = ecall_streamfeature_obj_->_total_unique_num;
        sync_info->total_unique_size = ecall_streamfeature_obj_->_total_unique_size;

        sync_info->total_similar_num = ecall_streamencode_obj_->_total_similar_num;
        sync_info->total_similar_size = ecall_streamencode_obj_->_total_similar_chunk_size;
        sync_info->total_base_size = ecall_streamencode_obj_->_total_base_chunk_size;
        sync_info->total_delta_size = ecall_streamencode_obj_->_total_delta_size;
        sync_info->total_comp_delta_size = ecall_streamencode_obj_->_total_comp_delta_size;
    } else if (type == DEST_CLOUD) {
        // for dest cloud logs
        sync_info->global_unique_num = ecall_streamchunkindex_obj_->_global_unique_num;
        sync_info->global_duplicate_num = ecall_streamchunkindex_obj_->_global_duplicate_num;
        sync_info->enclave_unique_num = ecall_streamchunkindex_obj_->_enclave_unique_num;
        sync_info->enclave_duplicate_num = ecall_streamchunkindex_obj_->_enclave_duplicate_num;

        sync_info->enclave_similar_num = ecall_streambasehash_obj_->_enclave_similar_num;
        sync_info->batch_similar_num = ecall_streambasehash_obj_->_batch_simlar_num;
        sync_info->non_similar_num = ecall_streambasehash_obj_->_non_similar_num;

        sync_info->total_recv_size = ecall_streamwriter_obj_->_total_recv_size;
        sync_info->total_write_size = ecall_streamwriter_obj_->_total_write_size;
        sync_info->only_comp_size = ecall_streamwriter_obj_->_only_comp_size;
        sync_info->uncomp_delta_size = ecall_streamwriter_obj_->_uncomp_delta_size;
        sync_info->comp_delta_size = ecall_streamwriter_obj_->_comp_delta_size;
        sync_info->uncomp_similar_size = ecall_streamwriter_obj_->_uncomp_similar_size;
        sync_info->comp_similar_size = ecall_streamwriter_obj_->_comp_similar_size;
    }

    return;
}
