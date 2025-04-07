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
#if RECIPE_HMAC == 1
unordered_map<string, vector<string>> fileRecipeNameToHMACIndex_;
#endif
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
// // the obj to the enclave index
// EnclaveBase* enclaveBaseObj_;
};

namespace SyncEnclave {
uint8_t* enclave_key_;
uint8_t* index_query_key_;
// config
uint64_t send_chunk_batch_size_;
uint64_t send_meta_batch_size_;
uint64_t send_recipe_batch_size_;
// uint64_t max_seg_index_entry_size_;
uint64_t enclave_cache_item_num_;
// lock
mutex enclave_cache_lck_;

EcallStreamChunkHash* ecall_streamchunkhash_obj_;
LocalityCache* locality_cache_obj_;
EcallStreamChunkIndex* ecall_streamchunkindex_obj_;
EcallStreamFeature* ecall_streamfeature_obj_;
EcallStreamBaseHash* ecall_streambasehash_obj_;
EcallStreamEncode* ecall_streamencode_obj_;
EcallStreamWriter* ecall_streamwriter_obj_;
};

void SyncEnclave::Logging(const char* logger, const char* fmt, ...)
{
    char buf[BUFSIZ] = { '\0' };
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
void Enclave::WriteBufferToFile(uint8_t* buffer, size_t bufferSize, const char* fileName)
{
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
    return;
}

/**
 * @brief read the file to the buffer
 *
 * @param buffer the pointer to the buffer
 * @param bufferSize the buffer size
 * @param fileName the file name
 */
void Enclave::ReadFileToBuffer(uint8_t* buffer, size_t bufferSize, const char* fileName)
{
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
    return;
}

#if RECIPE_HMAC == 1
bool Enclave::PersistRecipeIndex()
{
    uint32_t offset = 0;
    bool persistenceStatus = false;
    Ocall_InitWriteSealedFile(&persistenceStatus, SEALED_HMAC);
    if (persistenceStatus == false) {
        Ocall_SGX_Exit_Error("EcallFreqIndex: cannot init the hmac sealed file.");
        return false;
    }
    uint32_t requiredBufferSize = 0;
    uint32_t itemNum = Enclave::fileRecipeNameToHMACIndex_.size();
    for (auto it : Enclave::fileRecipeNameToHMACIndex_) {
        requiredBufferSize += (it.first.size() + it.second.size() * CHUNK_HASH_SIZE + 2 * sizeof(uint32_t));
    }
    requiredBufferSize += sizeof(uint32_t);
    uint8_t* tmpBuffer = (uint8_t*)malloc(sizeof(uint8_t) * requiredBufferSize);
    memcpy(tmpBuffer + offset, &itemNum, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    for (auto it : Enclave::fileRecipeNameToHMACIndex_) {
        uint32_t recipePathSize = it.first.size();
        uint32_t recipeHMACNumber = it.second.size();
        memcpy(tmpBuffer + offset, &recipePathSize, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(tmpBuffer + offset, &recipeHMACNumber, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(tmpBuffer + offset, &(it.first[0]), it.first.size());
        offset += it.first.size();
        for (auto hmacIt : it.second) {
            memcpy(tmpBuffer + offset, &(hmacIt[0]), hmacIt.size());
            offset += hmacIt.size();
        }
        // SyncEnclave::Logging("Write Recipe Index", "recipe path: %s, recipe path size: %ld, HMAC number: %ld.\n", it.first.c_str(), recipePathSize, recipeHMACNumber);
    }
    Enclave::WriteBufferToFile(tmpBuffer, requiredBufferSize, SEALED_HMAC);
    Ocall_CloseWriteSealedFile(SEALED_HMAC);
    free(tmpBuffer);
    return true;
}

bool Enclave::LoadRecipeIndex()
{
    size_t sealedDataSize = 0;
    Ocall_InitReadSealedFile(&sealedDataSize, SEALED_HMAC);
    if (sealedDataSize == 0) {
        return false;
    }
    uint8_t* tmpHMACBuffer = (uint8_t*)malloc(sealedDataSize * sizeof(uint8_t));
    Enclave::ReadFileToBuffer(tmpHMACBuffer, sealedDataSize, SEALED_HMAC);
    uint32_t itemNum = 0, offset = 0;
    memcpy(&itemNum, tmpHMACBuffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    for (uint32_t i = 0; i < itemNum; i++) {
        uint32_t recipePathSize = 0, recipeHMACNumber = 0;
        memcpy(&recipePathSize, tmpHMACBuffer + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(&recipeHMACNumber, tmpHMACBuffer + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        string recipePathStr((char*)tmpHMACBuffer + offset, recipePathSize);
        offset += recipePathSize;
        vector<string> hmacListVec;
        // SyncEnclave::Logging("Load Recipe Index", "recipe path: %s, recipe path size: %ld, HMAC number: %ld.\n", recipePathStr.c_str(), recipePathSize, recipeHMACNumber);
        for (uint32_t hmacIndex = 0; hmacIndex < recipeHMACNumber; hmacIndex++) {
            string recipeHMACStr((char*)tmpHMACBuffer + offset, CHUNK_HASH_SIZE);
            offset += CHUNK_HASH_SIZE;
            hmacListVec.push_back(recipeHMACStr);
            // SyncEnclave::Logging("Load Recipe Index", "recipe path: %s, hmac index: %ld, hmac: %s.\n", recipePathStr.c_str(), hmacIndex, convertBufferToHexString((uint8_t*)recipeHMACStr.c_str(), recipeHMACStr.size()).c_str());
        }
        Enclave::fileRecipeNameToHMACIndex_.emplace(recipePathStr, hmacListVec);
    }
    Ocall_CloseReadSealedFile(SEALED_HMAC);
    return true;
}
#endif
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