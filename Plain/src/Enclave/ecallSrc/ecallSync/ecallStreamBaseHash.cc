/**
 * @file ecallStreamBaseHash.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-06-06
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/ecallStreamBaseHash.h"

/**
 * @brief Construct a new Ecall Stream Base Hash object
 * 
 * @param enclave_locality_cache 
 */
EcallStreamBaseHash::EcallStreamBaseHash(LocalityCache* enclave_locality_cache) {
    crypto_util_ = new EcallCrypto(CIPHER_TYPE, HASH_TYPE);
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    md_ctx_ = EVP_MD_CTX_new();

    plain_feature_fp_list_ = (uint8_t*)malloc(SyncEnclave::send_chunk_batch_size_ * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
    plain_out_list_ = (uint8_t*) malloc(SyncEnclave::send_chunk_batch_size_ * sizeof(StreamPhase4MQ_t));

    enclave_locality_cache_ = enclave_locality_cache;

    batch_feature_index_.reserve(SyncEnclave::send_chunk_batch_size_ * SUPER_FEATURE_PER_CHUNK);

    memset(session_key_, 0, CHUNK_HASH_HMAC_SIZE);

    SyncEnclave::Logging(my_name_.c_str(), "init the StreamBaseHash.\n");
}

/**
 * @brief Destroy the Ecall Stream Base Hash object
 * 
 */
EcallStreamBaseHash::~EcallStreamBaseHash() {
    delete(crypto_util_);
    EVP_CIPHER_CTX_free(cipher_ctx_);
    EVP_MD_CTX_free(md_ctx_);

    free(plain_feature_fp_list_);
    free(plain_out_list_);
}

/**
 * @brief process a batch of feature list
 * 
 * @param feature_fp_list 
 * @param feature_num 
 * @param out_list 
 * @param out_num 
 */
void EcallStreamBaseHash::ProcessBatch(uint8_t* feature_fp_list, size_t feature_num, 
    uint8_t* out_list, size_t out_num) {
    // SyncEnclave::Logging(my_name_.c_str(), "in ecallStreamBaseHash. %d\n", feature_num);
    // decrypt the feature list with session key
    // crypto_util_->DecryptWithKey(cipher_ctx_, feature_fp_list, feature_num *
    //     (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)),
    //     session_key_, plain_feature_fp_list_);
    // // debug
    // SyncEnclave::Logging("dec size ", "%d\n", feature_num *
    //     (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
    memcpy(plain_feature_fp_list_, feature_fp_list, feature_num * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
    // SyncEnclave::Logging("", "decrypted feature list.\n");

    StreamPhase3MQ_t in_entry;
    StreamPhase4MQ_t out_entry;
    uint32_t read_offset = 0;
    for (size_t i = 0; i < feature_num; i++) {
        memcpy(in_entry.features, plain_feature_fp_list_ + read_offset, 
            SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
        read_offset += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
        memcpy(in_entry.chunkHash, plain_feature_fp_list_ + read_offset, CHUNK_HASH_HMAC_SIZE);
        read_offset += CHUNK_HASH_HMAC_SIZE;

        // // debug
        // // uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
        // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
        //     SyncEnclave::Logging("check feature (base) ", "%d\t", in_entry.features[j]);
        // }

        /******************debugging here******************/
        // query the enclave feature index
        bool is_sim = enclave_locality_cache_->FindLocalBaseChunk(in_entry.features, out_entry.baseHash);

        if (is_sim) {
            // similar chunk for enclave

            // debug
            // out_entry.sim_tag = NON_SIMILAR_CHUNK;
            out_entry.sim_tag = SIMILAR_CHUNK;
            memcpy(out_entry.chunkHash, in_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
            // SyncEnclave::Logging("similar chunk", "\n");

            _enclave_similar_num ++;

            // debug check the basehash here
            // Ocall_PrintfBinary(out_entry.baseHash, CHUNK_HASH_HMAC_SIZE);
        }
        else {
            // non-similar chunk for enclave cache
            
            // TODO: detect local sim within this batch here
            bool batch_sim = LocalDetect(in_entry.features, in_entry.chunkHash, 
                out_entry.baseHash);

            if (batch_sim) {
                out_entry.sim_tag = BATCH_SIMILAR_CHUNK;
                memcpy(out_entry.chunkHash, in_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
                // SyncEnclave::Logging("batch similar chunk", "\n");

                _batch_simlar_num ++;

                // Ocall_PrintfBinary(out_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
                // Ocall_PrintfBinary(out_entry.baseHash, CHUNK_HASH_HMAC_SIZE);
            }
            else {
                out_entry.sim_tag = NON_SIMILAR_CHUNK;
                memcpy(out_entry.chunkHash, in_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
                // SyncEnclave::Logging("non-similar", "\n");

                _non_similar_num ++;
            }

            //             out_entry.sim_tag = NON_SIMILAR_CHUNK;
            // memcpy(out_entry.chunkHash, in_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
        }

            // bool batch_sim = LocalDetect(in_entry.features, in_entry.chunkHash, 
            //     out_entry.baseHash);

        // if (batch_sim) {
        //     out_entry.sim_tag = BATCH_SIMILAR_CHUNK;
        //     memcpy(out_entry.chunkHash, in_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
        //     SyncEnclave::Logging("batch similar chunk", "\n");
        //     // Ocall_PrintfBinary(out_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
        //     // Ocall_PrintfBinary(out_entry.baseHash, CHUNK_HASH_HMAC_SIZE);
        // }
        // else {
        //     out_entry.sim_tag = NON_SIMILAR_CHUNK;
        //     memcpy(out_entry.chunkHash, in_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
        // }

        // copy the out_entry to out_list
        memcpy((char*)plain_out_list_ + i * sizeof(StreamPhase4MQ_t), (char*)&out_entry, 
            sizeof(StreamPhase4MQ_t));
    }

    out_num = feature_num;

    // encrypt out_list with session key
    // crypto_util_->EncryptWithKey(cipher_ctx_, plain_out_list_, out_num * sizeof(StreamPhase4MQ_t), 
    //     session_key_, out_list);

    // debug
    memcpy(out_list, plain_out_list_, out_num * sizeof(StreamPhase4MQ_t));

    // clear the batch index
    batch_feature_index_.clear();

    // SyncEnclave::Logging(my_name_.c_str(), "after ecallStreamBaseHash.\n");

    return ;
}

bool BatchCmp(pair<string, uint32_t>& a, pair<string, uint32_t>& b) {
    return a.second > b.second;
}

/**
 * @brief detect similar chunk within the local batch
 * 
 * @param features 
 * @param chunkHash 
 * @param baseHash 
 * @return true 
 * @return false 
 */
bool EcallStreamBaseHash::LocalDetect(uint64_t* features, uint8_t* chunkHash, 
    uint8_t* baseHash) {
    
    bool is_similar = false;
    // uint8_t tmp_base_hash[CHUNK_HASH_HMAC_SIZE];
    string tmp_base_hash;
    bool is_first_match = true;
    uint8_t first_match_base_hash[CHUNK_HASH_HMAC_SIZE];
    unordered_map<string, uint32_t> base_freq_map;

    for (size_t i = 0; i < SUPER_FEATURE_PER_CHUNK; i++) {

        if (this->QueryLocalFeature(features[i], tmp_base_hash)) {
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
        sort(tmp_freq_vec.begin(), tmp_freq_vec.end(), BatchCmp);
        if (tmp_freq_vec[0].second == 1) {
            memcpy(baseHash, first_match_base_hash, CHUNK_HASH_HMAC_SIZE);
        }
        else {
            memcpy(baseHash, tmp_freq_vec[0].first.c_str(),
                CHUNK_HASH_HMAC_SIZE);
        }

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

        // insert the batch map

        for (size_t k = 0; k < SUPER_FEATURE_PER_CHUNK; k++) {
            string insert_hash;
            insert_hash.assign((char*)chunkHash, CHUNK_HASH_HMAC_SIZE);
            batch_feature_index_.insert(std::make_pair(features[k], insert_hash));
        }

        base_freq_map.clear();
        return false;
    }
}

/**
 * @brief query the local feature index
 * 
 * @param feature 
 * @param baseHash 
 * @return true 
 * @return false 
 */
bool EcallStreamBaseHash::QueryLocalFeature(uint64_t feature, string& baseHash) {
    if (batch_feature_index_.find(feature) != batch_feature_index_.end()) {
        // baseHash.assign(batch_feature_index_[feature], CHUNK_HASH_HMAC_SIZE);
        baseHash.assign(batch_feature_index_[feature].c_str(), CHUNK_HASH_HMAC_SIZE);
        return true;
    }

    return false;
}