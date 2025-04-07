/**
 * @file ecallStreamBaseHash.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-06-06
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ECALL_STREAM_BASE_HASH_H
#define ECALL_STREAM_BASE_HASH_H

#include "commonEnclave.h"
#include "ecallEnc.h"

#include "../../../include/constVar.h"
#include "../../../include/chunkStructure.h"
#include "localityCache.h"

class EcallCrypto;

class EcallStreamBaseHash {
    private:
        string my_name_ = "EcallStreamBaseHash";

        // for crypto
        EcallCrypto* crypto_util_;
        EVP_CIPHER_CTX* cipher_ctx_;
        EVP_MD_CTX* md_ctx_;

        // session key
        uint8_t session_key_[CHUNK_HASH_SIZE];

        // dec buf
        uint8_t* plain_feature_fp_list_;

        // enc buf
        uint8_t* plain_out_list_;

        // in-enclave cache
        LocalityCache* enclave_locality_cache_;

        // the local feature index for the current batch: key=feature; value=feature_id
        unordered_map<uint64_t, string> batch_feature_index_;

        /**
         * @brief detect similar chunk within the local batch
         * 
         * @param features 
         * @param chunkHash 
         * @param baseHash 
         * @return true 
         * @return false 
         */
        bool LocalDetect(uint64_t* features, uint8_t* chunkHash, uint8_t* baseHash);

        /**
         * @brief query the local feature index
         * 
         * @param feature 
         * @param baseHash 
         * @return true 
         * @return false 
         */
        bool QueryLocalFeature(uint64_t feature, string& baseHash);

    public:
        // for logs
        uint64_t _enclave_similar_num = 0;
        uint64_t _batch_simlar_num = 0;
        uint64_t _non_similar_num = 0;

        /**
         * @brief Construct a new Ecall Stream Base Hash object
         * 
         * @param enclave_locality_cache 
         */
        EcallStreamBaseHash(LocalityCache* enclave_locality_cache);

        /**
         * @brief Destroy the Ecall Stream Base Hash object
         * 
         */
        ~EcallStreamBaseHash();

        /**
         * @brief process a batch of feature list
         * 
         * @param feature_fp_list 
         * @param feature_num 
         * @param out_list 
         * @param out_num 
         */
        void ProcessBatch(uint8_t* feature_fp_list, size_t feature_num, 
            uint8_t* out_list, size_t out_num);
};


#endif