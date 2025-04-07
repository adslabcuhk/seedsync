/**
 * @file commonEnclave.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief define the 
 * @version 0.1
 * @date 2020-12-21
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include "../../include/commonEnclave.h"

namespace Enclave {
    unordered_map<int, string> clientSessionKeyIndex_;
    unordered_map<string, vector<string>> fileRecipeNameToHMACIndex_;
    uint8_t* enclaveKey_;
    uint8_t* indexQueryKey_;
    bool firstBootstrap_; // 
    // config
    uint64_t sendChunkBatchSize_;
    uint64_t sendRecipeBatchSize_;
    uint64_t topKParam_;
    uint64_t sendMetaBatchSize_;
    // lock
    mutex sessionKeyLck_;
    mutex sketchLck_;
    mutex topKIndexLck_;
    // the obj to the enclave index
    EnclaveBase* enclaveBaseObj_;
};

namespace SyncEnclave {
    uint8_t* enclave_key_;
    uint8_t* index_query_key_;
    // config
    uint64_t send_chunk_batch_size_;
    uint64_t send_meta_batch_size_;
    uint64_t send_recipe_batch_size_;
    uint64_t max_seg_index_entry_size_;
    // lock
    mutex enclave_cache_lck_;

    EcallSegHash* ecall_seghash_obj_;
    EcallSegIndex* ecall_segindex_obj_;
    EcallChunkHash* ecall_chunkhash_obj_;
    EcallChunkIndex* ecall_chunkindex_obj_;
    EcallFeatureGen* ecall_featuregen_obj_;
    EcallFeatureIndex* ecall_featureindex_obj_;
    EcallStreamChunkHash* ecall_streamchunkhash_obj_;
    LocalityCache* locality_cache_obj_;
    EcallStreamChunkIndex* ecall_streamchunkindex_obj_;
    EcallStreamFeature* ecall_streamfeature_obj_;
    EcallStreamBaseHash* ecall_streambasehash_obj_;
    EcallStreamEncode* ecall_streamencode_obj_;
    EcallStreamWriter* ecall_streamwriter_obj_;
};

void Enclave::Logging(const char* logger, const char* fmt, ...) {
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    string output;
    output = "<" + string(logger) + ">: " + string(buf);
    Ocall_Printf(output.c_str());
}

/**
 * @brief write the buffer to the file
 * 
 * @param buffer the pointer to the buffer
 * @param bufferSize the buffer size
 * @param fileName the file name
 */
void Enclave::WriteBufferToFile(uint8_t* buffer, size_t bufferSize, const char* fileName) {
    size_t remainSize = bufferSize;
    size_t offset = 0;
    while (remainSize != 0) {
        if (remainSize > SGX_PERSISTENCE_BUFFER_SIZE) {
            Ocall_WriteSealedData(fileName, buffer + offset, SGX_PERSISTENCE_BUFFER_SIZE);
            offset += SGX_PERSISTENCE_BUFFER_SIZE;
            remainSize -= SGX_PERSISTENCE_BUFFER_SIZE;
        } else {
            Ocall_WriteSealedData(fileName, buffer + offset, remainSize);
            offset += remainSize;
            remainSize -= remainSize;
            break;
        }
    }
    return ;
}

/**
 * @brief read the file to the buffer
 * 
 * @param buffer the pointer to the buffer
 * @param bufferSize the buffer size
 * @param fileName the file name
 */
void Enclave::ReadFileToBuffer(uint8_t* buffer, size_t bufferSize, const char* fileName) {
    size_t remainSize = bufferSize;
    size_t offset = 0;
    while (remainSize != 0) {
        if (remainSize > SGX_PERSISTENCE_BUFFER_SIZE) {
            Ocall_ReadSealedData(fileName, buffer + offset, SGX_PERSISTENCE_BUFFER_SIZE); 
            offset += SGX_PERSISTENCE_BUFFER_SIZE;
            remainSize -= SGX_PERSISTENCE_BUFFER_SIZE;
        } else {
            Ocall_ReadSealedData(fileName, buffer + offset, remainSize);
            offset += remainSize;
            remainSize -= remainSize;
            break;
        }
    }
    return ;
}

string Enclave::convertBufferToHexString(uint8_t* buffer, uint32_t bufferSize)
{
    const char hexDigits[] = "0123456789abcdef";
    string hexStr(bufferSize * 2, '\0');
    for (uint32_t i = 0; i < bufferSize; ++i) {
        hexStr[2 * i] = hexDigits[(buffer[i] >> 4) & 0x0F];
        hexStr[2 * i + 1] = hexDigits[buffer[i] & 0x0F];
    }
    return hexStr;
}