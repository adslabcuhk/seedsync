/**
 * @file ecallStreamFeature.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-06-05
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/ecallStreamFeature.h"

/**
 * @brief Construct a new Ecall Stream Feature object
 * 
 */
EcallStreamFeature::EcallStreamFeature() {
    crypto_util_ = new EcallCrypto(CIPHER_TYPE, HASH_TYPE);
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    md_ctx_ = EVP_MD_CTX_new();

    plain_unifp_list_ = (uint8_t*)malloc(SyncEnclave::send_meta_batch_size_ * CHUNK_HASH_HMAC_SIZE);

    memset(session_key_, 0, CHUNK_HASH_HMAC_SIZE);

    SyncEnclave::Logging(my_name_.c_str(), "init the EcallStreamFeature.\n");
}

/**
 * @brief Destroy the Ecall Stream Feature object
 * 
 */
EcallStreamFeature::~EcallStreamFeature() {
    delete(crypto_util_);
    EVP_CIPHER_CTX_free(cipher_ctx_);
    EVP_MD_CTX_free(md_ctx_);

    free(plain_unifp_list_);
}

/**
 * @brief process the batch of feature req
 * 
 * @param unifp_list 
 * @param unifp_num 
 * @param req_container 
 * @param feature_list 
 * @param feature_num 
 */
void EcallStreamFeature::ProcessBatch(uint8_t* unifp_list, uint32_t unifp_num,
    ReqContainer_t* req_container, OutChunkQuery_t* addr_query,
    uint8_t* feature_list, size_t feature_num) {

    // decrypt the batch with session key first
    // crypto_util_->DecryptWithKey(cipher_ctx_, unifp_list, unifp_num * CHUNK_HASH_HMAC_SIZE,
    //     session_key_, plain_unifp_list_);
    // debug
    memcpy(plain_unifp_list_, unifp_list, unifp_num * CHUNK_HASH_HMAC_SIZE);

    uint8_t* id_buf = req_container->idBuffer;
    uint8_t** container_array = req_container->containerArray;

    string tmp_container_id;
    unordered_map<string, uint32_t> tmp_container_map;
    tmp_container_map.reserve(CONTAINER_CAPPING_VALUE);

    // uint64_t chunk_features[SUPER_FEATURE_PER_CHUNK];
    uint32_t write_feature_offset = 0;

    // query the chunk index to get the chunk Addr
    OutChunkQueryEntry_t* tmp_query_entry = addr_query->OutChunkQueryBase;
    uint32_t query_num = 0;
    uint32_t fplist_offset = 0;

    // SyncEnclave::Logging("unifp num ", "%d\n", unifp_num);

    for (size_t i = 0; i < unifp_num; i++) {
        crypto_util_->IndexAESCMCEnc(cipher_ctx_, plain_unifp_list_ + fplist_offset,
            CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
        fplist_offset += CHUNK_HASH_HMAC_SIZE;
        query_num ++;
        tmp_query_entry ++;
    }
    // reset offset
    fplist_offset = 0;

    addr_query->queryNum = query_num;
    Ocall_QueryChunkIndex(addr_query);
    
    // point back to base
    tmp_query_entry = addr_query->OutChunkQueryBase;
    RecipeEntry_t dec_query_entry;
    EnclaveRecipeEntry_t tmp_local_addr_entry;
    for (size_t i = 0; i < addr_query->queryNum; i++) {
        crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->value, 
            sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
            (uint8_t*)&dec_query_entry);
        
        // SyncEnclave::Logging("addr offset ", "%d\n", dec_query_entry.offset);
        // SyncEnclave::Logging("addr len ", "%d\n", dec_query_entry.length);
        // Ocall_PrintfBinary(dec_query_entry.containerName, CONTAINER_ID_LENGTH);

        _total_unique_num ++;
        _total_unique_size += dec_query_entry.length;
        
        // get the chunk addr
        tmp_container_id.assign((char*)dec_query_entry.containerName, 
            CONTAINER_ID_LENGTH);
        // prepare the local addr entry
        tmp_local_addr_entry.offset = dec_query_entry.offset;
        tmp_local_addr_entry.length = dec_query_entry.length;
        
        auto find_cont = tmp_container_map.find(tmp_container_id);
        if (find_cont == tmp_container_map.end()) {
            // unique container for load buffer
            // SyncEnclave::Logging("uni container","\n");
            tmp_local_addr_entry.containerID = req_container->idNum;
            tmp_container_map[tmp_container_id] = req_container->idNum;
            memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
                tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
            
            // uint8_t check[CONTAINER_ID_LENGTH];
            // memcpy(check, id_buf + req_container->idNum * CONTAINER_ID_LENGTH, CONTAINER_ID_LENGTH);
            // Ocall_PrintfBinary(check, CONTAINER_ID_LENGTH);
            
            req_container->idNum ++;
        }
        else {
            // exist in load buffer, get the local id
            // SyncEnclave::Logging("exist container","\n");
            tmp_local_addr_entry.containerID = find_cont->second;
        }
        local_addr_list_.push_back(tmp_local_addr_entry);

        // SyncEnclave::Logging("req container num ","%d\n", req_container->idNum);
        // SyncEnclave::Logging("local addr num ","%d\n", local_addr_list_.size());
        if (req_container->idNum == CONTAINER_CAPPING_VALUE) {
            // fetch containers
            Ocall_SyncGetReqContainer((void*)req_container);
            // SyncEnclave::Logging("after ocall req cont","\n");
            // read chunk from the encrypted container buffer
            for (size_t k = 0; k < local_addr_list_.size(); k++) {
                // // get chunk data one by one
                // uint32_t local_id = local_addr_list_[k].containerID;
                // uint32_t local_offset = local_addr_list_[k].offset;
                // uint32_t local_size = local_addr_list_[k].length;
                // // get the metaoffset of the container
                // uint32_t meta_offset = 0;
                // memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
                // meta_offset += sizeof(uint32_t);
                // uint8_t* local_chunk_data = container_array[local_id] + local_offset + meta_offset;
                
                // get the features from metadata session
                uint32_t local_id = local_addr_list_[k].containerID;
                string req_fp;
                req_fp.assign((char*)plain_unifp_list_ + fplist_offset, CHUNK_HASH_HMAC_SIZE);
                fplist_offset += CHUNK_HASH_HMAC_SIZE;
                // find the features based on fp
                uint32_t meta_offset = 0;
                uint32_t meta_num = 0;
                memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
                meta_num = meta_offset / (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                uint8_t* meta_session = container_array[local_id] + sizeof(uint32_t);

                // point to the first fp
                uint32_t read_meta_offset = SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
                uint8_t check_hash_req[CHUNK_HASH_HMAC_SIZE];
                uint8_t check_hash_read[CHUNK_HASH_HMAC_SIZE];
                for (size_t j = 0; j < meta_num; j++) {
                    // get the chunk fp in meta session one by one
                    string read_fp;
                    read_fp.assign((char*)meta_session + read_meta_offset, CHUNK_HASH_HMAC_SIZE);

                    if (read_fp == req_fp) {
                        // get the features
                        size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);

                        // // debug
                        // memcpy((char*)check_features, meta_session +
                        //     read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                        // memcpy(check_hash_read, read_fp.c_str(), CHUNK_HASH_HMAC_SIZE);
                        // memcpy(check_hash_req, req_fp.c_str(), CHUNK_HASH_HMAC_SIZE);

                        // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
                        //     SyncEnclave::Logging("check feature ", "%d\t", check_features[j]);
                        // }
                        // Ocall_PrintfBinary(check_hash_req, CHUNK_HASH_HMAC_SIZE);
                        // Ocall_PrintfBinary(check_hash_read, CHUNK_HASH_HMAC_SIZE);

                        // reuse input buffer
                        memcpy((char*)unifp_list + write_feature_offset, 
                            meta_session + read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                        write_feature_offset += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                        memcpy((char*)unifp_list + write_feature_offset, &read_fp[0], CHUNK_HASH_HMAC_SIZE);
                        write_feature_offset += CHUNK_HASH_HMAC_SIZE;
                        break;
                    }

                    // point to the next fp
                    read_meta_offset += (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                }
            }

            // reset
            req_container->idNum = 0;
            tmp_container_map.clear();
            local_addr_list_.clear();
        }
        // move on
        tmp_query_entry ++;
    }

    // deal with tail
    if (req_container->idNum != 0) {
        // fetch containers
        // SyncEnclave::Logging("fetch containers","%d\n", req_container->idNum);
        // for (size_t i = 0; i < req_container->idNum; i++) {
        //     Ocall_PrintfBinary(id_buf + i * CONTAINER_ID_LENGTH, CONTAINER_ID_LENGTH);
        // }
        Ocall_SyncGetReqContainer((void*)req_container);
        // SyncEnclave::Logging("after ocall req cont","\n");
        // read chunk from the encrypted container buffer
        for (size_t k = 0; k < local_addr_list_.size(); k++) {
            // // get chunk data one by one
            // uint32_t local_id = local_addr_list_[k].containerID;
            // uint32_t local_offset = local_addr_list_[k].offset;
            // uint32_t local_size = local_addr_list_[k].length;
            // // get the metaoffset of the container
            // uint32_t meta_offset = 0;
            // memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
            // meta_offset += sizeof(uint32_t);
            // uint8_t* local_chunk_data = container_array[local_id] + local_offset + meta_offset;
            
            // get the features from metadata session
            uint32_t local_id = local_addr_list_[k].containerID;
            string req_fp;
            req_fp.assign((char*)plain_unifp_list_ + fplist_offset, CHUNK_HASH_HMAC_SIZE);
            fplist_offset += CHUNK_HASH_HMAC_SIZE;
            // find the features based on fp
            uint32_t meta_offset = 0;
            uint32_t meta_num = 0;
            memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
            // SyncEnclave::Logging("metaoffset ","%d\n", meta_offset);
            meta_num = meta_offset / (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
            // SyncEnclave::Logging("metanum ","%d\n", meta_num);
            // SyncEnclave::Logging("entrysize ","%d\n", (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));

            uint8_t* meta_session = container_array[local_id] + sizeof(uint32_t);

            // point to the first fp
            uint32_t read_meta_offset = SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
            // // point to the first feature
            // uint32_t read_feature_offset = 0;
            // uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
            // uint8_t check_hash_req[CHUNK_HASH_HMAC_SIZE];
            // uint8_t check_hash_read[CHUNK_HASH_HMAC_SIZE];
            for (size_t j = 0; j < meta_num; j++) {
                // get the chunk fp in meta session one by one
                string read_fp;
                read_fp.assign((char*)meta_session + read_meta_offset, CHUNK_HASH_HMAC_SIZE);
                // SyncEnclave::Logging("read fp offset ","%d\n", read_meta_offset);

                if (read_fp == req_fp) {
                    // get the features
                    // size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                    size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                    // SyncEnclave::Logging("read feature offset ","%d\n", read_feature_offset);

                    // // debug
                    // memcpy((char*)check_features, meta_session +
                    //     read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                    // memcpy(check_hash_read, read_fp.c_str(), CHUNK_HASH_HMAC_SIZE);
                    // memcpy(check_hash_req, req_fp.c_str(), CHUNK_HASH_HMAC_SIZE);

                    // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
                    //     SyncEnclave::Logging("check feature ", "%d\t", check_features[j]);
                    // }
                    // Ocall_PrintfBinary(check_hash_req, CHUNK_HASH_HMAC_SIZE);
                    // Ocall_PrintfBinary(check_hash_read, CHUNK_HASH_HMAC_SIZE);

                    // reuse input buffer
                    memcpy((char*)unifp_list + write_feature_offset, 
                        meta_session + read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                    write_feature_offset += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                    memcpy((char*)unifp_list + write_feature_offset, &read_fp[0], CHUNK_HASH_HMAC_SIZE);
                    write_feature_offset += CHUNK_HASH_HMAC_SIZE;

                    break;
                }

                // point to the next fp
                read_meta_offset += (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                // SyncEnclave::Logging("skipsize ","%d\n", (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
            }
        }

        // reset
        req_container->idNum = 0;
        tmp_container_map.clear();
        local_addr_list_.clear(); 
    }

    // encrypt the feature list with session key
    // crypto_util_->EncryptWithKey(cipher_ctx_, unifp_list, write_feature_offset, 
    //     session_key_, feature_list);
    // debug
    memcpy(feature_list, unifp_list, write_feature_offset);

    // feature_num = write_feature_offset / (SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + CHUNK_HASH_HMAC_SIZE);
    // SyncEnclave::Logging("feature num in header ", "%d, %d, %d\n",write_feature_offset, feature_num, write_feature_offset / (SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + CHUNK_HASH_HMAC_SIZE));

    return ;
}