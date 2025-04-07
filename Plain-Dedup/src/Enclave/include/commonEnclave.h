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
#include "ecallClient.h"
#include "sgx_trts.h"

class EnclaveBase;

class EcallSegHash;
class EcallSegIndex;
class EcallChunkHash;
class EcallChunkIndex;
class EcallFeatureGen;
class EcallFeatureIndex;
class EcallStreamChunkHash;
class LocalityCache;
class EcallStreamChunkIndex;
class EcallStreamFeature;
class EcallStreamBaseHash;
class EcallStreamEncode;
class EcallStreamWriter;

using namespace std;
namespace Enclave {
void Logging(const char* logger, const char* fmt, ...);
void WriteBufferToFile(uint8_t* buffer, size_t bufferSize, const char* fileName);
void ReadFileToBuffer(uint8_t* buffer, size_t bufferSize, const char* fileName);
string convertBufferToHexString(uint8_t* buffer, uint32_t bufferSize);
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
// the obj to the enclave index
extern EnclaveBase* enclaveBaseObj_;
};

namespace SyncEnclave {
extern uint8_t* enclave_key_; // for sealing
extern uint8_t* index_query_key_;
// config
extern uint64_t send_chunk_batch_size_;
extern uint64_t send_recipe_batch_size_;
extern uint64_t send_meta_batch_size_;
extern uint64_t max_seg_index_entry_size_;
// lock
extern mutex enclave_cache_lck_;

extern EcallSegHash* ecall_seghash_obj_;
extern EcallSegIndex* ecall_segindex_obj_;
extern EcallChunkHash* ecall_chunkhash_obj_;
extern EcallChunkIndex* ecall_chunkindex_obj_;
extern EcallFeatureGen* ecall_featuregen_obj_;
extern EcallFeatureIndex* ecall_featureindex_obj_;
extern EcallStreamChunkHash* ecall_streamchunkhash_obj_;
extern LocalityCache* locality_cache_obj_;
extern EcallStreamChunkIndex* ecall_streamchunkindex_obj_;
extern EcallStreamFeature* ecall_streamfeature_obj_;
extern EcallStreamBaseHash* ecall_streambasehash_obj_;
extern EcallStreamEncode* ecall_streamencode_obj_;
extern EcallStreamWriter* ecall_streamwriter_obj_;
};

#endif