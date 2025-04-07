/**
 * @file chunkStructure.h
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief define the necessary data structure in deduplication
 * @version 0.1
 * @date 2019-12-19
 * 
 * @copyright Copyright (c) 2019
 * 
 */

#ifndef DEDUP_CHUNK_h
#define DEDUP_CHUNK_h

#include "constVar.h"
#include <stdint.h>

typedef struct {
    uint32_t chunkSize;
    uint8_t data[MAX_CHUNK_SIZE];
} Chunk_t;

// typedef struct {
//     uint32_t chunkNum;
//     uint32_t segSize;
//     uint8_t data[MAX_SEG_SIZE];
// } SyncSegment_t;

// typedef struct {
//     uint32_t totalChunkNum;
//     uint32_t segSize;
// } SyncSegmentHead_t;

typedef struct {
    uint64_t fileSize;
    uint64_t totalChunkNum;
    // uint64_t totalSegNum;
} FileRecipeHead_t;

// typedef struct {
//     union {
//         Chunk_t chunk;
//         SyncSegmentHead_t segmentHead;
//         FileRecipeHead_t recipeHead;
//     };
//     int dataType;
// } Data_t;

// typedef struct {
//     uint32_t chunkSize;
//     uint8_t chunkHash[CHUNK_HASH_SIZE];
// } SegmentMeta_t;

// typedef struct {
//    uint32_t chunkNum;
//    uint32_t segmentSize;
//    uint32_t minHashVal;
//    uint8_t minHash[CHUNK_HASH_SIZE];
//    uint8_t* buffer;
//    SegmentMeta_t* metadata;
// } Segment_t;

typedef struct {
    uint8_t containerName[CONTAINER_ID_LENGTH]; 
    uint32_t offset;
    uint32_t length;
    uint8_t chunkHash[CHUNK_HASH_SIZE]; // add the chunkHash in file recipe entry (for segHash computation)
} RecipeEntry_t;

typedef struct {
    uint64_t sendChunkBatchSize;
    uint64_t sendRecipeBatchSize;
    uint64_t topKParam;
} EnclaveConfig_t;

typedef struct {
    uint64_t uniqueChunkNum;
    uint64_t uniqueDataSize;
    uint64_t logicalChunkNum;
    uint64_t logicalDataSize;
    uint64_t compressedSize;
    double enclaveProcessTime;
#if (SGX_BREAKDOWN == 1)
    double dataTranTime;
    double fpTime;
    double freqTime;
    double firstDedupTime;
    double secondDedupTime;
    double compTime;
    double encTime;
#endif
} EnclaveInfo_t;

typedef struct {
    // for dest cloud
    // phase-2
    uint64_t global_unique_num;
    uint64_t global_unique_size;
    uint64_t global_duplicate_num;
    uint64_t global_duplicate_size;
    uint64_t enclave_unique_num;
    uint64_t enclave_unique_size;
    uint64_t enclave_duplicate_num;
    uint64_t enclave_duplicate_size;
    // phase-4
    uint64_t enclave_similar_num;
    uint64_t enclave_similar_size;
    uint64_t batch_similar_num;
    uint64_t batch_similar_size;
    uint64_t non_similar_num;
    uint64_t non_similar_size;
    // phase-6
    uint64_t total_recv_size;
    uint64_t total_write_size;
    uint64_t only_comp_size;
    uint64_t uncomp_delta_size;
    uint64_t comp_delta_size;
    uint64_t uncomp_similar_size;
    uint64_t comp_similar_size;
    
    // for source cloud
    // phase-1
    uint64_t total_chunk_num;
    uint64_t total_chunk_size;
    // phase-3
    uint64_t total_unique_num;
    uint64_t total_unique_size;
    // phase-5
    uint64_t total_similar_num;
    uint64_t total_similar_size;
    uint64_t total_base_size;
    uint64_t total_delta_size;
    uint64_t total_comp_delta_size;
} SyncEnclaveInfo_t;

typedef struct {
    int messageType;
    uint32_t clientID;
    uint32_t dataSize;
    uint32_t currentItemNum;
} NetworkHead_t;

typedef struct {
    NetworkHead_t* header;
    uint8_t* sendBuffer;
    uint8_t* dataBuffer;
} SendMsgBuffer_t;

typedef struct {
    uint32_t recipeNum;
    uint8_t* entryList; // file recipe entry: [chunkHash || chunkSize]
} Recipe_t;

typedef struct {
    uint32_t containerID; // the ID to current restore buffer
    uint32_t offset;
    uint32_t length;
} EnclaveRecipeEntry_t;

typedef struct {
    uint8_t sim_tag; // sim; base; non-sim
    uint8_t baseHash[CHUNK_HASH_SIZE];
    uint32_t containerID; // the ID to current fetch buffer
    uint32_t offset;
    uint32_t length;
} SyncRecipeEntry_t;

// typedef struct {
//     uint8_t containerName[CONTAINER_ID_LENGTH]; 
//     uint32_t offset;
// } CacheIndex_t;

typedef struct {
    char containerID[CONTAINER_ID_LENGTH];
    uint8_t body[MAX_CONTAINER_SIZE]; 
    uint32_t currentSize;
    // uint8_t metadata[MAX_META_SIZE];
    // uint32_t currentMetaSize;
// #if (CONTAINER_META_SESSION == 1)
    // add metadata
    uint8_t metadata[MAX_META_SIZE];
    uint32_t currentMetaSize;
// #endif
} Container_t;

typedef struct {
    char* containerID;
    uint8_t* body;
    uint32_t currentSize;
// #if (CONTAINER_META_SESSION == 1)
    uint8_t* metadata;
    uint32_t currentMetaSize;
// #endif
} PtrContainer_t;

// typedef struct {
//     uint8_t segmentHash[CHUNK_HASH_SIZE];
//     uint8_t binID_[SEGMENT_ID_LENGTH];
// } PrimaryValue_t;

// typedef struct {
//     uint8_t chunkFp[CHUNK_HASH_SIZE];
//     RecipeEntry_t address;
// } BinValue_t;

// typedef struct {
//     RecipeEntry_t address;
//     uint32_t chunkFreq;
//     uint32_t idx;
// } HeapItem_t;

// typedef struct {
//     uint8_t dedupFlag; // true: for duplicate, false: for unique
//     uint8_t chunkHash[CHUNK_HASH_SIZE];
//     RecipeEntry_t chunkAddr;
// } OutQueryEntry_t; // returned by the outside application for query

// typedef struct {
//     uint8_t dedupFlag; // true: for duplicate, false: for unique
//     uint8_t chunkHash[CHUNK_HASH_SIZE];
//     RecipeEntry_t chunkAddr;
//     uint32_t chunkFreq;
//     uint32_t chunkSize;
//     // add chunk features
//     uint64_t features[SUPER_FEATURE_PER_CHUNK];
// } InQueryEntry_t; // returned by the outside application for query

// typedef struct {
//     uint32_t queryNum;
//     OutQueryEntry_t* outQueryBase;
// } OutQuery_t;

typedef struct {
    uint8_t* idBuffer;
    uint8_t** containerArray;
    uint32_t idNum;
    uint32_t* sizeArray;
} ReqContainer_t;

// typedef struct {
//     Container_t* curContainer;
//     // PtrContainer_t* curContainer;
//     Recipe_t* outRecipe;
//     OutQuery_t* outQuery;
//     void* outClient;
//     void* sgxClient;
// } UpOutSGX_t;

// typedef struct {
//     ReqContainer_t* reqContainer;
//     SendMsgBuffer_t* sendChunkBuf;
//     void* outClient; // the out-enclave client ptr
//     void* sgxClient; // the sgx-client ptr
// } ResOutSGX_t;

typedef struct _ra_msg4_struct {
    uint8_t status; // true: 1, false: 0
    //sgx_platform_info_t platformInfoBlob;
} ra_msg4_t;

typedef struct {
    uint8_t* secret;
    unsigned long length;
} DerivedKey_t;

// for sync
typedef struct {
    uint8_t type;
    uint32_t size;
    uint8_t data[MAX_CHUNK_SIZE];
} Msg_t;

typedef struct {
    uint8_t flag; // tag if this entry is a DATA or METADATA
    uint32_t index; // the index (position) in memory pool
} SendMQEntry_t;

// structures for stream-informed sync
typedef struct {
    uint8_t chunkHash[CHUNK_HASH_SIZE];
    uint8_t is_file_end;
} StreamPhase1MQ_t;

typedef StreamPhase1MQ_t StreamPhase2MQ_t;

typedef struct {
    uint64_t features[SUPER_FEATURE_PER_CHUNK];
    uint8_t chunkHash[CHUNK_HASH_SIZE];
    uint8_t is_file_end;
} StreamPhase3MQ_t;

typedef struct {
    uint8_t sim_tag; // 1 = sim; 0 = non-sim
    uint8_t baseHash[CHUNK_HASH_SIZE];
    uint8_t chunkHash[CHUNK_HASH_SIZE];
    uint8_t is_file_end;
} StreamPhase4MQ_t;

typedef struct {
    uint8_t chunkData[MAX_CHUNK_SIZE];
    uint8_t flag; // UNI_CHUNK or DELTA_CHUNK
    uint8_t baseHash[CHUNK_HASH_SIZE];
    uint32_t chunkSize;
    uint8_t is_file_end;
} StreamPhase5MQ_t;

//////////////////////////////////////////////////

// typedef struct {
//     uint8_t fileName[CHUNK_HASH_SIZE * 2];
// } phase1MQ_t;

// typedef phase1MQ_t phase2MQ_t;

// typedef struct {
//     uint8_t segHash[CHUNK_HASH_SIZE];
//     uint8_t is_find_end;
// } phase3MQ_t;

// typedef phase3MQ_t phase4MQ_t;

// typedef struct {
//     uint8_t chunkHash[CHUNK_HASH_SIZE];
//     uint8_t is_find_end;
// } phase5MQ_t;

// typedef phase5MQ_t phase6MQ_t;

// typedef struct {
//     uint64_t features[SUPER_FEATURE_PER_CHUNK];
//     uint8_t is_find_end; 
// } phase7MQ_t;

// typedef struct {
//     uint8_t simFlag; // SIM or NOT_SIM
//     uint8_t baseHash[CHUNK_HASH_SIZE];
//     uint8_t is_find_end;
// } phase8MQ_t;

// typedef struct {
//     uint64_t features[SUPER_FEATURE_PER_CHUNK];
//     uint8_t chunkData[MAX_CHUNK_SIZE + IV_SIZE];
//     uint32_t chunkSize;
//     uint8_t dedupFlag;
//     uint8_t chunkHash[CHUNK_HASH_SIZE];
//     uint8_t is_find_end;
// } phase8LocalMQ_t;

// typedef struct {
//     // for local delta
//     uint64_t features[SUPER_FEATURE_PER_CHUNK];
//     uint8_t deltaData[MAX_CHUNK_SIZE];
//     uint32_t deltaSize;
//     uint8_t baseHash[CHUNK_HASH_SIZE];
//     uint8_t is_find_end;
// } sendDelta_t;

// typedef struct {
//     uint32_t chunkSize;
//     uint8_t chunkData[MAX_CHUNK_SIZE];
//     uint8_t chunkHash[CHUNK_HASH_SIZE];
//     uint64_t features[SUPER_FEATURE_PER_CHUNK];
//     uint8_t type; // compressed delta; delta; compressed normal; normal
//     uint8_t baseHash[CHUNK_HASH_SIZE];
//     uint8_t is_file_end;
// } SyncChunk_t;

// typedef struct {
//     uint8_t chunkType; // DELTA_CHUNK or UNI_CHUNK
//     uint8_t baseHash[CHUNK_HASH_SIZE];
//     uint8_t chunkData[MAX_CHUNK_SIZE];
//     uint32_t chunkSize;
//     uint8_t is_find_end;
// } phase9MQ_t;

// typedef struct {
//     int msgTypeNum; // the num of message type in this network data batch
//     uint8_t msgType[MAX_MSG_TYPE_IN_BATCH]; // the message types
//     uint32_t curItemNum[MAX_MSG_TYPE_IN_BATCH]; // the current item num of each type
//     uint32_t dataSize; // total datasize of this network data batch 
// } SyncNetworkHead_t;

typedef struct {
    uint8_t segHash[CHUNK_HASH_SIZE];
} SegFP_t;

typedef struct {
    uint8_t recipeName[CHUNK_HASH_SIZE * 2];
    uint64_t offset;
    uint64_t chunkNum;
} SegIndexValue_t;

typedef struct {
    uint64_t idx;
    uint8_t baseHash[CHUNK_HASH_SIZE];
} EnclaveFeatureIndexValue_t;

// typedef struct {
//     uint8_t dedupFlag;
//     uint8_t segHash[CHUNK_HASH_SIZE];
//     SegIndexValue_t value;
// } OutSegQueryEntry_t;

// typedef struct {
//     uint32_t queryNum;
//     OutSegQueryEntry_t* OutSegQueryBase;
// } OutSegQuery_t;

typedef struct {
    uint8_t dedupFlag;
    uint8_t existFlag;
    uint8_t chunkHash[CHUNK_HASH_SIZE];
    RecipeEntry_t value;
} OutChunkQueryEntry_t;

typedef struct {
    uint32_t queryNum;
    OutChunkQueryEntry_t* OutChunkQueryBase;
} OutChunkQuery_t;

typedef struct {
    uint8_t dedupFlag;
    uint64_t features[SUPER_FEATURE_PER_CHUNK];
    uint8_t baseHash[CHUNK_HASH_SIZE];
} OutFeatureQueryEntry_t;

typedef struct {
    uint32_t queryNum;
    OutFeatureQueryEntry_t* OutFeatureQueryBase;
} OutFeatureQuery_t;

typedef struct {
    uint64_t sendChunkBatchSize;
    uint64_t sendRecipeBatchSize;
    uint64_t sendMetaBatchSize;
    uint64_t enclaveCacheItemNum;
} SyncEnclaveConfig_t;

#endif