/**
 * @file commonEnclave.h
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief the common structure of the enclave
 * @version 0.1
 * @date 2021-03-24
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef COMMON_ENCLAVE_H
#define COMMON_ENCLAVE_H

// for std using inside the enclave
#include "list"
#include "mutex"
#include "queue"
#include "set"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string"
#include "string.h"
#include "unordered_map" // for deduplication index inside the enclave
#include "unordered_set"
#include "vector" // for heap

// for sgx library
#include "../../../build/src/Enclave/storeEnclave_t.h"
#include "../../../include/chunkStructure.h"
#include "../../../include/constVar.h"
#include "sgx_trts.h"

#define SEALED_HMAC "recipe-hmac"

class EcallStreamChunkHash;
class LocalityCache;
class EcallStreamChunkIndex;
class EcallStreamFeature;
class EcallStreamBaseHash;
class EcallStreamEncode;
class EcallStreamWriter;

using namespace std;

namespace Enclave {
// void Logging(const char* logger, const char* fmt, ...);
string convertBufferToHexString(uint8_t* buffer, uint32_t bufferSize);
void WriteBufferToFile(uint8_t* buffer, size_t bufferSize, const char* fileName);
void ReadFileToBuffer(uint8_t* buffer, size_t bufferSize, const char* fileName);
bool PersistRecipeIndex();
bool LoadRecipeIndex();

extern unordered_map<int, string> clientSessionKeyIndex_;
extern unordered_map<string, vector<string>> fileRecipeNameToHMACIndex_;
extern uint8_t* enclaveKey_;
extern uint8_t* indexQueryKey_;
extern bool firstBootstrap_; // use to control the RA
// config
extern uint64_t sendChunkBatchSize_;
extern uint64_t sendRecipeBatchSize_;
extern uint64_t topKParam_;
extern uint64_t sendMetaBatchSize_;
// mutex
extern mutex sessionKeyLck_;
extern mutex sketchLck_;
extern mutex topKIndexLck_;
// // the obj to the enclave index
// extern EnclaveBase* enclaveBaseObj_;
};

namespace SyncEnclave {
void Logging(const char* logger, const char* fmt, ...);
extern uint8_t* enclave_key_; // for sealing
extern uint8_t* index_query_key_;
// config
extern uint64_t send_chunk_batch_size_;
extern uint64_t send_recipe_batch_size_;
extern uint64_t send_meta_batch_size_;
extern uint64_t max_seg_index_entry_size_;
// lock
extern mutex enclave_cache_lck_;

extern EcallStreamChunkHash* ecall_streamchunkhash_obj_;
extern LocalityCache* locality_cache_obj_;
extern EcallStreamChunkIndex* ecall_streamchunkindex_obj_;
extern EcallStreamFeature* ecall_streamfeature_obj_;
extern EcallStreamBaseHash* ecall_streambasehash_obj_;
extern EcallStreamEncode* ecall_streamencode_obj_;
extern EcallStreamWriter* ecall_streamwriter_obj_;
};

#endif