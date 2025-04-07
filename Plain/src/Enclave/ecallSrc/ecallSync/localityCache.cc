/**
 * @file localityCache.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-16
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/localityCache.h"

/**
 * @brief Construct a new Locality Cache object
 * 
 * @param max_cache_size 
 */
LocalityCache::LocalityCache(uint32_t max_cache_size) {
    max_cache_size_ = max_cache_size;
    container_item_ = new lru11::Cache<string, uint32_t>(max_cache_size_);
    
    memory_pool_ = (uint8_t**) malloc(max_cache_size_ * sizeof(uint8_t*));
    for (size_t i = 0; i < max_cache_size_; i++) {
        memory_pool_[i] = (uint8_t*) malloc(MAX_META_SIZE);
    }

    cur_idx_num_ = 0;

    local_fp_index_.reserve(MAX_META_SIZE / (CHUNK_HASH_HMAC_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK));
    local_feature_index_.reserve((MAX_META_SIZE / (CHUNK_HASH_HMAC_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK))
        * SUPER_FEATURE_PER_CHUNK);
}

/**
 * @brief Construct a new Locality Cache object
 * 
 */
LocalityCache::LocalityCache() {
    max_cache_size_ = 512;
    size_t elasticity = 0;
    container_item_ = new lru11::Cache<string, uint32_t>(max_cache_size_, elasticity);
    
    memory_pool_ = (uint8_t**) malloc(max_cache_size_ * sizeof(uint8_t*));
    for (size_t i = 0; i < max_cache_size_; i++) {
        memory_pool_[i] = (uint8_t*) malloc(MAX_META_SIZE);
    }

    cur_idx_num_ = 0;

    local_fp_index_.reserve(MAX_META_SIZE / (CHUNK_HASH_HMAC_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK));
    local_feature_index_.reserve((MAX_META_SIZE / (CHUNK_HASH_HMAC_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK))
        * SUPER_FEATURE_PER_CHUNK);

    SyncEnclave::Logging("max cache size = ", "%d\n", max_cache_size_);
}

/**
 * @brief Destroy the Locality Cache object
 * 
 */
LocalityCache::~LocalityCache() {
    delete container_item_;
    for (size_t i = 0; i < max_cache_size_; i++) {
        free(memory_pool_[i]);
    }
    free(memory_pool_);

    local_fp_index_.clear();
    local_feature_index_.clear();
}

/**
 * @brief locality cache insertion
 * 
 * @param insert_container 
 */
void LocalityCache::InsertCache(Container_t* insert_container) {
// #if (NAIVE_CACHE == 1)
    SyncEnclave::enclave_cache_lck_.lock();
// #endif

    // check whether the cache is full
    if (container_item_->size() + 1 > max_cache_size_) {
        // evict a container from cache
        // SyncEnclave::Logging("evict before insert: cache size ", "%d\n", container_item_->size());
        size_t evict_idx = container_item_->pruneValue();
        
        // SyncEnclave::Logging("cache full, evict before insert", "%d\n", evict_idx);

        _total_evict_num ++;

        // uint32_t meta_size;
        // memcpy(&meta_size, memory_pool_[evict_idx], sizeof(uint32_t));
        // uint32_t offset = sizeof(uint32_t);
        // uint32_t meta_num = meta_size / (CHUNK_HASH_HMAC_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
        // uint32_t meta_num = insert_container->currentMetaSize;
        // SyncEnclave::Logging("meta_num (to evict)", "%d\n", insert_container->currentMetaSize);

        uint32_t evict_meta_size;
        memcpy(&evict_meta_size, memory_pool_[evict_idx], sizeof(uint32_t));
        uint32_t offset = sizeof(uint32_t);
        uint32_t evict_meta_num = evict_meta_size / (CHUNK_HASH_HMAC_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
        // SyncEnclave::Logging("meta_num (to evict)", "%d\n", evict_meta_num);

        string evict_hash;
        uint64_t evict_feature[SUPER_FEATURE_PER_CHUNK];
        for (size_t i = 0; i < evict_meta_num; i++) {
            // get evict features
            memcpy((char*)evict_feature, memory_pool_[evict_idx] + offset, 
                sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
            offset += sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK;
            // get evict hash
            evict_hash.assign((char*)memory_pool_[evict_idx] + offset, CHUNK_HASH_HMAC_SIZE);
            offset += CHUNK_HASH_HMAC_SIZE;

            // remove feature index entries
            for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
                local_feature_index_.erase(evict_feature[j]);
            }

            // remove fp index entries
            local_fp_index_.erase(evict_hash);
        }

        // pass the evict_idx as the pos of writing a new container
        // InsertContainer(containerID, containerData, containerSize, evict_idx);
        InsertContainer(insert_container, evict_idx);
    }
    else {
        // insert a new container into cache
        // InsertContainer(containerID, containerData, containerSize, cur_idx_num_);
        InsertContainer(insert_container, cur_idx_num_);

        // SyncEnclave::Logging("insert without evict", "%d\n", cur_idx_num_);

        cur_idx_num_ ++;
    }

// #if (NAIVE_CACHE == 1)
    SyncEnclave::enclave_cache_lck_.unlock();
// #endif

    return ;
}

/**
 * @brief insert a batch of containers into locality cache
 * 
 * @param req_container 
 */
void LocalityCache::BatchInsertCache(ReqContainer_t* req_container) {
    // SyncEnclave::enclave_cache_lck_.lock();

    SyncEnclave::Logging("batch insert cache", "%d\n", req_container->idNum);
    for (size_t i = 0; i < req_container->idNum; i++) {

        if (req_container->sizeArray[i] > 0) {
            // prepare the Container_t structure
            Container_t tmp_container;
            
            // get the metadata
            uint32_t meta_size;
            memcpy((uint8_t*)&meta_size, req_container->containerArray[i], sizeof(uint32_t));
            SyncEnclave::Logging("check meta_size", "%d\n", meta_size);
            tmp_container.currentMetaSize = meta_size;
            memcpy(tmp_container.metadata, req_container->containerArray[i] + sizeof(uint32_t), meta_size);
            // get the id
            memcpy(tmp_container.containerID, req_container->idBuffer + i * CONTAINER_ID_LENGTH, CONTAINER_ID_LENGTH);

            SyncEnclave::Logging("before insert cache", "\n");

            // evict the cache
            this->InsertCache(&tmp_container);

            SyncEnclave::Logging("after insert cache", "\n");
        }
        else {
            SyncEnclave::Logging("empty container", "%d\n", i);
        }
    }

    // SyncEnclave::enclave_cache_lck_.unlock();

    return ;
}


/**
 * @brief insert a new container into locality cache
 * 
 * @param insert_container 
 * @param idx 
 */
void LocalityCache::InsertContainer(Container_t* insert_container, uint32_t idx) {
    // first get metadata session size
    // uint32_t meta_size;
    // memcpy(&meta_size, containerData, sizeof(uint32_t));
    // uint32_t offset = sizeof(uint32_t);

    // uint32_t meta_num = meta_size / (CHUNK_HASH_HMAC_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
    // SyncEnclave::Logging("meta_num", "%d\n", insert_container->currentMetaSize);
    uint32_t meta_num = insert_container->currentMetaSize / (CHUNK_HASH_HMAC_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);

    // SyncEnclave::Logging("in insertcontainer", "%d\n", meta_num);

    // uint32_t offset = sizeof(uint32_t);
    uint32_t offset = 0;
    string tmp_hash_str;
    uint64_t tmp_feature[SUPER_FEATURE_PER_CHUNK];
    for (size_t i = 0; i < meta_num; i++) {
        // first get features
        memcpy((char*)tmp_feature, insert_container->metadata + offset, sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
        offset += sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK;
        // get fp
        tmp_hash_str.assign((char*)insert_container->metadata + offset, CHUNK_HASH_HMAC_SIZE);
        offset += CHUNK_HASH_HMAC_SIZE;

        memcpy((uint8_t*)local_fp_index_[tmp_hash_str].features, (uint8_t*)tmp_feature,
            sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);

        // insert feature index
        for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
            // debug
            // SyncEnclave::Logging("insert feature in cache: ", "%d\t", tmp_feature[j]);

            memcpy(local_feature_index_[tmp_feature[j]].baseHash, tmp_hash_str.c_str(),
                CHUNK_HASH_HMAC_SIZE);

            // Ocall_PrintfBinary(local_feature_index_[tmp_feature[j]].baseHash, CHUNK_HASH_HMAC_SIZE);

            // // copy features to value of fp index
            // memcpy(local_fp_index_[tmp_hash_str].features + j * sizeof(uint64_t), 
            //     (char*)tmp_feature + j * sizeof(uint64_t), sizeof(uint64_t));
            
            // SyncEnclave::Logging("insert feature in cache: ", "%d\t", local_fp_index_[tmp_hash_str].features[j]);
        }


        // SyncEnclave::Logging(" ", "\n");

        // copy container ID to fp index value
        memcpy(local_fp_index_[tmp_hash_str].containerID, insert_container->containerID, 
            CONTAINER_ID_LENGTH);

        // SyncEnclave::Logging("insert fp in cache ", "idx = %d\n", idx);
        // Ocall_PrintfBinary((uint8_t*)tmp_hash_str.c_str(), CHUNK_HASH_HMAC_SIZE);
        // Ocall_PrintfBinary((uint8_t*)insert_container->containerID, CONTAINER_ID_LENGTH);

        // insert memory pool
        // memcpy(memory_pool_[idx], containerData, containerSize);
        uint32_t copy_offset = 0;
        memcpy(memory_pool_[idx], (uint8_t*)&insert_container->currentMetaSize, sizeof(uint32_t));
        copy_offset += sizeof(uint32_t);
        memcpy(memory_pool_[idx] + copy_offset, insert_container->metadata, insert_container->currentMetaSize);
        // copy_offset += insert_container->currentMetaSize;
        // memcpy(memory_pool_[idx] + copy_offset, insert_container->body, insert_container->currentSize);

        string containerID;
        containerID.assign((char*)insert_container->containerID, CONTAINER_ID_LENGTH);
        container_item_->insert(containerID, idx);
    }

    return ;
}

/**
 * @brief query the local fp index
 * 
 * @param chunkHash 
 * @param containerID 
 * @param features 
 * @return true 
 * @return false 
 */
bool LocalityCache::QueryFP(string chunkHash, string& containerID, uint64_t* features) {
    SyncEnclave::enclave_cache_lck_.lock();

    // SyncEnclave::Logging("in query fp", "%d\n", local_fp_index_.size());

    if (local_fp_index_.find(chunkHash) != local_fp_index_.end()) {

        // SyncEnclave::Logging("found", "\n");

        string container_id;
        container_id.assign((char*)local_fp_index_[chunkHash].containerID, 
            CONTAINER_ID_LENGTH);

        containerID.assign((char*)local_fp_index_[chunkHash].containerID, 
            CONTAINER_ID_LENGTH);
        memcpy(features, local_fp_index_[chunkHash].features, 
            sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
        
        // SyncEnclave::Logging("after memcpy", "\n");


        // !!!!! the illegal instruction occurs in the following "get"
        // TODO: check the containerID
        // Ocall_PrintfBinary((uint8_t*)container_id.c_str(), CONTAINER_ID_LENGTH);

        bool flag = false;
        flag = container_item_->contains(container_id);
        if (!flag)
            SyncEnclave::Logging("containerid does not exist in lru: flag ", "%d\n", flag);

        // cannot find this container_id

        // update the lru map
        if (flag) {
            container_item_->get(container_id);
        }

        // SyncEnclave::Logging("after get", "\n");
        
        SyncEnclave::enclave_cache_lck_.unlock();

        return true;
    }

    // SyncEnclave::Logging("not found", "\n");

    SyncEnclave::enclave_cache_lck_.unlock();

    return false;
}

/**
 * @brief query the feature index in cache
 * 
 * @param features 
 * @return true 
 * @return false 
 */
bool LocalityCache::QueryFeature(uint64_t feature, string& baseHash) {       
    SyncEnclave::enclave_cache_lck_.lock();

    if (local_feature_index_.find(feature) != local_feature_index_.end()) {
        baseHash.assign((char*)local_feature_index_[feature].baseHash, CHUNK_HASH_HMAC_SIZE);
        // // debug
        // SyncEnclave::Logging("in query feature, check basehash ", "\n");
        // Ocall_PrintfBinary(local_feature_index_[feature].baseHash, CHUNK_HASH_HMAC_SIZE);

        SyncEnclave::enclave_cache_lck_.unlock();
        
        return true;
    }

    SyncEnclave::enclave_cache_lck_.unlock();
    return false;
}

bool CacheCmp(pair<string, uint32_t>& a, pair<string, uint32_t>& b) {
    return a.second > b.second;
}

/**
 * @brief find the base chunk hash
 * 
 * @param features 
 * @param baseHash 
 * @return true 
 * @return false 
 */
bool LocalityCache::FindLocalBaseChunk(uint64_t* features, uint8_t* baseHash) {
    bool is_similar = false;
    // uint8_t tmp_base_hash[CHUNK_HASH_HMAC_SIZE];
    string tmp_base_hash;
    bool is_first_match = true;
    uint8_t first_match_base_hash[CHUNK_HASH_HMAC_SIZE];
    unordered_map<string, uint32_t> base_freq_map;

    for (size_t i = 0; i < SUPER_FEATURE_PER_CHUNK; i++) {

        if (this->QueryFeature(features[i], tmp_base_hash)) {
            if (is_first_match) {

                memcpy(first_match_base_hash, &tmp_base_hash[0], CHUNK_HASH_HMAC_SIZE);
                is_first_match = false;
            }

            if (base_freq_map.find(tmp_base_hash) != base_freq_map.end()) {
                base_freq_map[tmp_base_hash] ++;
            }
            else {
                base_freq_map[tmp_base_hash] = 1;
            }

            is_similar = true;
        }
    }

    if (is_similar) {
        vector<pair<string, uint32_t>> tmp_freq_vec;
        for (auto it : base_freq_map) {
            tmp_freq_vec.push_back(it);
        }

        // sort freq
        sort(tmp_freq_vec.begin(), tmp_freq_vec.end(), CacheCmp);
        if (tmp_freq_vec[0].second == 1) {
            memcpy(baseHash, first_match_base_hash, CHUNK_HASH_HMAC_SIZE);
        }
        else {
            memcpy(baseHash, tmp_freq_vec[0].first.c_str(),
                CHUNK_HASH_HMAC_SIZE);
        }

        // // debug: check basehash output here
        // SyncEnclave::Logging("check base after detection", "\n");
        // uint8_t check_base[CHUNK_HASH_HMAC_SIZE];
        // memcpy(check_base, baseHash, CHUNK_HASH_HMAC_SIZE);
        // Ocall_PrintfBinary(check_base, CHUNK_HASH_HMAC_SIZE);

        // // check the features for similar chunks
        // uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
        // memcpy((uint8_t*)check_features, (uint8_t*)features, sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
        // SyncEnclave::Logging("features ", "%d\t%d\t%d\n", check_features[0], check_features[1], check_features[2]);

        // base_query->dedupFlag = ENCLAVE_SIMILAR_CHUNK;
        // SyncEnclave::Logging("enclave similar chunk", "\n");

        // for (size_t k = 0; k < SUPER_FEATURE_PER_CHUNK; k++) {
        //     SyncEnclave::Logging("similar feature ","%d\t", base_query->features[k]);
        // }
        // SyncEnclave::Logging(" ","\n");

        base_freq_map.clear();
        return true;
    }
    else {
        // Note: the cache miss of a feature do NOT lead to a cache insertion

        // base_query->dedupFlag = ENCLAVE_NON_SIMILAR_CHUNK;
        // insert the feature index with chunkhash as basehash
        // for (size_t i = 0; i < SUPER_FEATURE_PER_CHUNK; i++) {
        //     this->Insert(base_query->features[i], base_query->baseHash);
        // }

        // SyncEnclave::Logging("enclave non-similar chunk", "\n");
        // for (size_t k = 0; k < SUPER_FEATURE_PER_CHUNK; k++) {
        //     SyncEnclave::Logging("check feature ","%d\t", base_query->features[k]);
        // }
        // SyncEnclave::Logging(" ","\n");
        base_freq_map.clear();
        return false;
    }
}
