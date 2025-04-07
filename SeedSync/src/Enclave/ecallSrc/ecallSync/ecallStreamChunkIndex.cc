/**
 * @file ecallStreamChunkIndex.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/ecallStreamChunkIndex.h"

/**
 * @brief Construct a new Ecall Stream Chunk Index object
 * 
 * @param enclave_locality_cache 
 */
EcallStreamChunkIndex::EcallStreamChunkIndex(LocalityCache* enclave_locality_cache) {
    crypto_util_ = new EcallCrypto(CIPHER_TYPE, HASH_TYPE);
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    md_ctx_ = EVP_MD_CTX_new();

    memset(session_key_, 0, CHUNK_HASH_SIZE);

    plain_uni_fp_list_ = (uint8_t*)malloc(2 * SyncEnclave::send_meta_batch_size_ * CHUNK_HASH_SIZE);
    enclave_locality_cache_ = enclave_locality_cache;
#if (DEBUG_FLAG == 1)    
    SyncEnclave::Logging(my_name_.c_str(), "init the StreamChunkIndex.\n");
#endif    
}

/**
 * @brief Destroy the Ecall Stream Chunk Index object
 * 
 */
EcallStreamChunkIndex::~EcallStreamChunkIndex() {
    delete(crypto_util_);
    EVP_CIPHER_CTX_free(cipher_ctx_);
    EVP_MD_CTX_free(md_ctx_);

    free(plain_uni_fp_list_);
}

/**
 * @brief process a batch of fp list with batching stream cache
 * 
 * @param fp_list 
 * @param fp_num 
 * @param uni_fp_list 
 * @param uni_fp_num 
 * @param addr_query 
 * @param req_container 
 */
void EcallStreamChunkIndex::BatchStreamCache(uint8_t* fp_list, size_t fp_num, 
    uint8_t* uni_fp_list, size_t* uni_fp_num, OutChunkQuery_t* addr_query, 
    ReqContainer_t* req_container) {

    // decrypt the fp list with session key
    crypto_util_->DecryptWithKey(cipher_ctx_, fp_list, fp_num * CHUNK_HASH_SIZE,
        session_key_, plain_uni_fp_list_);

    // prepare the req container
    uint8_t* id_buf = req_container->idBuffer;
    uint8_t** container_array = req_container->containerArray;
    string tmp_container_id;
    unordered_map<string, uint32_t> tmp_container_map;
    tmp_container_map.reserve(CONTAINER_CAPPING_VALUE);

    std::vector<SyncRecipeEntry_t> local_addr_list;

    // record the current reuse pos in plain_uni_fp_list_
    uint32_t cur_reuse_offset = 0;

    // prepare the out query
    OutChunkQueryEntry_t* tmp_query_entry = addr_query->OutChunkQueryBase;
    uint32_t out_query_num = 0;

    // check the fp index in enclave cache
    for (size_t i = 0; i < fp_num; i++) {
        string fp_str;
        fp_str.assign((char*)plain_uni_fp_list_ + i * CHUNK_HASH_SIZE, CHUNK_HASH_SIZE);

        string enclave_query_container_id;
        uint64_t enclave_query_features[SUPER_FEATURE_PER_CHUNK];

        if (!enclave_locality_cache_->QueryFP(fp_str, enclave_query_container_id,
            enclave_query_features)) {
            // not exit the in enclave cache
            
            _enclave_unique_num ++;

#if (INDEX_ENC == 1)
            // prepare the out query
            crypto_util_->IndexAESCMCEnc(cipher_ctx_, (uint8_t*)&fp_str[0],
                CHUNK_HASH_SIZE, SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
            memcpy(tmp_query_entry->chunkHash, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
#endif            

            out_query_num ++;
            tmp_query_entry ++;

            if (out_query_num == 1) {
                // issue the out query
                addr_query->queryNum = out_query_num;
                Ocall_QueryChunkIndex((void*)addr_query);
                out_query_num = 0;

                // point back to base
                tmp_query_entry = addr_query->OutChunkQueryBase;
                RecipeEntry_t dec_query_entry;
                SyncRecipeEntry_t tmp_local_addr_entry;
                for (size_t i = 0; i < addr_query->queryNum; i++) {
                    // outside duplicate
                    if (tmp_query_entry->dedupFlag == DUPLICATE) {
                        _global_duplicate_num ++;

                        //  load the containers in enclave
#if (INDEX_ENC == 1)                        
                        crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->value, 
                            sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
                            (uint8_t*)&dec_query_entry);
#endif

#if (INDEX_ENC == 0)
                        memcpy((uint8_t*)&dec_query_entry, (uint8_t*)&tmp_query_entry->value, sizeof(RecipeEntry_t));
#endif                        

#if (DEBUG_FLAG == 1)
                        // debug
                        // SyncEnclave::Logging("check addr", "offset=%d; len=%d; type=%d\n", dec_query_entry.offset, dec_query_entry.length, tmp_query_entry->dedupFlag);
#endif                        

                        // prepare the addr in local_addr_list_ 
                        tmp_container_id.assign((char*)dec_query_entry.containerName, CONTAINER_ID_LENGTH);
                        tmp_local_addr_entry.offset = dec_query_entry.offset;
                        tmp_local_addr_entry.length = dec_query_entry.length;

                        auto find_cont = tmp_container_map.find(tmp_container_id);
                        if (find_cont == tmp_container_map.end()) {
                            tmp_local_addr_entry.containerID = req_container->idNum;
                            tmp_container_map[tmp_container_id] = req_container->idNum;
                            memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
                                tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
                            req_container->idNum ++;
                        }
                        else {
                            tmp_local_addr_entry.containerID = find_cont->second;
                        }
                        local_addr_list.push_back(tmp_local_addr_entry);

                        if (req_container->idNum > 0) {
                            // debug
                            // SyncEnclave::Logging("the req_cont idNum", "%d\n", req_container->idNum);

                            // fetch containers
                            Ocall_SyncGetReqContainerWithSize((void*)req_container);

                            // insert the containers into enclave cache
                            for (size_t k = 0; k < req_container->idNum; k++) {
                                if (req_container->sizeArray[k] > 0) {
                                    // prepare container
                                    Container_t tmp_container;

                                    uint32_t meta_size;
                                    memcpy((uint8_t*)&meta_size, req_container->containerArray[k], sizeof(uint32_t));
                                    // SyncEnclave::Logging("check the meta_size", "%d\n", meta_size);

                                    tmp_container.currentMetaSize = meta_size;
                                    memcpy(tmp_container.metadata, req_container->containerArray[k] + sizeof(uint32_t), meta_size);
                                    // get the id
                                    memcpy(tmp_container.containerID, req_container->idBuffer + k * CONTAINER_ID_LENGTH, CONTAINER_ID_LENGTH);

                                    enclave_locality_cache_->InsertCache(&tmp_container);

                                }
                                else {
                                    // SyncEnclave::Logging("the empty container", "%d\n", k);
                                }
                            }

                            // reset
                            req_container->idNum = 0;
                            tmp_container_map.clear();
                            local_addr_list.clear();
                        }
                    }
                    else {

                        memcpy(plain_uni_fp_list_ + cur_reuse_offset, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
                        cur_reuse_offset += CHUNK_HASH_SIZE;

                        _global_unique_num ++;
                    }

                    // move on
                    tmp_query_entry ++;
                }
            }
        }
        else {
            _enclave_duplicate_num ++;
        }
    }

    // deal with the tail
    if (out_query_num != 0) {
        // issue the out query
        addr_query->queryNum = out_query_num;
        Ocall_QueryChunkIndex((void*)addr_query);
        out_query_num = 0;

        // point back to base
        tmp_query_entry = addr_query->OutChunkQueryBase;
        RecipeEntry_t dec_query_entry;
        SyncRecipeEntry_t tmp_local_addr_entry;
        for (size_t i = 0; i < addr_query->queryNum; i++) {            
            // outside duplicate
            if (tmp_query_entry->dedupFlag == DUPLICATE) {
                _global_duplicate_num ++;

                //  load the containers in enclave
#if (INDEX_ENC == 1)                
                crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->value, 
                    sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
                    (uint8_t*)&dec_query_entry);
#endif

#if (INDEX_ENC == 0)
                memcpy((uint8_t*)&dec_query_entry, (uint8_t*)&tmp_query_entry->value, sizeof(RecipeEntry_t));
#endif                

#if (DEBUG_FLAG == 1)
                // debug
                // SyncEnclave::Logging("tail check addr", "offset=%d; len=%d; type=%d\n", dec_query_entry.offset, dec_query_entry.length, tmp_query_entry->dedupFlag);
#endif
                // prepare the addr in local_addr_list_ 
                tmp_container_id.assign((char*)dec_query_entry.containerName, CONTAINER_ID_LENGTH);
                tmp_local_addr_entry.offset = dec_query_entry.offset;
                tmp_local_addr_entry.length = dec_query_entry.length;

                auto find_cont = tmp_container_map.find(tmp_container_id);
                if (find_cont == tmp_container_map.end()) {
                    tmp_local_addr_entry.containerID = req_container->idNum;
                    tmp_container_map[tmp_container_id] = req_container->idNum;
                    memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
                        tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
                    req_container->idNum ++;
                }
                else {
                    tmp_local_addr_entry.containerID = find_cont->second;
                }
                local_addr_list.push_back(tmp_local_addr_entry);

                if (req_container->idNum == CONTAINER_CAPPING_VALUE) {
                    // fetch containers
                    Ocall_SyncGetReqContainerWithSize((void*)req_container);

                    // insert the containers into enclave cache
                    enclave_locality_cache_->BatchInsertCache(req_container);

                    // reset
                    req_container->idNum = 0;
                    tmp_container_map.clear();
                    local_addr_list.clear();
                }
            }
            else {
                // dec chunk hash from query entry
                uint8_t dec_chunk_hash[CHUNK_HASH_SIZE];
#if (INDEX_ENC == 1)
                crypto_util_->IndexAESCMCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->chunkHash[0],
                    CHUNK_HASH_SIZE, SyncEnclave::index_query_key_, dec_chunk_hash);
#endif

#if (INDEX_ENC == 0)
                memcpy(dec_chunk_hash, tmp_query_entry->chunkHash, CHUNK_HASH_SIZE);
#endif

                memcpy(plain_uni_fp_list_ + cur_reuse_offset, dec_chunk_hash, CHUNK_HASH_SIZE);
                cur_reuse_offset += CHUNK_HASH_SIZE;

                _global_unique_num ++;
            }

            // move on
            tmp_query_entry ++;
        }
    }

    if (req_container->idNum != 0) {
        // fetch containers
        Ocall_SyncGetReqContainerWithSize((void*)req_container);

        // insert the containers into enclave cache
        enclave_locality_cache_->BatchInsertCache(req_container);

        // reset
        req_container->idNum = 0;
        tmp_container_map.clear();
        local_addr_list.clear();
    }

    memcpy(uni_fp_list, plain_uni_fp_list_, cur_reuse_offset);
    (*uni_fp_num) = cur_reuse_offset / CHUNK_HASH_SIZE;

    return;
}

/**
 * @brief process a batch of fp list
 * 
 * @param fp_list 
 * @param fp_num 
 * @param out_chunk_query
 * @param req_container
 */
void EcallStreamChunkIndex::NaiveStreamCache(uint8_t* fp_list, size_t fp_num,
    uint8_t* uni_fp_list, size_t* uni_fp_num, Container_t* req_container) {
    // decrypt the fp list with session key
    crypto_util_->DecryptWithKey(cipher_ctx_, fp_list, fp_num * CHUNK_HASH_SIZE,
        session_key_, plain_uni_fp_list_);

    // record the current reuse pos in plain_uni_fp_list_
    uint32_t cur_reuse_offset = 0;

    // for outquery
    OutChunkQueryEntry_t out_query_entry;
    uint32_t global_uni_num = 0;
    uint32_t enclave_uni_num = 0;

    // SyncEnclave::Logging("before for", "\n");

    // check the fp index in enclave cache
    for (size_t i = 0; i < fp_num; i++) {
        string fp_str;
        fp_str.assign((char*)plain_uni_fp_list_ + i * CHUNK_HASH_SIZE, CHUNK_HASH_SIZE);
        string req_containerID;
        uint64_t req_features[SUPER_FEATURE_PER_CHUNK];

        // SyncEnclave::Logging("check fp: ", "%d\n", enclave_uni_num);
        // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);

        // SyncEnclave::Logging("before cache query", "\n");

        if (!enclave_locality_cache_->QueryFP(fp_str, req_containerID, req_features)) {
            // not exist in enclave cache (cache unique)

            // SyncEnclave::Logging("cache query uni", "\n");

            enclave_uni_num ++;
            _enclave_unique_num ++;
#if (DEBUG_FLAG == 1)
            // debug
            // // SyncEnclave::Logging("enclave unique: ", "%d\n", enclave_uni_num);
            // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
#endif

#if (INDEX_ENC == 1)
            // quary the out index immediately without batching
            crypto_util_->IndexAESCMCEnc(cipher_ctx_, (uint8_t*)&fp_str[0],
                CHUNK_HASH_SIZE, SyncEnclave::index_query_key_, out_query_entry.chunkHash);
#endif

#if (INDEX_ENC == 0)
            memcpy(out_query_entry.chunkHash, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
#endif            

            // SyncEnclave::Logging("before single query", "\n");

            Ocall_SingleQueryChunkIndex((void*)&out_query_entry);

            if (out_query_entry.dedupFlag == DUPLICATE) {
                
                // SyncEnclave::Logging("single query dup", "\n");

                // case-1: global duplicate: load the container in enclave
                // // SyncEnclave::Logging("global duplicate: ", "load container in cache.\n");
                _global_duplicate_num ++;

                // decrypt the value
                RecipeEntry_t dec_query_entry;
#if (INDEX_ENC == 1)                
                crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&out_query_entry.value, 
                    sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
                        (uint8_t*)&dec_query_entry);
#endif

#if (INDEX_ENC == 0)
                memcpy((uint8_t*)&dec_query_entry, (uint8_t*)&out_query_entry.value, sizeof(RecipeEntry_t));
#endif                

                // get the corresponding container id for this duplicate chunk
                memcpy(req_container->containerID, dec_query_entry.containerName, CONTAINER_ID_LENGTH);
#if (DEBUG_FLAG == 1)                
                // Ocall_PrintfBinary((uint8_t*)req_container->containerID, CONTAINER_ID_LENGTH);
                // Ocall_PrintfBinary(dec_query_entry.containerName, CONTAINER_ID_LENGTH);
#endif                

                Ocall_SingleGetReqContainer((void*)req_container);

                if (req_container->currentSize != 0) {
                    // insert the enclave cache
#if (DEBUG_FLAG == 1)                    
                    // // SyncEnclave::Logging("before insert reqcont ", "%d,%d",req_container->currentSize, req_container->currentMetaSize);
#endif
                    // SyncEnclave::Logging("cache insert", "\n");
                    enclave_locality_cache_->InsertCache(req_container);
                }
                else {
#if (DEBUG_FLAG == 1)        
                    // debug            
                    // SyncEnclave::Logging("reqContainer curSize = 0", "\n");
#endif                    
                }
            }
            else {
                // SyncEnclave::Logging("single query uni", "\n");

                // case-2: global unique: first send it in plain_uni_list, write the chunk in container later
                memcpy(plain_uni_fp_list_ + cur_reuse_offset, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
                cur_reuse_offset += CHUNK_HASH_SIZE;
#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("global unique: ", "%d\n", global_uni_num + 1);
                // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
#endif
                

                global_uni_num ++;
                _global_unique_num ++;                
            }
        }
        else {
            // a duplicate chunk to enclave cache
            // SyncEnclave::Logging("cache query dup", "\n");

            _enclave_duplicate_num ++;
        }
    }

    // encrypt the output uni fp list
    crypto_util_->EncryptWithKey(cipher_ctx_, plain_uni_fp_list_, cur_reuse_offset, 
        session_key_, uni_fp_list);
#if (DEBUG_FLAG == 1)         
    // memcpy(uni_fp_list, plain_uni_fp_list_, cur_reuse_offset);
#endif    
    (*uni_fp_num) = global_uni_num;

#if (DEBUG_FLAG == 1)
    // // debug: check the uni fp list
    // uint32_t checkoffset = 0;
    // // SyncEnclave::Logging("check uni fp list", "\n");
    // for (size_t i = 0; i < global_uni_num; i++) {
    //     uint8_t check_uni_fp[CHUNK_HASH_SIZE];
    //     memcpy(check_uni_fp, plain_uni_fp_list_ + checkoffset, CHUNK_HASH_SIZE);
    //     checkoffset += CHUNK_HASH_SIZE;
    //     Ocall_PrintfBinary(check_uni_fp, CHUNK_HASH_SIZE);
    // }
#endif

    return ;
}

// for debugging
void EcallStreamChunkIndex::NaiveStreamCacheDebug(uint8_t* fp_list, size_t fp_num,
    uint8_t* uni_fp_list, size_t* uni_fp_num, Container_t* req_container, 
    OutChunkQuery_t* debug_out_query, OutChunkQueryEntry_t* single_out_query) {

    // decrypt the fp list with session key
    crypto_util_->DecryptWithKey(cipher_ctx_, fp_list, fp_num * CHUNK_HASH_SIZE,
        session_key_, plain_uni_fp_list_);


    // SyncEnclave::Logging("in streamcache", "%d\n", fp_num);

    // record the current reuse pos in plain_uni_fp_list_
    uint32_t cur_reuse_offset = 0;

    // for outquery
    // OutChunkQueryEntry_t out_query_entry;
    uint32_t global_uni_num = 0;
    uint32_t enclave_uni_num = 0;

    OutChunkQueryEntry_t* tmp_debug_query_entry = debug_out_query->OutChunkQueryBase;

    // check the fp index in enclave cache
    for (size_t i = 0; i < fp_num; i++) {
        string fp_str;
        fp_str.assign((char*)plain_uni_fp_list_ + i * CHUNK_HASH_SIZE, CHUNK_HASH_SIZE);
        string req_containerID;
        uint64_t req_features[SUPER_FEATURE_PER_CHUNK];

        // SyncEnclave::Logging("check fp: ", "%d\n", enclave_uni_num);
        // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);

#if (DEBUG_FLAG == 1)
        // // debug
        // uint8_t check_fp[CHUNK_HASH_SIZE];
        // memcpy(check_fp, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
        // // SyncEnclave::Logging("check fp: ", "\n");
        // Ocall_PrintfBinary(check_fp, CHUNK_HASH_SIZE);
#endif

        // SyncEnclave::Logging("before queryfp", "\n");

        if (!enclave_locality_cache_->QueryFP(fp_str, req_containerID, req_features)) {
            // not exist in enclave cache (cache unique)
            enclave_uni_num ++;
            _enclave_unique_num ++;

#if (DEBUG_FLAG == 1)
            // debug
            // // SyncEnclave::Logging("enclave unique: ", "%d\n", enclave_uni_num);
            // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
#endif

// #if (INDEX_ENC == 1)
//             // quary the out index immediately without batching
//             crypto_util_->IndexAESCMCEnc(cipher_ctx_, (uint8_t*)&fp_str[0],
//                 CHUNK_HASH_SIZE, SyncEnclave::index_query_key_, out_query_entry.chunkHash);
// #endif

// #if (INDEX_ENC == 0)
//             memcpy(out_query_entry.chunkHash, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
// #endif            

#if (INDEX_ENC == 1)
            crypto_util_->IndexAESCMCEnc(cipher_ctx_, (uint8_t*)&fp_str[0],
                CHUNK_HASH_SIZE, SyncEnclave::index_query_key_, single_out_query->chunkHash);
#endif

#if (INDEX_ENC == 0)
            memcpy(single_out_query->chunkHash, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
#endif              

            // SyncEnclave::Logging("check before query", "\n");
            // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
            // Ocall_PrintfBinary(out_query_entry.chunkHash, CHUNK_HASH_SIZE);

            Ocall_SingleQueryChunkIndex((void*)single_out_query);

            // Ocall_PrintfBinary(out_query_entry.chunkHash, CHUNK_HASH_SIZE);

            // SyncEnclave::Logging("after single query", "\n");

            if (single_out_query->dedupFlag == DUPLICATE) {
                // case-1: global duplicate: load the container in enclave
                _global_duplicate_num ++;

                // decrypt the value
                RecipeEntry_t dec_query_entry;
#if (INDEX_ENC == 1)                
                crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&(single_out_query->value), 
                    sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
                        (uint8_t*)&dec_query_entry);
#endif

#if (INDEX_ENC == 0)
                memcpy((uint8_t*)&dec_query_entry, (uint8_t*)&(single_out_query->value), sizeof(RecipeEntry_t));
#endif                

                // get the corresponding container id for this duplicate chunk
                memcpy(req_container->containerID, dec_query_entry.containerName, CONTAINER_ID_LENGTH);
#if (DEBUG_FLAG == 1) 
                // Ocall_PrintfBinary((uint8_t*)req_container->containerID, CONTAINER_ID_LENGTH);
                // Ocall_PrintfBinary(dec_query_entry.containerName, CONTAINER_ID_LENGTH);
#endif                

                // SyncEnclave::Logging("global duplicate", "\n");

                Ocall_SingleGetReqContainer((void*)req_container);

                // SyncEnclave::Logging("global duplicate", "\n");

                if (req_container->currentSize != 0) {
                    // insert the enclave cache
#if (DEBUG_FLAG == 1)                     
                    // // SyncEnclave::Logging("before insert reqcont ", "%d,%d",req_container->currentSize, req_container->currentMetaSize);
#endif              
                    // SyncEnclave::Logging("insert cache", "\n");
                    enclave_locality_cache_->InsertCache(req_container);
                }
                else {
#if (DEBUG_FLAG == 1)                     
                    // debug
                    // SyncEnclave::Logging("reqContainer curSize = 0", "\n");
#endif                    
                }
            }
            else {
                // case-2: global unique: first send it in plain_uni_list, write the chunk in container later
                memcpy(plain_uni_fp_list_ + cur_reuse_offset, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
                cur_reuse_offset += CHUNK_HASH_SIZE;
#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("global unique: ", "%d\n", global_uni_num + 1);
                // Ocall_PrintfBinary((uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
#endif                

                global_uni_num ++;
                _global_unique_num ++;

                // prepare the unique chunk fp list here for debugging
                memcpy(tmp_debug_query_entry->chunkHash, (uint8_t*)&fp_str[0], CHUNK_HASH_SIZE);
                tmp_debug_query_entry ++;
            }
        }
        else {
            // a duplicate chunk to enclave cache
            _enclave_duplicate_num ++;
        }
    }

    // // debug check the uni fp list
    // for (size_t i = 0; i < global_uni_num; i++) {
    //     uint8_t check_uni_fp[CHUNK_HASH_SIZE];
    //     memcpy(check_uni_fp, plain_uni_fp_list_ + i * CHUNK_HASH_SIZE, CHUNK_HASH_SIZE);
    //     SyncEnclave::Logging("sent unifp", "\n");
    //     Ocall_PrintfBinary(check_uni_fp, CHUNK_HASH_SIZE);
    // }

    // encrypt the output uni fp list
    // crypto_util_->EncryptWithKey(cipher_ctx_, plain_uni_fp_list_, cur_reuse_offset, 
    //     session_key_, uni_fp_list);
    // SyncEnclave::Logging("cur_reuse_offset", "%d\n", cur_reuse_offset);
    memcpy(uni_fp_list, plain_uni_fp_list_, cur_reuse_offset);

#if (DEBUG_FLAG == 1)         
    // memcpy(uni_fp_list, plain_uni_fp_list_, cur_reuse_offset);
#endif    
    (*uni_fp_num) = global_uni_num;

    debug_out_query->queryNum = global_uni_num;

    Ocall_InsertDebugIndex((void*)debug_out_query);

#if (DEBUG_FLAG == 1)
    // // debug: check the uni fp list
    // uint32_t checkoffset = 0;
    // // SyncEnclave::Logging("check uni fp list", "\n");
    // for (size_t i = 0; i < global_uni_num; i++) {
    //     uint8_t check_uni_fp[CHUNK_HASH_SIZE];
    //     memcpy(check_uni_fp, plain_uni_fp_list_ + checkoffset, CHUNK_HASH_SIZE);
    //     checkoffset += CHUNK_HASH_SIZE;
    //     Ocall_PrintfBinary(check_uni_fp, CHUNK_HASH_SIZE);
    // }
#endif

    // SyncEnclave::Logging("chunkindex sent batch ", "%d\n", global_uni_num);

    return ;
}