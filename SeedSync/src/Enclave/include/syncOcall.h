/**
 * @file syncOcall.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-01-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SYNC_OCALL_H
#define SYNC_OCALL_H

#include "ocallUtil.h"
#include "../../../include/absDatabase.h"
#include "../../../include/sync_storage.h"
#include "../../../include/phase_sender.h"
#include "../../../build/src/Enclave/storeEnclave_u.h"

// for sgx
#include "sgx_urts.h"

namespace SyncOutEnclave {
    // ocall for out-enclave global indexes
    extern AbsDatabase* out_chunk_index_;
    extern AbsDatabase* out_feature_index_;

    extern string my_name_;
    
    // // for sealing
    // extern ofstream out_sealed_file_hdl_;
    // extern ifstream in_sealed_file_hdl_;

    extern string myName_;

    // for persistence
    extern ofstream outSealedFile_;
    extern ifstream inSealedFile_;

    /**
     * @brief init the sync ocall var
     * 
     * @param out_chunk_index 
     * @param out_feature_index 
     */
    void Init(AbsDatabase* out_chunk_index, AbsDatabase* out_feature_index, 
        SyncStorage* sync_storage);

    /**
     * @brief destroy the sync ocall var
     * 
     */
    void Destroy();
};

/**
 * @brief exit the enclave with error message
 * 
 * @param error_msg 
 */
void Ocall_SGX_Exit_Error(const char* error_msg);

/**
 * @brief Print the content of the buffer
 * 
 * @param buffer the input buffer
 * @param len the length in byte
 */
void Ocall_PrintfBinary(const uint8_t* buffer, size_t len);

/**
 * @brief printf interface for Ocall
 * 
 * @param str input string 
 */
void Ocall_Printf(const char* str);

/**
 * @brief generate the UUID
 * 
 * @param id the uuid buffer
 * @param len the id len
 */
void Ocall_CreateUUID(uint8_t* id, size_t len);

/**
 * @brief write the data to the disk file
 * 
 * @param sealedFileName the name of the sealed file
 * @param sealedDataBuffer sealed data buffer
 * @param sealedDataSize sealed data size
 */
void Ocall_WriteSealedData(const char* sealedFileName, uint8_t* sealedDataBuffer, size_t sealedDataSize);

/**
 * @brief init the file output stream
 *
 * @param ret the return result 
 * @param sealedFileName the sealed file name
 */
void Ocall_InitWriteSealedFile(bool* ret, const char* sealedFileName);

/**
 * @brief close the file output stream
 * 
 * @param sealedFileName the sealed file name
 */
void Ocall_CloseWriteSealedFile(const char* sealedFileName);


/**
 * @brief read the sealed data from the file
 * 
 * @param sealedFileName the sealed file
 * @param dataBuffer the data buffer
 * @param sealedDataSize the size of sealed data
 */
void Ocall_ReadSealedData(const char* sealedFileName, uint8_t* dataBuffer, uint32_t sealedDataSize);

/**
 * @brief get current time from the outside
 * 
 */
void Ocall_GetCurrentTime(uint64_t* retTime);

/**
 * @brief Init the unseal file stream 
 *
 * @param fileSize the return file size
 * @param sealedFileName the sealed file name
 */
void Ocall_InitReadSealedFile(size_t* fileSize, const char* sealedFileName);
/**
 * @brief close the file input stream
 * 
 * @param sealedFileName the sealed file name
 */
void Ocall_CloseReadSealedFile(const char* sealedFileName);

/**
 * @brief query the out-enclave chunk index
 * 
 * @param out_chunk_query 
 */
void Ocall_QueryChunkIndex(void* out_chunk_query);

/**
 * @brief query the out-enclave chunk index
 * 
 * @param out_chunk_query 
 */
void Ocall_QueryChunkAddr(void* out_chunk_query);

void Ocall_QueryEncodeChunkAddr(void* out_chunk_query);

/**
 * @brief issue a single query to out-enclave chunk index
 * 
 * @param query_entry 
 */
void Ocall_SingleQueryChunkIndex(void* query_entry);

/**
 * @brief query the out-enclave feature index
 * 
 * @param out_feature_query 
 */
void Ocall_QueryFeatureIndex(void* out_base_query);

// void Ocall_UpdateSegIndex();

/**
 * @brief fetch the containers into enclave
 * 
 * @param req_container 
 */
void Ocall_SyncGetReqContainer(void* req_container);

/**
 * @brief fetch the containers with sizes into enclave
 * 
 * @param req_container 
 */
void Ocall_SyncGetReqContainerWithSize(void* req_container);

/**
 * @brief fetch a single container into enclave
 * 
 * @param req_container 
 */
void Ocall_SingleGetReqContainer(void* req_container);

/**
 * @brief read the chunk batch
 * 
 * @param chunk_data 
 * @param chunk_num 
 */
void Ocall_ReadChunkBatch(uint8_t* chunk_data, uint32_t chunk_num);

/**
 * @brief write the chunk batch
 * 
 * @param chunk_data 
 * @param chunk_num 
 */
void Ocall_WriteChunkBatch(uint8_t* chunk_data, uint32_t chunk_num);

/**
 * @brief write the sync container
 * 
 * @param sync_container 
 */
void Ocall_WriteSyncContainer(Container_t* newContainer);

/**
 * @brief update the outside fp index
 * 
 * @param update_index 
 */
void Ocall_UpdateOutFPIndex(void* update_index);

// for debugging
void Ocall_InsertDebugIndex(void* debug_index);

void Ocall_CheckDebugIndex(void* debug_index);

// void Ocall_ClearDebugIndex(void* debug_index);

#endif