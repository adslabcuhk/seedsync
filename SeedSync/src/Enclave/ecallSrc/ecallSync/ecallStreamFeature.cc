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

    plain_unifp_list_ = (uint8_t*)malloc(SyncEnclave::send_meta_batch_size_ * CHUNK_HASH_SIZE);

    memset(session_key_, 0, CHUNK_HASH_SIZE);

#if (DEBUG_FLAG == 1) 
    SyncEnclave::Logging(my_name_.c_str(), "init the EcallStreamFeature.\n");
#endif    
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

    // SyncEnclave::Logging("streamfeature recv batch ", "%d\n", unifp_num);

    // decrypt the batch with session key first
    // crypto_util_->DecryptWithKey(cipher_ctx_, unifp_list, unifp_num * CHUNK_HASH_SIZE,
    //     session_key_, plain_unifp_list_);
    memcpy(plain_unifp_list_, unifp_list, unifp_num * CHUNK_HASH_SIZE);

#if (DEBUG_FLAG == 1)
    // debug
    // memcpy(plain_unifp_list_, unifp_list, unifp_num * CHUNK_HASH_SIZE);
#endif    

    uint8_t* id_buf = req_container->idBuffer;
    uint8_t** container_array = req_container->containerArray;

    string tmp_container_id;
    unordered_map<string, uint32_t> tmp_container_map;
    tmp_container_map.reserve(CONTAINER_CAPPING_VALUE);

    uint32_t write_feature_offset = 0;

    // query the chunk index to get the chunk Addr
    OutChunkQueryEntry_t* tmp_query_entry = addr_query->OutChunkQueryBase;
    uint32_t query_num = 0;
    uint32_t fplist_offset = 0;

#if (DEBUG_FLAG == 1) 
    // SyncEnclave::Logging("unifp num ", "%d\n", unifp_num);
#endif

    for (size_t i = 0; i < unifp_num; i++) {
        // SyncEnclave::Logging("unifp list", "\n");
        // Ocall_PrintfBinary(plain_unifp_list_ + fplist_offset, CHUNK_HASH_SIZE);

#if (INDEX_ENC == 1)
        crypto_util_->IndexAESCMCEnc(cipher_ctx_, plain_unifp_list_ + fplist_offset,
            CHUNK_HASH_SIZE, SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
        memcpy(tmp_query_entry->chunkHash, plain_unifp_list_ + fplist_offset, CHUNK_HASH_SIZE);
#endif

        fplist_offset += CHUNK_HASH_SIZE;
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
#if (INDEX_ENC == 1)        
        crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->value, 
            sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
            (uint8_t*)&dec_query_entry);
#endif

#if (INDEX_ENC == 0)
        memcpy((uint8_t*)&dec_query_entry, (uint8_t*)&tmp_query_entry->value, sizeof(RecipeEntry_t));
#endif        

        // SyncEnclave::Logging("addr offset ", "%d\n", dec_query_entry.offset);
        // SyncEnclave::Logging("addr len ", "%d\n", dec_query_entry.length);
        // Ocall_PrintfBinary(dec_query_entry.containerName, CONTAINER_ID_LENGTH);

#if (DEBUG_FLAG == 1) 
        // SyncEnclave::Logging("addr offset ", "%d\n", dec_query_entry.offset);
        // SyncEnclave::Logging("addr len ", "%d\n", dec_query_entry.length);
        // Ocall_PrintfBinary(dec_query_entry.containerName, CONTAINER_ID_LENGTH);
#endif
        _total_unique_num ++;
        _total_unique_size += dec_query_entry.length;
        
        // get the chunk addr
        tmp_container_id.assign((char*)dec_query_entry.containerName, 
            CONTAINER_ID_LENGTH);
#if (DEBUG_FLAG == 1)             
        // SyncEnclave::Logging("Find container with name: %s", (char*)dec_query_entry.containerName);
#endif        
        // prepare the local addr entry
        tmp_local_addr_entry.offset = dec_query_entry.offset;
        tmp_local_addr_entry.length = dec_query_entry.length;
        
        auto find_cont = tmp_container_map.find(tmp_container_id);
        if (find_cont == tmp_container_map.end()) {
            // unique container for load buffer
            tmp_local_addr_entry.containerID = req_container->idNum;
            tmp_container_map[tmp_container_id] = req_container->idNum;
            memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
                tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
#if (DEBUG_FLAG == 1)
            // SyncEnclave::Logging("unique container","\n");
            // uint8_t check[CONTAINER_ID_LENGTH];
            // memcpy(check, id_buf + req_container->idNum * CONTAINER_ID_LENGTH, CONTAINER_ID_LENGTH);
            // Ocall_PrintfBinary(check, CONTAINER_ID_LENGTH);
#endif            
            req_container->idNum ++;
        }
        else {
            // exist in load buffer, get the local id
#if (DEBUG_FLAG == 1)            
            // SyncEnclave::Logging("exist container","\n");
#endif
            tmp_local_addr_entry.containerID = find_cont->second;
        }
        local_addr_list_.push_back(tmp_local_addr_entry);
#if (DEBUG_FLAG == 1) 
        // SyncEnclave::Logging("req container num ","%d\n", req_container->idNum);
        // SyncEnclave::Logging("local addr num ","%d\n", local_addr_list_.size());
#endif        
        if (req_container->idNum == CONTAINER_CAPPING_VALUE) {
            // fetch containers
            // SyncEnclave::Logging("before feature read cont", "%d\n", req_container->idNum);
            Ocall_SyncGetReqContainer((void*)req_container);
            // SyncEnclave::Logging("after feature read cont", "%d\n", req_container->idNum);
            // read chunk from the encrypted container buffer
            for (size_t k = 0; k < local_addr_list_.size(); k++) {
                // get chunk data one by one                
                // get the features from metadata session
                uint32_t local_id = local_addr_list_[k].containerID;
                string req_fp;
                req_fp.assign((char*)plain_unifp_list_ + fplist_offset, CHUNK_HASH_SIZE);
                fplist_offset += CHUNK_HASH_SIZE;
                // find the features based on fp
                uint32_t meta_offset = 0;
                uint32_t meta_num = 0;
                memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
                meta_num = meta_offset / (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                uint8_t* meta_session = container_array[local_id] + sizeof(uint32_t);

                // point to the first fp
                uint32_t read_meta_offset = SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
                uint8_t check_hash_req[CHUNK_HASH_SIZE];
                uint8_t check_hash_read[CHUNK_HASH_SIZE];
                for (size_t j = 0; j < meta_num; j++) {
                    // get the chunk fp in meta session one by one
                    string read_fp;
                    read_fp.assign((char*)meta_session + read_meta_offset, CHUNK_HASH_SIZE);

                    if (read_fp == req_fp) {
                        // get the features
                        size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
#if (DEBUG_FLAG == 1)
                        // // debug
                        // memcpy((char*)check_features, meta_session +
                        //     read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                        // memcpy(check_hash_read, read_fp.c_str(), CHUNK_HASH_SIZE);
                        // memcpy(check_hash_req, req_fp.c_str(), CHUNK_HASH_SIZE);

                        // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
                        //     // SyncEnclave::Logging("check feature ", "%d\t", check_features[j]);
                        // }
                        // Ocall_PrintfBinary(check_hash_req, CHUNK_HASH_SIZE);
                        // Ocall_PrintfBinary(check_hash_read, CHUNK_HASH_SIZE);
#endif
                        // reuse input buffer
                        memcpy((char*)unifp_list + write_feature_offset, 
                            meta_session + read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                        write_feature_offset += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                        memcpy((char*)unifp_list + write_feature_offset, &read_fp[0], CHUNK_HASH_SIZE);
                        write_feature_offset += CHUNK_HASH_SIZE;
                        break;
                    }

                    // point to the next fp
                    read_meta_offset += (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
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
        // SyncEnclave::Logging("tail before feature read cont", "%d\n", req_container->idNum);
        Ocall_SyncGetReqContainer((void*)req_container);
        // SyncEnclave::Logging("tail after feature read cont", "%d\n", req_container->idNum);
        // read chunk from the encrypted container buffer
        for (size_t k = 0; k < local_addr_list_.size(); k++) {
            // get chunk data one by one
            // get the features from metadata session
            uint32_t local_id = local_addr_list_[k].containerID;
            string req_fp;
            req_fp.assign((char*)plain_unifp_list_ + fplist_offset, CHUNK_HASH_SIZE);
            fplist_offset += CHUNK_HASH_SIZE;
            // find the features based on fp
            uint32_t meta_offset = 0;
            uint32_t meta_num = 0;
            memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
            meta_num = meta_offset / (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
#if (DEBUG_FLAG == 1)
            // SyncEnclave::Logging("metaoffset ","%d\n", meta_offset);
            // SyncEnclave::Logging("metanum ","%d\n", meta_num);
            // SyncEnclave::Logging("entrysize ","%d\n", (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
#endif
            uint8_t* meta_session = container_array[local_id] + sizeof(uint32_t);

            // point to the first fp
            uint32_t read_meta_offset = SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
            // point to the first feature
#if (DEBUG_FLAG == 1)
            // uint32_t read_feature_offset = 0;
            // uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
            // uint8_t check_hash_req[CHUNK_HASH_SIZE];
            // uint8_t check_hash_read[CHUNK_HASH_SIZE];
#endif            
            for (size_t j = 0; j < meta_num; j++) {
                // get the chunk fp in meta session one by one
                string read_fp;
                read_fp.assign((char*)meta_session + read_meta_offset, CHUNK_HASH_SIZE);
                // SyncEnclave::Logging("read fp offset ","%d\n", read_meta_offset);

                if (read_fp == req_fp) {
                    // get the features
                    size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                    // SyncEnclave::Logging("read feature offset ","%d\n", read_feature_offset);
#if (DEBUG_FLAG == 1)
                    // // debug
                    // memcpy((char*)check_features, meta_session +
                    //     read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                    // memcpy(check_hash_read, read_fp.c_str(), CHUNK_HASH_SIZE);
                    // memcpy(check_hash_req, req_fp.c_str(), CHUNK_HASH_SIZE);

                    // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
                    //     // SyncEnclave::Logging("check feature ", "%d\t", check_features[j]);
                    // }
                    // Ocall_PrintfBinary(check_hash_req, CHUNK_HASH_SIZE);
                    // Ocall_PrintfBinary(check_hash_read, CHUNK_HASH_SIZE);
#endif
                    // reuse input buffer
                    memcpy((char*)unifp_list + write_feature_offset, 
                        meta_session + read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                    write_feature_offset += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                    memcpy((char*)unifp_list + write_feature_offset, &read_fp[0], CHUNK_HASH_SIZE);
                    write_feature_offset += CHUNK_HASH_SIZE;

                    break;
                }

                // point to the next fp
                read_meta_offset += (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
            }
        }

        // reset
        req_container->idNum = 0;
        tmp_container_map.clear();
        local_addr_list_.clear(); 
    }

    // deal with diff batch size between metadata and data
    uint64_t meta_batch_num = SyncEnclave::send_meta_batch_size_;
    uint64_t data_batch_num = SyncEnclave::send_chunk_batch_size_;
    if (meta_batch_num > data_batch_num) {
        uint64_t data_batch_size = data_batch_num * (SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + CHUNK_HASH_SIZE);
        uint8_t tmp_buf[data_batch_size];
        size_t tmp_offset = 0;
        size_t remaining_size = write_feature_offset;
        size_t enc_size = 0;

        while (remaining_size != 0) {
            // SyncEnclave::Logging("remaining size ", "%d\n", remaining_size); 
            // SyncEnclave::Logging("enc size = ", "%d; tmp_offset = %d\n", enc_size, tmp_offset); 
            
            if (remaining_size < data_batch_size) {
                enc_size = remaining_size;
                remaining_size = 0; 
            }
            else {
                enc_size = data_batch_size;
                remaining_size -= data_batch_size; 
            }

            memcpy(tmp_buf, unifp_list + tmp_offset, enc_size);
            // encrypt tmp buf and write to feature list
            crypto_util_->EncryptWithKey(cipher_ctx_, tmp_buf, enc_size, 
                session_key_, feature_list + tmp_offset);

            tmp_offset += data_batch_size;  
        }
    }
    else if (meta_batch_num == data_batch_num) {
        // encrypt the feature list with session key
        crypto_util_->EncryptWithKey(cipher_ctx_, unifp_list, write_feature_offset, 
            session_key_, feature_list);
    }
    else {
        SyncEnclave::Logging("error ", "meta batch size is smaller than data batch size.\n");
    }

#if (DEBUG_FLAG == 1)          
    // debug
    // memcpy(feature_list, unifp_list, write_feature_offset);

    // feature_num = write_feature_offset / (SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + CHUNK_HASH_SIZE);
    // // SyncEnclave::Logging("feature num in header ", "%d, %d, %d\n",write_feature_offset, feature_num, write_feature_offset / (SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + CHUNK_HASH_SIZE));
#endif

    // SyncEnclave::Logging("streamfeature sent batch ", "%d\n", write_feature_offset / (SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + CHUNK_HASH_SIZE));

    return ;
}