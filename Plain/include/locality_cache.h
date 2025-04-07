/**
 * @file localityCache.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-16
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef PLAIN_LOCALITY_CACHE_H
#define PLAIN_LOCALITY_CACHE_H

// #include "commonEnclave.h"
// for std using inside the enclave
// #include "list"
// #include "mutex"
// #include "queue"
// #include "set"
// #include "stdbool.h"
// #include "stdint.h"
// #include "stdio.h"
// #include "stdlib.h"
// #include "string.h"
// #include "unordered_map" // for deduplication index inside the enclave
// #include "unordered_set"
// #include "vector" // for heap

#include "chunkStructure.h"
#include "constVar.h"
#include "lruCache.h"
#include "define.h"

using namespace std;

typedef struct {
    uint8_t containerID[CONTAINER_ID_LENGTH];
    // uint8_t chunkHash[CHUNK_HASH_HMAC_SIZE];
    uint64_t features[SUPER_FEATURE_PER_CHUNK];
} EnclaveContainerID_t;

typedef struct {
    uint8_t baseHash[CHUNK_HASH_HMAC_SIZE];
} EnclaveValueBaseFP_t;


class LocalityCache {
    private:
        string my_name_ = "LocalityCache";

        // local FP index: key = FP; value = containerID + features;
        unordered_map<string, EnclaveContainerID_t> local_fp_index_;

        // local feature index: key = feature; value = containerID
        // unordered_map<uint64_t, EnclaveContainerID_t> local_feature_index_;

        // local feature index: key = feature; value = baseHash
        unordered_map<uint64_t, EnclaveValueBaseFP_t> local_feature_index_;

        // lru map: key = containerID; value = memory pool idx
        lru11::Cache<string, uint32_t>* container_item_;

        mutex enclave_cache_lck_;

        // current idx num
        uint32_t cur_idx_num_;

        uint32_t max_cache_size_;

        // memory pool
        uint8_t** memory_pool_;

        /**
         * @brief insert a new container into locality cache
         * 
         * @param insert_container 
         * @param idx 
         */
        void InsertContainer(Container_t* insert_container, uint32_t idx);
    
        /**
         * @brief query the local feature index
         * 
         * @param features 
         * @return true 
         * @return false 
         */
        bool QueryFeature(uint64_t feature, string& baseHash);

    public:
        // for logs
        uint64_t _total_evict_num = 0;

        /**
         * @brief Construct a new Locality Cache object
         * 
         * @param max_cache_size 
         */
        LocalityCache(uint32_t max_cache_size);

        /**
         * @brief Construct a new Locality Cache object
         * 
         */
        LocalityCache();

        /**
         * @brief Destroy the Locality Cache object
         * 
         */
        ~LocalityCache();

        /**
         * @brief locality cache insertion
         * 
         * @param insert_container 
         */
        void InsertCache(Container_t* insert_container);

        /**
         * @brief insert a batch of containers into locality cache
         * 
         * @param req_container 
         */
        void BatchInsertCache(ReqContainer_t* req_container);

        /**
         * @brief query the local fp index
         * 
         * @param chunkHash 
         * @param containerID 
         * @param features 
         * @return true 
         * @return false 
         */
        bool QueryFP(string chunkHash, string& containerID, uint64_t* features);

        /**
         * @brief find the base chunk hash
         * 
         * @param features 
         * @param baseHash 
         * @return true 
         * @return false 
         */
        bool FindLocalBaseChunk(uint64_t* features, uint8_t* baseHash);

        /**
         * @brief get the current cache size
         * 
         * @return uint32_t 
         */
        inline uint32_t GetCacheSize() {
            return max_cache_size_;
        }
        
};


#endif