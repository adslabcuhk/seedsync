/**
 * @file ecallStreamWriter.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-06-15
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/ecallStreamWriter.h"


/**
 * @brief Construct a new Ecall Stream Writer object
 * 
 */
EcallStreamWriter::EcallStreamWriter() {
    crypto_util_ = new EcallCrypto(CIPHER_TYPE, HASH_TYPE);
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    md_ctx_ = EVP_MD_CTX_new();

    memset(session_key_, 0, CHUNK_HASH_SIZE);

    // for feature gen
    finesse_util_ = new EcallFinesseUtil(SUPER_FEATURE_PER_CHUNK,    
        FEATURE_PER_CHUNK, FEATURE_PER_SUPER_FEATURE);
    rabin_util_ = new RabinFPUtil(SLIDING_WIN_SIZE);
    rabin_util_->NewCtx(rabin_ctx_);

    plain_in_buf_ = (uint8_t*)malloc(SyncEnclave::send_chunk_batch_size_ * (MAX_CHUNK_SIZE + sizeof(uint32_t) + sizeof(uint8_t) + CHUNK_HASH_SIZE));

    batch_fp_index_.reserve(SyncEnclave::send_chunk_batch_size_);

#if (DEBUG_FLAG == 1)    
    SyncEnclave::Logging(my_name_.c_str(), "init the StreamWriter.\n");
#endif    

}

/**
 * @brief Destroy the Ecall Stream Writer object
 * 
 */
EcallStreamWriter::~EcallStreamWriter() {
    delete(crypto_util_);
    EVP_CIPHER_CTX_free(cipher_ctx_);
    EVP_MD_CTX_free(md_ctx_);

    rabin_util_->FreeCtx(rabin_ctx_);
    delete rabin_util_;
    delete finesse_util_;

    free(plain_in_buf_);
}

/**
 * @brief process a batch of recv data
 * 
 * @param recv_buf 
 * @param recv_size 
 * @param req_container 
 * @param base_addr_query 
 */
void EcallStreamWriter::ProcessBatch(uint8_t* recv_buf, uint32_t recv_size, 
    ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
    Container_t* container_buf, OutChunkQuery_t* update_index) {

    _total_recv_size += recv_size;

    // dec the recv batch
    crypto_util_->DecryptWithKey(cipher_ctx_, recv_buf, recv_size, session_key_, 
        plain_in_buf_);

    uint32_t process_size = 0;

    uint8_t* id_buf = req_container->idBuffer;
    uint8_t** container_array = req_container->containerArray;

    string tmp_container_id;
    unordered_map<string, uint32_t> tmp_container_map;
    tmp_container_map.reserve(CONTAINER_CAPPING_VALUE);

    // prepare the addr query for base chunk
    OutChunkQueryEntry_t* tmp_query_entry = base_addr_query->OutChunkQueryBase;
    uint32_t query_num = 0;

    // the chunk addr (to be updated in outside indexes)
    OutChunkQueryEntry_t* tmp_update_entry = update_index->OutChunkQueryBase;
    update_index->queryNum = 0;
    uint32_t update_num = 0;

    // prepare the batch fp index
    batch_fp_index_.clear();
    BatchAddrValue_t tmp_batch_addr_value;

    // process recv batch
    while(process_size != recv_size) {
#if (DEBUG_FLAG == 1)        
        // // SyncEnclave::Logging("process size = ; recv size = ", "%d, %d\n", process_size, recv_size);
#endif
        // recv chunk format [chunkSize || chunkType || (baseHash) || chunkData]
        uint32_t cur_size = *(uint32_t*)(plain_in_buf_ + process_size);
        process_size += sizeof(uint32_t);
        uint8_t cur_type = *(uint8_t*)(plain_in_buf_ + process_size);
        process_size += sizeof(uint8_t);

#if (DEBUG_FLAG == 1)  
        // // SyncEnclave::Logging("recv chunksize ", "%d\n", cur_size);
        // // SyncEnclave::Logging("recv chunkType ", "%d\n", cur_type);
#endif

        tmp_batch_addr_value.type = cur_type;
        tmp_batch_addr_value.size = cur_size;
        tmp_batch_addr_value.offset = process_size;

        uint8_t* cur_iv = this->PickNewIV();

        uint8_t tmp_enc_chunk[MAX_CHUNK_SIZE];
        uint8_t cur_hash[CHUNK_HASH_SIZE];
        uint64_t cur_feature[SUPER_FEATURE_PER_CHUNK];
        uint8_t tmp_original_chunk[MAX_CHUNK_SIZE];
        int tmp_original_size = 0;
        uint8_t tmp_compress_chunk[MAX_CHUNK_SIZE];
        int tmp_compress_size = 0;

        if (cur_type == COMP_ONLY_CHUNK) {
            uint8_t* cur_data = plain_in_buf_ + process_size;
            // do decompression before generate chunkhash !!!!!
            tmp_original_size = LZ4_decompress_safe((char*)cur_data, 
                (char*)tmp_original_chunk, cur_size, MAX_CHUNK_SIZE);
            if (tmp_original_size > 0) {
#if (DEBUG_FLAG == 1)                  
                // // SyncEnclave::Logging("can decomp ","decompsize = %d\n", tmp_original_size);
#endif                
            }
            else {
#if (DEBUG_FLAG == 1)                  
                // // SyncEnclave::Logging("cannot decomp ","compsize = %d, decompsize = %d\n", cur_size, tmp_original_size);
#endif
                memcpy(tmp_original_chunk, cur_data, cur_size);
                tmp_original_size = cur_size;

#if (DEBUG_FLAG == 1)  
                // SyncEnclave::Logging("cannot decomp ","decompsize = %d\n", tmp_original_size);
                // debug
                uint8_t check_hash[CHUNK_HASH_SIZE];
                crypto_util_->GenerateHMAC(tmp_original_chunk, tmp_original_size,
                    check_hash);
                Ocall_PrintfBinary(check_hash, CHUNK_HASH_SIZE);
                crypto_util_->GenerateHMAC(cur_data, cur_size,
                    check_hash);
                Ocall_PrintfBinary(check_hash, CHUNK_HASH_SIZE);
#endif                
            }

            // generate chunkhash
            crypto_util_->GenerateHMAC(tmp_original_chunk, tmp_original_size, cur_hash);
#if EXTRA_HASH_FUNCTION == 1
            // use the extra hash function
            uint8_t tmp_hash_val[CHUNK_HASH_SIZE];
            crypto_util_->GenerateHash(tmp_original_chunk, tmp_original_size, tmp_hash_val);
#endif
            // insert the chunk to batch index
            string tmp_key;
            tmp_key.assign((char*)cur_hash, CHUNK_HASH_SIZE);
            batch_fp_index_.insert(make_pair(tmp_key, tmp_batch_addr_value));
#if (DEBUG_FLAG == 1)
            // // debug: check batch fp insert
            // // SyncEnclave::Logging("batch fp index insert", "\n");
            // Ocall_PrintfBinary(cur_hash, CHUNK_HASH_SIZE);
            // string check_query;
            // check_query.assign((char*)cur_hash, CHUNK_HASH_SIZE);
            // auto check_find = batch_fp_index_.find(check_query);
            // if (check_find != batch_fp_index_.end()) {
            //     // SyncEnclave::Logging("insert found", "\n");
            // }

            // // SyncEnclave::Logging("before features ","decompsize = %d\n", tmp_original_size);
#endif
            // extract features
            this->ExtractFeature(rabin_ctx_, tmp_original_chunk, tmp_original_size, cur_feature);

            // save chunk to container
#if (INDEX_ENC == 1)
            crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
            memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif            

            // enc with data key, write to container
            crypto_util_->EncryptWithKeyIV(cipher_ctx_, cur_data, cur_size,
                SyncEnclave::enclave_key_, tmp_enc_chunk, cur_iv);

            this->SaveChunk(container_buf, tmp_enc_chunk, cur_size, cur_iv, cur_hash, 
                cur_feature, &tmp_update_entry->value);

            // move on
            process_size += cur_size;
            tmp_update_entry ++;
            update_num ++;

            _only_comp_size += cur_size;
        }
        else if (cur_type == DELTA_ONLY_CHUNK) {

            // record the cur delta chunk
            uint32_t record_offset = process_size;
            uint32_t record_size = cur_size;

            // read base chunk for decoding
            uint8_t* base_hash = plain_in_buf_ + process_size;
            process_size += CHUNK_HASH_SIZE;
            uint8_t* delta_data = plain_in_buf_ + process_size;
            process_size += cur_size;

            _comp_delta_size += cur_size;

#if (DEBUG_FLAG == 1)
            // // debug
            // uint8_t check_basehash[CHUNK_HASH_SIZE];
            // memcpy(check_basehash, base_hash, CHUNK_HASH_SIZE);
#endif

            string query_local;
            query_local.assign((char*)base_hash, CHUNK_HASH_SIZE);

            auto local_find = batch_fp_index_.find(query_local);
            if (local_find != batch_fp_index_.end()) {

#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("writer check base: local delta only", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif

                // get the base chunk in current batch
                if (local_find->second.type != COMP_ONLY_CHUNK) {
#if (DEBUG_FLAG == 1)                      
                    SyncEnclave::Logging("error ", "multi-level delta\n");
#endif                    
                    continue;
                }
                
                // delta decode
                tmp_original_size = DeltaDecode(plain_in_buf_ + local_find->second.offset,
                    local_find->second.size, delta_data, cur_size, tmp_original_chunk);
                
#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("batch local decode size 7: ", "original = %d; decode = %d\n", cur_size,tmp_original_size);
#endif
                // generate chunkhash
                crypto_util_->GenerateHMAC(tmp_original_chunk, tmp_original_size, cur_hash);
#if EXTRA_HASH_FUNCTION == 1
                // use the extra hash function
                uint8_t tmp_hash_val[CHUNK_HASH_SIZE];
                crypto_util_->GenerateHash(tmp_original_chunk, tmp_original_size, tmp_hash_val);
#endif
                // extract features
                this->ExtractFeature(rabin_ctx_, tmp_original_chunk, tmp_original_size, cur_feature);
                
                // local compress
                tmp_compress_size = LZ4_compress_fast((char*)tmp_original_chunk, 
                    (char*)tmp_compress_chunk, tmp_original_size, tmp_original_size,
                    3);
                if (tmp_compress_size > 0) {
                    // it can be compressed
                    // do encryption
                    crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_compress_chunk, 
                        tmp_compress_size, SyncEnclave::enclave_key_, tmp_enc_chunk, 
                        cur_iv);

#if (INDEX_ENC == 1)
                    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                        SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                    memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif                    

                    SaveChunk(container_buf, tmp_enc_chunk, tmp_compress_size, cur_iv, cur_hash, 
                        cur_feature, &tmp_update_entry->value);
                    
                    _comp_similar_size += tmp_compress_size;
                }
                else {
                    // it cannot be compressed
                    // do encryption
                    crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_original_chunk, 
                        tmp_original_size, SyncEnclave::enclave_key_, tmp_enc_chunk, 
                        cur_iv);

#if (INDEX_ENC == 1)
                    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                        SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                    memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif                    

                    SaveChunk(container_buf, tmp_enc_chunk, tmp_original_size, cur_iv, cur_hash, 
                        cur_feature, &tmp_update_entry->value);

                    _comp_similar_size += tmp_original_size;
                }

                // move on
                tmp_update_entry ++;
                update_num ++;
            }
            else {
#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("writer check base: delta only", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif
                // get the base chunk in outside container
#if (INDEX_ENC == 1)
                crypto_util_->IndexAESCMCEnc(cipher_ctx_, base_hash, CHUNK_HASH_SIZE,
                    SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                memcpy(tmp_query_entry->chunkHash, base_hash, CHUNK_HASH_SIZE);
#endif                

                query_num ++;
                tmp_query_entry ++;

                // record the offset and size of the delta chunk in plain_buf
                DeltaBatchAddrValue_t tmp_record_addr;
                tmp_record_addr.offset = record_offset; // [baseHash || delta chunk]
                tmp_record_addr.size = record_size;
                tmp_record_addr.type = DELTA_ONLY_CHUNK;
                pending_delta_list_.push(tmp_record_addr);
            }

        }
        else if (cur_type == DELTA_COMP_CHUNK) {

            // record the cur delta chunk
            uint32_t record_offset = process_size;
            uint32_t record_size = cur_size;

            uint8_t* base_hash = plain_in_buf_ + process_size;
            process_size += CHUNK_HASH_SIZE;
            uint8_t* comp_delta_data = plain_in_buf_ + process_size;
            process_size += cur_size;

            _comp_delta_size += cur_size;

#if (DEBUG_FLAG == 1)
            // // debug: check the basehash
            // uint8_t check_basehash[CHUNK_HASH_SIZE];
            // memcpy(check_basehash, base_hash, CHUNK_HASH_SIZE);
            // // SyncEnclave::Logging("writer check base hash1", "\n");
            // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif

            // decompress the delta first
            uint8_t tmp_decomp_chunk[MAX_CHUNK_SIZE];
            int tmp_decomp_size = LZ4_decompress_safe((char*)comp_delta_data, 
                (char*)tmp_decomp_chunk, cur_size, MAX_CHUNK_SIZE);
            
            if (tmp_decomp_size > 0) {

            }
            else {
                memcpy(tmp_decomp_chunk, comp_delta_data, cur_size);
                tmp_decomp_size = cur_size;
            }

            string query_local;
            query_local.assign((char*)base_hash, CHUNK_HASH_SIZE);

            auto local_find = batch_fp_index_.find(query_local);
            if (local_find != batch_fp_index_.end()) {
                // get the base chunk in current batch
                if (local_find->second.type != COMP_ONLY_CHUNK) {
#if (DEBUG_FLAG == 1)                      
                    SyncEnclave::Logging("error ", "multi-level delta\n");
#endif                    
                    continue;
                }
#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("writer check base: local delta comp", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif
                // delta decode
                tmp_original_size = DeltaDecode(plain_in_buf_ + local_find->second.offset, 
                    local_find->second.size, tmp_decomp_chunk, tmp_decomp_size, tmp_original_chunk);
#if (DEBUG_FLAG == 1)  
                // // SyncEnclave::Logging("batch local decode size 6: ", "original = %d; decode = %d\n", tmp_decomp_size,tmp_original_size);
#endif
                // generate chunkhash
                crypto_util_->GenerateHMAC(tmp_original_chunk, tmp_original_size, cur_hash);
#if EXTRA_HASH_FUNCTION == 1
                // use the extra hash function
                uint8_t tmp_hash_val[CHUNK_HASH_SIZE];
                crypto_util_->GenerateHash(tmp_original_chunk, tmp_original_size, tmp_hash_val);
#endif
                // extract features
                this->ExtractFeature(rabin_ctx_, tmp_original_chunk, tmp_original_size, cur_feature);

                // local compress
                tmp_compress_size = LZ4_compress_fast((char*)tmp_original_chunk, 
                    (char*)tmp_compress_chunk, tmp_original_size, tmp_original_size,
                    3);
                if (tmp_compress_size > 0) {
                    // it can be compressed
                    // do encryption
                    crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_compress_chunk, 
                        tmp_compress_size, SyncEnclave::enclave_key_, tmp_enc_chunk, 
                        cur_iv);

#if (INDEX_ENC == 1)
                    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                        SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                    memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif                    

                    SaveChunk(container_buf, tmp_enc_chunk, tmp_compress_size, cur_iv, cur_hash, 
                        cur_feature, &tmp_update_entry->value);
                    
                    _comp_similar_size += tmp_compress_size;
                }
                else {
                    // it cannot be compressed
                    // do encryption
                    crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_original_chunk, 
                        tmp_original_size, SyncEnclave::enclave_key_, tmp_enc_chunk, 
                        cur_iv);

#if (INDEX_ENC == 1)
                    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                        SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                    memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif                    

                    SaveChunk(container_buf, tmp_enc_chunk, tmp_original_size, cur_iv, cur_hash, 
                        cur_feature, &tmp_update_entry->value);
                    
                    _comp_similar_size += tmp_original_size;
                }

                // move on
                tmp_update_entry ++;
                update_num ++;
            }
            else {
                // get the base chunk in outside container
#if (DEBUG_FLAG == 1)
                // // debug: check the basehash
                // uint8_t check_basehash[CHUNK_HASH_SIZE];
                // memcpy(check_basehash, base_hash, CHUNK_HASH_SIZE);
                // // SyncEnclave::Logging("writer check base hash2", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);

                // // SyncEnclave::Logging("writer check base: delta comp", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif

#if (INDEX_ENC == 1)
                crypto_util_->IndexAESCMCEnc(cipher_ctx_, base_hash, CHUNK_HASH_SIZE,
                    SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                memcpy(tmp_query_entry->chunkHash, base_hash, CHUNK_HASH_SIZE);
#endif                

                query_num ++;
                tmp_query_entry ++;

                // record the offset and size of the delta chunk in plain_buf
                DeltaBatchAddrValue_t tmp_record_addr;
                tmp_record_addr.offset = record_offset; // [baseHash || delta chunk]
                tmp_record_addr.size = record_size;
                tmp_record_addr.type = DELTA_COMP_CHUNK;
                pending_delta_list_.push(tmp_record_addr);
            }
        }
    }

    // query to get the base chunk addr
    base_addr_query->queryNum = query_num;
    Ocall_QueryChunkAddr(base_addr_query);

    // point back to base
    tmp_query_entry = base_addr_query->OutChunkQueryBase;
    RecipeEntry_t dec_query_entry;
    EnclaveRecipeEntry_t tmp_base_entry;
    for (size_t i = 0; i < base_addr_query->queryNum; i++) {
#if (INDEX_ENC == 1)        
        crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->value,
        sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, (uint8_t*)&dec_query_entry);
#endif

#if (INDEX_ENC == 0)
        memcpy((uint8_t*)&dec_query_entry, (uint8_t*)&tmp_query_entry->value, sizeof(RecipeEntry_t));
#endif        

#if (DEBUG_FLAG == 1)  
        // // SyncEnclave::Logging("base addr offset ", "%d\n", dec_query_entry.offset);
        // // SyncEnclave::Logging("base addr len ", "%d\n", dec_query_entry.length);
        // Ocall_PrintfBinary((uint8_t*)dec_query_entry.containerName, CONTAINER_ID_LENGTH);
#endif        
        // get the chunk addr
        tmp_container_id.assign((char*)dec_query_entry.containerName, 
            CONTAINER_ID_LENGTH);
        
        tmp_base_entry.offset = dec_query_entry.offset;
        tmp_base_entry.length = dec_query_entry.length;

        auto tmp_find = tmp_container_map.find(tmp_container_id);
        if (tmp_find == tmp_container_map.end()) {
            // unique container
            tmp_base_entry.containerID = req_container->idNum;
            tmp_container_map[tmp_container_id] = req_container->idNum;
            memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
                tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
            req_container->idNum ++;
        }
        else {
            tmp_base_entry.containerID = tmp_find->second;
        }
        base_addr_list_.push_back(tmp_base_entry);

        if (req_container->idNum == CONTAINER_CAPPING_VALUE) {
            // fetch containers
            Ocall_SyncGetReqContainerWithSize((void*)req_container);

            for (size_t k = 0; k < base_addr_list_.size(); k++) {
                // get the delta chunk for decoding
                DeltaBatchAddrValue_t tmp_delta_addr = pending_delta_list_.front();
                pending_delta_list_.pop();

                uint8_t* pending_delta_chunk = plain_in_buf_ + tmp_delta_addr.offset
                    + CHUNK_HASH_SIZE;
#if (DEBUG_FLAG == 1)
                // // debug: calculate the sim chunk hash
                // uint8_t check_sim_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( pending_delta_chunk, tmp_delta_addr.size,
                //     check_sim_hash);
                // // SyncEnclave::Logging("check pair", "\n");
                // uint8_t check_base_hash[CHUNK_HASH_SIZE];
                // memcpy(check_base_hash, plain_in_buf_ + tmp_delta_addr.offset, CHUNK_HASH_SIZE);
                // Ocall_PrintfBinary(check_base_hash, CHUNK_HASH_SIZE);

                // Ocall_PrintfBinary(check_sim_hash, CHUNK_HASH_SIZE);
#endif
                // get the base chunk
                uint32_t base_id = base_addr_list_[k].containerID;
                uint32_t base_offset = base_addr_list_[k].offset;
                uint32_t base_size = base_addr_list_[k].length;
                uint32_t meta_offset = 0;
                uint8_t* base_chunk_data = nullptr;

                if (req_container->sizeArray[base_id] == 0) {
                    // the container is currently in-memory
                    memcpy((char*)&meta_offset, container_buf, sizeof(uint32_t));

                    base_chunk_data = container_buf->body + base_offset;
                }
                else {
                    memcpy((char*)&meta_offset, container_array[base_id], sizeof(uint32_t));
#if (DEBUG_FLAG == 1)                      
                    // // SyncEnclave::Logging("check read base ","metaoffset=%d, offset=%d, len=%d\n", meta_offset, base_offset, base_size);
#endif
                    base_chunk_data = container_array[base_id] + meta_offset 
                        + base_offset + sizeof(uint32_t);
                }
                
                // decrypt the base chunk
                uint8_t plain_base[MAX_CHUNK_SIZE];
                uint8_t* base_iv = base_chunk_data + base_size;
                crypto_util_->DecryptionWithKeyIV(cipher_ctx_, base_chunk_data, base_size,
                    SyncEnclave::enclave_key_, plain_base, base_iv);
#if (DEBUG_FLAG == 1)
                // // debug: check the plain base data
                // uint8_t check_plain_base_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( plain_base, base_size, check_plain_base_hash);
                // // SyncEnclave::Logging("check plain base hash", "\n");
                // Ocall_PrintfBinary(check_plain_base_hash, CHUNK_HASH_SIZE);
#endif
                // delta decode & write
                uint8_t original_chunk[MAX_CHUNK_SIZE];
                uint32_t original_size = 0;

                if (tmp_delta_addr.type == DELTA_COMP_CHUNK) {
                    // decompress the delta first
                    uint8_t tmp_decomp_chunk[MAX_CHUNK_SIZE];
                    int tmp_decomp_size = LZ4_decompress_safe((char*)pending_delta_chunk, 
                        (char*)tmp_decomp_chunk, tmp_delta_addr.size, MAX_CHUNK_SIZE);
                    
                    if (tmp_decomp_size <= 0) {
#if (DEBUG_FLAG == 1)                          
                        SyncEnclave::Logging("error ", "decompress error\n");
#endif                    
                    }

#if (DEBUG_FLAG == 1)
                    // // debug: check the decompress data
                    // uint8_t check_decomp_hash[CHUNK_HASH_SIZE];
                    // crypto_util_->GenerateHMAC( tmp_decomp_chunk, tmp_decomp_size, check_decomp_hash);
                    // // SyncEnclave::Logging("check decomp hash", "\n");
                    // Ocall_PrintfBinary(check_decomp_hash, CHUNK_HASH_SIZE);
#endif

                    original_size = this->DeltaDecode(plain_base, base_size, 
                        tmp_decomp_chunk, tmp_decomp_size, original_chunk);
                }
                else if (tmp_delta_addr.type == DELTA_ONLY_CHUNK) {
#if (DEBUG_FLAG == 1)
                    // // debug: check the decompress data
                    // uint8_t check_decomp_hash[CHUNK_HASH_SIZE];
                    // crypto_util_->GenerateHMAC( pending_delta_chunk, tmp_delta_addr.size, check_decomp_hash);
                    // // SyncEnclave::Logging("check pending hash", "\n");
                    // Ocall_PrintfBinary(check_decomp_hash, CHUNK_HASH_SIZE);
#endif
                    original_size = this->DeltaDecode(plain_base, base_size, 
                        pending_delta_chunk, tmp_delta_addr.size, original_chunk);
                }
#if (DEBUG_FLAG == 1)  
                // // SyncEnclave::Logging("DELTA_ONLY_CHUNK decode size ", "original = %d; decode = %d; type = %d\n", tmp_delta_addr.size, original_size, tmp_debug_entry->dedupFlag);

                // debug
                // // SyncEnclave::Logging("tail decode size ", "%d\n", original_size);
                // uint8_t check_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( original_chunk, original_size,
                //     check_hash);
                // Ocall_PrintfBinary(check_hash, CHUNK_HASH_SIZE);
#endif
                this->ProcessOneChunk(container_buf, original_chunk, original_size, 
                    tmp_update_entry);

                // move on
                tmp_update_entry ++;
                update_num ++;
            }

            // reset
            req_container->idNum = 0;
            tmp_container_map.clear();
            base_addr_list_.clear();
        }

        tmp_query_entry ++;
    }

    // deal with tail base
    if (req_container->idNum != 0) {
#if (DEBUG_FLAG == 1)  
        // // debug
        // uint8_t check_cur_container_id[CONTAINER_ID_LENGTH];
        // memcpy(check_cur_container_id, container_buf->containerID, CONTAINER_ID_LENGTH);
        // // SyncEnclave::Logging("check cur in mem container id", "\n");
        // Ocall_PrintfBinary(check_cur_container_id, CONTAINER_ID_LENGTH);
#endif
        // fetch containers
        Ocall_SyncGetReqContainerWithSize((void*)req_container);

        for (size_t k = 0; k < base_addr_list_.size(); k++) {
            // get the delta chunk for decoding
            DeltaBatchAddrValue_t tmp_delta_addr = pending_delta_list_.front();
            pending_delta_list_.pop();

            uint8_t* pending_delta_chunk = plain_in_buf_ + tmp_delta_addr.offset
                + CHUNK_HASH_SIZE;
#if (DEBUG_FLAG == 1)
            // // debug: calculate the sim chunk hash
            // uint8_t check_sim_hash[CHUNK_HASH_SIZE];
            // crypto_util_->GenerateHMAC( pending_delta_chunk, tmp_delta_addr.size,
            //     check_sim_hash);
            // // SyncEnclave::Logging("check pair", "\n");
            // uint8_t check_base_hash[CHUNK_HASH_SIZE];
            // memcpy(check_base_hash, plain_in_buf_ + tmp_delta_addr.offset, CHUNK_HASH_SIZE);
            // Ocall_PrintfBinary(check_base_hash, CHUNK_HASH_SIZE);

            // Ocall_PrintfBinary(check_sim_hash, CHUNK_HASH_SIZE);
#endif
            // get the base chunk
            uint32_t base_id = base_addr_list_[k].containerID;
            uint32_t base_offset = base_addr_list_[k].offset;
            uint32_t base_size = base_addr_list_[k].length;
            uint32_t meta_offset = 0;
            uint8_t* base_chunk_data = nullptr;

            if (req_container->sizeArray[base_id] == 0) {
                // the container is currently in-memory
                memcpy((char*)&meta_offset, container_buf, sizeof(uint32_t));

                base_chunk_data = container_buf->body + base_offset;
            }
            else {
                memcpy((char*)&meta_offset, container_array[base_id], sizeof(uint32_t));

#if (DEBUG_FLAG == 1)                  
                // // SyncEnclave::Logging("check read base ","metaoffset=%d, offset=%d, len=%d\n", meta_offset, base_offset, base_size);
#endif

                base_chunk_data = container_array[base_id] + meta_offset 
                    + base_offset + sizeof(uint32_t);
            }
            
            // decrypt the base chunk
            uint8_t plain_base[MAX_CHUNK_SIZE];
            uint8_t* base_iv = base_chunk_data + base_size;
            crypto_util_->DecryptionWithKeyIV(cipher_ctx_, base_chunk_data, base_size,
                SyncEnclave::enclave_key_, plain_base, base_iv);

#if (DEBUG_FLAG == 1)
            // // debug: check the plain base data
            // uint8_t check_plain_base_hash[CHUNK_HASH_SIZE];
            // crypto_util_->GenerateHMAC( plain_base, base_size, check_plain_base_hash);
            // // SyncEnclave::Logging("check plain base hash", "\n");
            // Ocall_PrintfBinary(check_plain_base_hash, CHUNK_HASH_SIZE);
#endif

            // delta decode & write
            uint8_t original_chunk[MAX_CHUNK_SIZE];
            uint32_t original_size = 0;

            if (tmp_delta_addr.type == DELTA_COMP_CHUNK) {
                // decompress the delta first
                uint8_t tmp_decomp_chunk[MAX_CHUNK_SIZE];
                int tmp_decomp_size = LZ4_decompress_safe((char*)pending_delta_chunk, 
                    (char*)tmp_decomp_chunk, tmp_delta_addr.size, MAX_CHUNK_SIZE);
                
                if (tmp_decomp_size <= 0) {
#if (DEBUG_FLAG == 1)                      
                    SyncEnclave::Logging("error ", "decompress error\n");
#endif                    
                }

#if (DEBUG_FLAG == 1)
                // // debug: check the decompress data
                // uint8_t check_decomp_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( tmp_decomp_chunk, tmp_decomp_size, check_decomp_hash);
                // // SyncEnclave::Logging("check decomp hash", "\n");
                // Ocall_PrintfBinary(check_decomp_hash, CHUNK_HASH_SIZE);
#endif

                original_size = this->DeltaDecode(plain_base, base_size, 
                    tmp_decomp_chunk, tmp_decomp_size, original_chunk);
            }
            else if (tmp_delta_addr.type == DELTA_ONLY_CHUNK) {
#if (DEBUG_FLAG == 1)
                // // debug: check the decompress data
                // uint8_t check_decomp_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( pending_delta_chunk, tmp_delta_addr.size, check_decomp_hash);
                // // SyncEnclave::Logging("check pending hash", "\n");
                // Ocall_PrintfBinary(check_decomp_hash, CHUNK_HASH_SIZE);
#endif

                original_size = this->DeltaDecode(plain_base, base_size, 
                    pending_delta_chunk, tmp_delta_addr.size, original_chunk);
            }

#if (DEBUG_FLAG == 1)  
            // // SyncEnclave::Logging("DELTA_ONLY_CHUNK decode size ", "original = %d; decode = %d; type = %d\n", tmp_delta_addr.size, original_size, tmp_debug_entry->dedupFlag);

            // debug
            // // SyncEnclave::Logging("tail decode size ", "%d\n", original_size);
            // uint8_t check_hash[CHUNK_HASH_SIZE];
            // crypto_util_->GenerateHMAC( original_chunk, original_size,
            //     check_hash);
            // Ocall_PrintfBinary(check_hash, CHUNK_HASH_SIZE);
#endif

            this->ProcessOneChunk(container_buf, original_chunk, original_size, 
                tmp_update_entry);

            // move on
            tmp_update_entry ++;
            update_num ++;
        }

        // reset
        req_container->idNum = 0;
        tmp_container_map.clear();
        base_addr_list_.clear();
    }

    update_index->queryNum = update_num;

#if (DEBUG_FLAG == 1)  
    // debug
    // // SyncEnclave::Logging("fp index insert: update num = ", "%d (%d)\n", update_index->queryNum, update_num);
#endif

    // update the fp index here
    Ocall_UpdateOutFPIndex((void*)update_index);

    if (pending_delta_list_.size() != 0) {
#if (DEBUG_FLAG == 1)          
        SyncEnclave::Logging("error ", "pending delta list not empty.\n");
#endif        
    }

    return ;
}

// for debugging
void EcallStreamWriter::ProcessBatch(uint8_t* recv_buf, uint32_t recv_size, 
    ReqContainer_t* req_container, OutChunkQuery_t* base_addr_query,
    Container_t* container_buf, OutChunkQuery_t* update_index,
    OutChunkQuery_t* debug_check_quary) {

    // SyncEnclave::Logging("in writer enclave", "%d\n", recv_size);

    _total_recv_size += recv_size;

    // dec the recv batch
    crypto_util_->DecryptWithKey(cipher_ctx_, recv_buf, recv_size, session_key_, 
        plain_in_buf_);

    // SyncEnclave::Logging("after dec", "\n");
    

    uint32_t process_size = 0;

    uint8_t* id_buf = req_container->idBuffer;
    uint8_t** container_array = req_container->containerArray;

    string tmp_container_id;
    unordered_map<string, uint32_t> tmp_container_map;
    tmp_container_map.reserve(CONTAINER_CAPPING_VALUE);

    // prepare the addr query for base chunk
    OutChunkQueryEntry_t* tmp_query_entry = base_addr_query->OutChunkQueryBase;
    uint32_t query_num = 0;

    // the chunk addr (to be updated in outside indexes)
    OutChunkQueryEntry_t* tmp_update_entry = update_index->OutChunkQueryBase;
    update_index->queryNum = 0;
    uint32_t update_num = 0;

    // the debug check query
    OutChunkQueryEntry_t* tmp_debug_entry = debug_check_quary->OutChunkQueryBase;
    debug_check_quary->queryNum = 0;
    uint32_t debug_num = 0;

    // prepare the batch fp index
    batch_fp_index_.clear();
    BatchAddrValue_t tmp_batch_addr_value;

    // process recv batch
    while(process_size != recv_size) {
#if (DEBUG_FLAG == 1)          
        // // SyncEnclave::Logging("process size = ; recv size = ", "%d, %d\n", process_size, recv_size);
#endif

        // recv chunk format [chunkSize || chunkType || (baseHash) || chunkData]
        uint32_t cur_size = *(uint32_t*)(plain_in_buf_ + process_size);
        process_size += sizeof(uint32_t);
        uint8_t cur_type = *(uint8_t*)(plain_in_buf_ + process_size);
        process_size += sizeof(uint8_t);

        // SyncEnclave::Logging("recv chunksize ", "%d\n", cur_size);
        // SyncEnclave::Logging("recv chunkType ", "%d\n", cur_type);

#if (DEBUG_FLAG == 1)  
        // // SyncEnclave::Logging("recv chunksize ", "%d\n", cur_size);
        // // SyncEnclave::Logging("recv chunkType ", "%d\n", cur_type);
#endif        

        tmp_batch_addr_value.type = cur_type;
        tmp_batch_addr_value.size = cur_size;
        tmp_batch_addr_value.offset = process_size;

        uint8_t* cur_iv = this->PickNewIV();

        uint8_t tmp_enc_chunk[MAX_CHUNK_SIZE];
        uint8_t cur_hash[CHUNK_HASH_SIZE];
        uint64_t cur_feature[SUPER_FEATURE_PER_CHUNK];
        uint8_t tmp_original_chunk[MAX_CHUNK_SIZE];
        int tmp_original_size = 0;
        uint8_t tmp_compress_chunk[MAX_CHUNK_SIZE];
        int tmp_compress_size = 0;

        if (cur_type == COMP_ONLY_CHUNK) {
            uint8_t* cur_data = plain_in_buf_ + process_size;
            // do decompression before generate chunkhash !!!!!
            tmp_original_size = LZ4_decompress_safe((char*)cur_data, 
                (char*)tmp_original_chunk, cur_size, MAX_CHUNK_SIZE);
            if (tmp_original_size > 0) {
                // SyncEnclave::Logging("can decomp ","decompsize = %d\n", tmp_original_size);

#if (DEBUG_FLAG == 1)                  
                // // SyncEnclave::Logging("can decomp ","decompsize = %d\n", tmp_original_size);
#endif                
            }
            else {

                // SyncEnclave::Logging("cannot decomp ","compsize = %d, decompsize = %d\n", cur_size, tmp_original_size);
#if (DEBUG_FLAG == 1)                  
                // // SyncEnclave::Logging("cannot decomp ","compsize = %d, decompsize = %d\n", cur_size, tmp_original_size);
#endif                

                memcpy(tmp_original_chunk, cur_data, cur_size);
                tmp_original_size = cur_size;

#if (DEBUG_FLAG == 1)  
                // debug
                uint8_t check_hash[CHUNK_HASH_SIZE];
                crypto_util_->GenerateHMAC(tmp_original_chunk, tmp_original_size,
                    check_hash);
                Ocall_PrintfBinary(check_hash, CHUNK_HASH_SIZE);
                crypto_util_->GenerateHMAC(cur_data, cur_size,
                    check_hash);
                Ocall_PrintfBinary(check_hash, CHUNK_HASH_SIZE);
#endif                
            }

            // generate chunkhash
            crypto_util_->GenerateHMAC(tmp_original_chunk, tmp_original_size, cur_hash);
#if EXTRA_HASH_FUNCTION == 1
            // use the extra hash function
            uint8_t tmp_hash_val[CHUNK_HASH_SIZE];
            crypto_util_->GenerateHash(tmp_original_chunk, tmp_original_size, tmp_hash_val);
#endif

            // insert the chunk to batch index
            string tmp_key;
            tmp_key.assign((char*)cur_hash, CHUNK_HASH_SIZE);
            batch_fp_index_.insert(make_pair(tmp_key, tmp_batch_addr_value));

#if (DEBUG_FLAG == 1)
            // // debug: check batch fp insert
            // // SyncEnclave::Logging("batch fp index insert", "\n");
            // Ocall_PrintfBinary(cur_hash, CHUNK_HASH_SIZE);
            // string check_query;
            // check_query.assign((char*)cur_hash, CHUNK_HASH_SIZE);
            // auto check_find = batch_fp_index_.find(check_query);
            // if (check_find != batch_fp_index_.end()) {
            //     // SyncEnclave::Logging("insert found", "\n");
            // }
#endif

            // extract features
            this->ExtractFeature(rabin_ctx_, tmp_original_chunk, tmp_original_size, cur_feature);

            // debug check
            memcpy(tmp_debug_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
            tmp_debug_entry->dedupFlag = COMP_ONLY_CHUNK;
            tmp_debug_entry ++;
            debug_num ++;

            // save chunk to container
#if (INDEX_ENC == 1)
            crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
            memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif            

            // enc with data key, write to container
            crypto_util_->EncryptWithKeyIV(cipher_ctx_, cur_data, cur_size,
                SyncEnclave::enclave_key_, tmp_enc_chunk, cur_iv);

            this->SaveChunk(container_buf, tmp_enc_chunk, cur_size, cur_iv, cur_hash, 
                cur_feature, &tmp_update_entry->value);

            // move on
            process_size += cur_size;
            tmp_update_entry ++;
            update_num ++;

            _only_comp_size += cur_size;
        }
        else if (cur_type == DELTA_ONLY_CHUNK) {

            // SyncEnclave::Logging("in deltaonlychunk","\n");

            // record the cur delta chunk
            uint32_t record_offset = process_size;
            uint32_t record_size = cur_size;

            // read base chunk for decoding
            uint8_t* base_hash = plain_in_buf_ + process_size;
            process_size += CHUNK_HASH_SIZE;
            uint8_t* delta_data = plain_in_buf_ + process_size;
            process_size += cur_size;

            _comp_delta_size += cur_size;

#if (DEBUG_FLAG == 1)
            // // debug
            // uint8_t check_basehash[CHUNK_HASH_SIZE];
            // memcpy(check_basehash, base_hash, CHUNK_HASH_SIZE);
#endif

            string query_local;
            query_local.assign((char*)base_hash, CHUNK_HASH_SIZE);

            auto local_find = batch_fp_index_.find(query_local);
            if (local_find != batch_fp_index_.end()) {

#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("writer check base: local delta only", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
                // TODO: ????? what if it does not exist in current container, but has been writen
#endif

                // get the base chunk in current batch
                if (local_find->second.type != COMP_ONLY_CHUNK) {
#if (DEBUG_FLAG == 1)                      
                    SyncEnclave::Logging("error ", "multi-level delta\n");
#endif                    
                    continue;
                }
                
                // delta decode
                tmp_original_size = DeltaDecode(plain_in_buf_ + local_find->second.offset,
                    local_find->second.size, delta_data, cur_size, tmp_original_chunk);
#if (DEBUG_FLAG == 1)  
                // // SyncEnclave::Logging("batch local decode size 7: ", "original = %d; decode = %d\n", cur_size,tmp_original_size);
#endif
                // generate chunkhash
                crypto_util_->GenerateHMAC(tmp_original_chunk, tmp_original_size, cur_hash);
#if EXTRA_HASH_FUNCTION == 1
                // use the extra hash function
                uint8_t tmp_hash_val[CHUNK_HASH_SIZE];
                crypto_util_->GenerateHash(tmp_original_chunk, tmp_original_size, tmp_hash_val);
#endif
                // extract features
                this->ExtractFeature(rabin_ctx_, tmp_original_chunk, tmp_original_size, cur_feature);


                // debug check
                memcpy(tmp_debug_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
                tmp_debug_entry->dedupFlag = LOCAL_DELTA_ONLY_CHUNK;
                tmp_debug_entry ++;
                debug_num ++;
                
                // local compress
                tmp_compress_size = LZ4_compress_fast((char*)tmp_original_chunk, 
                    (char*)tmp_compress_chunk, tmp_original_size, tmp_original_size,
                    3);
                if (tmp_compress_size > 0) {
                    // it can be compressed
                    // do encryption
                    crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_compress_chunk, 
                        tmp_compress_size, SyncEnclave::enclave_key_, tmp_enc_chunk, 
                        cur_iv);

#if (INDEX_ENC == 1)
                    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                        SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                    memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif

                    SaveChunk(container_buf, tmp_enc_chunk, tmp_compress_size, cur_iv, cur_hash, 
                        cur_feature, &tmp_update_entry->value);
                    
                    _comp_similar_size += tmp_compress_size;
                }
                else {
                    // it cannot be compressed
                    // do encryption
                    crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_original_chunk, 
                        tmp_original_size, SyncEnclave::enclave_key_, tmp_enc_chunk, 
                        cur_iv);

#if (INDEX_ENC == 1)
                    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                        SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                    memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif

                    SaveChunk(container_buf, tmp_enc_chunk, tmp_original_size, cur_iv, cur_hash, 
                        cur_feature, &tmp_update_entry->value);

                    _comp_similar_size += tmp_original_size;
                }

                // move on
                tmp_update_entry ++;
                update_num ++;
            }
            else {
#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("writer check base: delta only", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif
                // get the base chunk in outside container
#if (INDEX_ENC == 1)
                crypto_util_->IndexAESCMCEnc(cipher_ctx_, base_hash, CHUNK_HASH_SIZE,
                    SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                memcpy(tmp_query_entry->chunkHash, base_hash, CHUNK_HASH_SIZE);
#endif

                query_num ++;
                tmp_query_entry ++;

                // record the offset and size of the delta chunk in plain_buf
                DeltaBatchAddrValue_t tmp_record_addr;
                tmp_record_addr.offset = record_offset; // [baseHash || delta chunk]
                tmp_record_addr.size = record_size;
                tmp_record_addr.type = DELTA_ONLY_CHUNK;
                pending_delta_list_.push(tmp_record_addr);
            }

        }
        else if (cur_type == DELTA_COMP_CHUNK) {

            // SyncEnclave::Logging("in deltacompchunk","\n");


            // record the cur delta chunk
            uint32_t record_offset = process_size;
            uint32_t record_size = cur_size;

            uint8_t* base_hash = plain_in_buf_ + process_size;
            process_size += CHUNK_HASH_SIZE;
            uint8_t* comp_delta_data = plain_in_buf_ + process_size;
            process_size += cur_size;

            _comp_delta_size += cur_size;
#if (DEBUG_FLAG == 1)
            // // debug: check the basehash
            // uint8_t check_basehash[CHUNK_HASH_SIZE];
            // memcpy(check_basehash, base_hash, CHUNK_HASH_SIZE);
            // // SyncEnclave::Logging("writer check base hash1", "\n");
            // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif
            // decompress the delta first
            uint8_t tmp_decomp_chunk[MAX_CHUNK_SIZE];
            int tmp_decomp_size = LZ4_decompress_safe((char*)comp_delta_data, 
                (char*)tmp_decomp_chunk, cur_size, MAX_CHUNK_SIZE);
            
            if (tmp_decomp_size > 0) {

            }
            else {
                memcpy(tmp_decomp_chunk, comp_delta_data, cur_size);
                tmp_decomp_size = cur_size;
            }

            string query_local;
            query_local.assign((char*)base_hash, CHUNK_HASH_SIZE);

            auto local_find = batch_fp_index_.find(query_local);
            if (local_find != batch_fp_index_.end()) {
                // get the base chunk in current batch
                if (local_find->second.type != COMP_ONLY_CHUNK) {
#if (DEBUG_FLAG == 1)                      
                    SyncEnclave::Logging("error ", "multi-level delta\n");
#endif                    
                    continue;
                }

#if (DEBUG_FLAG == 1)
                // // SyncEnclave::Logging("writer check base: local delta comp", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif
                // delta decode
                tmp_original_size = DeltaDecode(plain_in_buf_ + local_find->second.offset, 
                    local_find->second.size, tmp_decomp_chunk, tmp_decomp_size, tmp_original_chunk);

#if (DEBUG_FLAG == 1)  
                // // SyncEnclave::Logging("batch local decode size 6: ", "original = %d; decode = %d\n", tmp_decomp_size,tmp_original_size);
#endif

                // generate chunkhash
                crypto_util_->GenerateHMAC(tmp_original_chunk, tmp_original_size, cur_hash);
#if EXTRA_HASH_FUNCTION == 1
                // use the extra hash function
                uint8_t tmp_hash_val[CHUNK_HASH_SIZE];
                crypto_util_->GenerateHash(tmp_original_chunk, tmp_original_size, tmp_hash_val);
#endif
                // extract features
                this->ExtractFeature(rabin_ctx_, tmp_original_chunk, tmp_original_size, cur_feature);
                
                // debug check
                memcpy(tmp_debug_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
                tmp_debug_entry->dedupFlag = LOCAL_DELTA_COMP_CHUNK;
                tmp_debug_entry ++;
                debug_num ++;

                // local compress
                tmp_compress_size = LZ4_compress_fast((char*)tmp_original_chunk, 
                    (char*)tmp_compress_chunk, tmp_original_size, tmp_original_size,
                    3);
                if (tmp_compress_size > 0) {
                    // it can be compressed
                    // do encryption
                    crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_compress_chunk, 
                        tmp_compress_size, SyncEnclave::enclave_key_, tmp_enc_chunk, 
                        cur_iv);

#if (INDEX_ENC == 1)
                    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                        SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                    memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif

                    SaveChunk(container_buf, tmp_enc_chunk, tmp_compress_size, cur_iv, cur_hash, 
                        cur_feature, &tmp_update_entry->value);
                    
                    _comp_similar_size += tmp_compress_size;
                }
                else {
                    // it cannot be compressed
                    // do encryption
                    crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_original_chunk, 
                        tmp_original_size, SyncEnclave::enclave_key_, tmp_enc_chunk, 
                        cur_iv);

#if (INDEX_ENC == 1)
                    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
                        SyncEnclave::index_query_key_, tmp_update_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                    memcpy(tmp_update_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif                    
                    SaveChunk(container_buf, tmp_enc_chunk, tmp_original_size, cur_iv, cur_hash, 
                        cur_feature, &tmp_update_entry->value);
                    
                    _comp_similar_size += tmp_original_size;
                }

                // move on
                tmp_update_entry ++;
                update_num ++;
            }
            else {
                // get the base chunk in outside container
#if (DEBUG_FLAG == 1)
                // // debug: check the basehash
                // uint8_t check_basehash[CHUNK_HASH_SIZE];
                // memcpy(check_basehash, base_hash, CHUNK_HASH_SIZE);
                // // SyncEnclave::Logging("writer check base hash2", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);

                // // SyncEnclave::Logging("writer check base: delta comp", "\n");
                // Ocall_PrintfBinary(check_basehash, CHUNK_HASH_SIZE);
#endif
                // debug
                // memcpy(tmp_query_entry->chunkHash, base_hash, CHUNK_HASH_SIZE);
#if (INDEX_ENC == 1)
                crypto_util_->IndexAESCMCEnc(cipher_ctx_, base_hash, CHUNK_HASH_SIZE,
                    SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
#endif

#if (INDEX_ENC == 0)
                memcpy(tmp_query_entry->chunkHash, base_hash, CHUNK_HASH_SIZE);
#endif                

                query_num ++;
                tmp_query_entry ++;

                // record the offset and size of the delta chunk in plain_buf
                DeltaBatchAddrValue_t tmp_record_addr;
                tmp_record_addr.offset = record_offset; // [baseHash || delta chunk]
                tmp_record_addr.size = record_size;
                tmp_record_addr.type = DELTA_COMP_CHUNK;
                pending_delta_list_.push(tmp_record_addr);
            }
        }
    }

    // SyncEnclave::Logging("after while","\n");
    

    // query to get the base chunk addr
    base_addr_query->queryNum = query_num;
    Ocall_QueryChunkAddr(base_addr_query);

    unordered_set<uint32_t> tmp_base_set;

    // SyncEnclave::Logging("after base query","\n");

    // point back to base
    tmp_query_entry = base_addr_query->OutChunkQueryBase;
    RecipeEntry_t dec_query_entry;
    EnclaveRecipeEntry_t tmp_base_entry;
    for (size_t i = 0; i < base_addr_query->queryNum; i++) {
#if (INDEX_ENC == 1)        
        crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->value,
        sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, (uint8_t*)&dec_query_entry);
#endif

#if (INDEX_ENC == 0)
        memcpy((char*)&dec_query_entry, (char*)&tmp_query_entry->value, sizeof(RecipeEntry_t));
#endif

#if (DEBUG_FLAG == 1)  
        // // SyncEnclave::Logging("base addr offset ", "%d\n", dec_query_entry.offset);
        // // SyncEnclave::Logging("base addr len ", "%d\n", dec_query_entry.length);
        // Ocall_PrintfBinary((uint8_t*)dec_query_entry.containerName, CONTAINER_ID_LENGTH);
#endif        

        // if (tmp_query_entry->dedupFlag == 101) {
        //     // record the idx
        //     tmp_base_set.insert(i);
        //     continue;
        // }

        // SyncEnclave::Logging("base addr offset ", "%d\n", dec_query_entry.offset);
        // SyncEnclave::Logging("base addr len ", "%d\n", dec_query_entry.length);
        // Ocall_PrintfBinary((uint8_t*)dec_query_entry.containerName, CONTAINER_ID_LENGTH);

        // get the chunk addr
        tmp_container_id.assign((char*)dec_query_entry.containerName, 
            CONTAINER_ID_LENGTH);
        
        tmp_base_entry.offset = dec_query_entry.offset;
        tmp_base_entry.length = dec_query_entry.length;

        auto tmp_find = tmp_container_map.find(tmp_container_id);
        if (tmp_find == tmp_container_map.end()) {
            // unique container
            tmp_base_entry.containerID = req_container->idNum;
            tmp_container_map[tmp_container_id] = req_container->idNum;
            memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
                tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
            req_container->idNum ++;
        }
        else {
            tmp_base_entry.containerID = tmp_find->second;
        }
        base_addr_list_.push_back(tmp_base_entry);

        if (tmp_query_entry->dedupFlag == 101) {
            // record the idx
            tmp_base_set.insert(i);
            // SyncEnclave::Logging("should skip base entry i", "%d\n", i);
            // continue;
        }

        if (req_container->idNum == CONTAINER_CAPPING_VALUE) {
            // fetch containers
            Ocall_SyncGetReqContainerWithSize((void*)req_container);

            for (size_t k = 0; k < base_addr_list_.size(); k++) {
                // get the delta chunk for decoding
                DeltaBatchAddrValue_t tmp_delta_addr = pending_delta_list_.front();
                pending_delta_list_.pop();

                uint8_t* pending_delta_chunk = plain_in_buf_ + tmp_delta_addr.offset
                    + CHUNK_HASH_SIZE;
#if (DEBUG_FLAG == 1)
                // // debug: calculate the sim chunk hash
                // uint8_t check_sim_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( pending_delta_chunk, tmp_delta_addr.size,
                //     check_sim_hash);
                // // SyncEnclave::Logging("check pair", "\n");
                // uint8_t check_base_hash[CHUNK_HASH_SIZE];
                // memcpy(check_base_hash, plain_in_buf_ + tmp_delta_addr.offset, CHUNK_HASH_SIZE);
                // Ocall_PrintfBinary(check_base_hash, CHUNK_HASH_SIZE);

                // Ocall_PrintfBinary(check_sim_hash, CHUNK_HASH_SIZE);
#endif
                // get the base chunk
                uint32_t base_id = base_addr_list_[k].containerID;
                uint32_t base_offset = base_addr_list_[k].offset;
                uint32_t base_size = base_addr_list_[k].length;
                uint32_t meta_offset = 0;
                uint8_t* base_chunk_data = nullptr;
                

                // checking
                auto check_iter = tmp_base_set.find(k);
                if (check_iter != tmp_base_set.end()) {
                    // the base should be skipped
                    // SyncEnclave::Logging("skip base entry k", "%d\n", k);
                    tmp_base_set.erase(k);
                    continue;
                }

                if (req_container->sizeArray[base_id] == 0) {
                    // the container is currently in-memory
                    memcpy((char*)&meta_offset, container_buf, sizeof(uint32_t));

                    base_chunk_data = container_buf->body + base_offset;
                }
                else {
                    memcpy((char*)&meta_offset, container_array[base_id], sizeof(uint32_t));
#if (DEBUG_FLAG == 1)                      
                    // // SyncEnclave::Logging("check read base ","metaoffset=%d, offset=%d, len=%d\n", meta_offset, base_offset, base_size);
#endif
                    base_chunk_data = container_array[base_id] + meta_offset 
                        + base_offset + sizeof(uint32_t);
                }
                
                // decrypt the base chunk
                uint8_t plain_base[MAX_CHUNK_SIZE];
                uint8_t* base_iv = base_chunk_data + base_size;
                crypto_util_->DecryptionWithKeyIV(cipher_ctx_, base_chunk_data, base_size,
                    SyncEnclave::enclave_key_, plain_base, base_iv);

#if (DEBUG_FLAG == 1)
                // // debug: check the plain base data
                // uint8_t check_plain_base_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( plain_base, base_size, check_plain_base_hash);
                // // SyncEnclave::Logging("check plain base hash", "\n");
                // Ocall_PrintfBinary(check_plain_base_hash, CHUNK_HASH_SIZE);
#endif
                // delta decode & write
                uint8_t original_chunk[MAX_CHUNK_SIZE];
                uint32_t original_size = 0;

                if (tmp_delta_addr.type == DELTA_COMP_CHUNK) {
                    // decompress the delta first
                    uint8_t tmp_decomp_chunk[MAX_CHUNK_SIZE];
                    int tmp_decomp_size = LZ4_decompress_safe((char*)pending_delta_chunk, 
                        (char*)tmp_decomp_chunk, tmp_delta_addr.size, MAX_CHUNK_SIZE);
                    
                    if (tmp_decomp_size <= 0) {
                        // SyncEnclave::Logging("error ", "decompress error\n");
                    }

#if (DEBUG_FLAG == 1)
                    // // debug: check the decompress data
                    // uint8_t check_decomp_hash[CHUNK_HASH_SIZE];
                    // crypto_util_->GenerateHMAC( tmp_decomp_chunk, tmp_decomp_size, check_decomp_hash);
                    // // SyncEnclave::Logging("check decomp hash", "\n");
                    // Ocall_PrintfBinary(check_decomp_hash, CHUNK_HASH_SIZE);
#endif

                    original_size = this->DeltaDecode(plain_base, base_size, 
                        tmp_decomp_chunk, tmp_decomp_size, original_chunk);
                    
                    // debug
                    tmp_debug_entry->dedupFlag = DELTA_COMP_CHUNK;
                }
                else if (tmp_delta_addr.type == DELTA_ONLY_CHUNK) {
#if (DEBUG_FLAG == 1)
                    // // debug: check the decompress data
                    // uint8_t check_decomp_hash[CHUNK_HASH_SIZE];
                    // crypto_util_->GenerateHMAC( pending_delta_chunk, tmp_delta_addr.size, check_decomp_hash);
                    // // SyncEnclave::Logging("check pending hash", "\n");
                    // Ocall_PrintfBinary(check_decomp_hash, CHUNK_HASH_SIZE);
#endif

                    original_size = this->DeltaDecode(plain_base, base_size, 
                        pending_delta_chunk, tmp_delta_addr.size, original_chunk);
                    
                    // debug
                    tmp_debug_entry->dedupFlag = DELTA_ONLY_CHUNK;
                }

#if (DEBUG_FLAG == 1)  
                // // SyncEnclave::Logging("DELTA_ONLY_CHUNK decode size ", "original = %d; decode = %d; type = %d\n", tmp_delta_addr.size, original_size, tmp_debug_entry->dedupFlag);

                // debug
                // // SyncEnclave::Logging("tail decode size ", "%d\n", original_size);
                // uint8_t check_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( original_chunk, original_size,
                //     check_hash);
                // Ocall_PrintfBinary(check_hash, CHUNK_HASH_SIZE);
#endif

                this->ProcessOneChunk(container_buf, original_chunk, original_size, 
                    tmp_update_entry, tmp_debug_entry);
                tmp_debug_entry ++;
                debug_num ++;

                // move on
                tmp_update_entry ++;
                update_num ++;
            }

            // reset
            req_container->idNum = 0;
            tmp_container_map.clear();
            base_addr_list_.clear();
        }

        tmp_query_entry ++;
    }

    // deal with tail base
    if (req_container->idNum != 0) {
#if (DEBUG_FLAG == 1)          
        // // debug
        // uint8_t check_cur_container_id[CONTAINER_ID_LENGTH];
        // memcpy(check_cur_container_id, container_buf->containerID, CONTAINER_ID_LENGTH);
        // // SyncEnclave::Logging("check cur in mem container id", "\n");
        // Ocall_PrintfBinary(check_cur_container_id, CONTAINER_ID_LENGTH);
#endif
        // fetch containers
        Ocall_SyncGetReqContainerWithSize((void*)req_container);

        for (size_t k = 0; k < base_addr_list_.size(); k++) {
            // get the delta chunk for decoding
            DeltaBatchAddrValue_t tmp_delta_addr = pending_delta_list_.front();
            pending_delta_list_.pop();

            uint8_t* pending_delta_chunk = plain_in_buf_ + tmp_delta_addr.offset
                + CHUNK_HASH_SIZE;

#if (DEBUG_FLAG == 1)
            // // debug: calculate the sim chunk hash
            // uint8_t check_sim_hash[CHUNK_HASH_SIZE];
            // crypto_util_->GenerateHMAC( pending_delta_chunk, tmp_delta_addr.size,
            //     check_sim_hash);
            // // SyncEnclave::Logging("check pair", "\n");
            // uint8_t check_base_hash[CHUNK_HASH_SIZE];
            // memcpy(check_base_hash, plain_in_buf_ + tmp_delta_addr.offset, CHUNK_HASH_SIZE);
            // Ocall_PrintfBinary(check_base_hash, CHUNK_HASH_SIZE);

            // Ocall_PrintfBinary(check_sim_hash, CHUNK_HASH_SIZE);
#endif


            // checking
            auto check_iter = tmp_base_set.find(k);
            if (check_iter != tmp_base_set.end()) {
                // the base should be skipped
                // SyncEnclave::Logging("skip base entry k", "%d\n", k);
                tmp_base_set.erase(k);
                continue;
            }

            // get the base chunk
            uint32_t base_id = base_addr_list_[k].containerID;
            uint32_t base_offset = base_addr_list_[k].offset;
            uint32_t base_size = base_addr_list_[k].length;
            uint32_t meta_offset = 0;
            uint8_t* base_chunk_data = nullptr;

            if (req_container->sizeArray[base_id] == 0) {
                // the container is currently in-memory
                memcpy((char*)&meta_offset, container_buf, sizeof(uint32_t));

                base_chunk_data = container_buf->body + base_offset;
            }
            else {
                memcpy((char*)&meta_offset, container_array[base_id], sizeof(uint32_t));
#if (DEBUG_FLAG == 1)                  
                // // SyncEnclave::Logging("check read base ","metaoffset=%d, offset=%d, len=%d\n", meta_offset, base_offset, base_size);
#endif

                base_chunk_data = container_array[base_id] + meta_offset 
                    + base_offset + sizeof(uint32_t);
            }
            
            // decrypt the base chunk
            uint8_t plain_base[MAX_CHUNK_SIZE];
            uint8_t* base_iv = base_chunk_data + base_size;
            crypto_util_->DecryptionWithKeyIV(cipher_ctx_, base_chunk_data, base_size,
                SyncEnclave::enclave_key_, plain_base, base_iv);

#if (DEBUG_FLAG == 1)
            // // debug: check the plain base data
            // uint8_t check_plain_base_hash[CHUNK_HASH_SIZE];
            // crypto_util_->GenerateHMAC( plain_base, base_size, check_plain_base_hash);
            // // SyncEnclave::Logging("check plain base hash", "\n");
            // Ocall_PrintfBinary(check_plain_base_hash, CHUNK_HASH_SIZE);
#endif

            // delta decode & write
            uint8_t original_chunk[MAX_CHUNK_SIZE];
            uint32_t original_size = 0;

            if (tmp_delta_addr.type == DELTA_COMP_CHUNK) {
                // decompress the delta first
                uint8_t tmp_decomp_chunk[MAX_CHUNK_SIZE];
                int tmp_decomp_size = LZ4_decompress_safe((char*)pending_delta_chunk, 
                    (char*)tmp_decomp_chunk, tmp_delta_addr.size, MAX_CHUNK_SIZE);
                
                if (tmp_decomp_size <= 0) {
#if (DEBUG_FLAG == 1)                      
                    SyncEnclave::Logging("error ", "decompress error\n");
#endif                    
                }

#if (DEBUG_FLAG == 1)
                // // debug: check the decompress data
                // uint8_t check_decomp_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( tmp_decomp_chunk, tmp_decomp_size, check_decomp_hash);
                // // SyncEnclave::Logging("check decomp hash", "\n");
                // Ocall_PrintfBinary(check_decomp_hash, CHUNK_HASH_SIZE);
#endif

                original_size = this->DeltaDecode(plain_base, base_size, 
                    tmp_decomp_chunk, tmp_decomp_size, original_chunk);
                
                // debug
                tmp_debug_entry->dedupFlag = DELTA_COMP_CHUNK;
            }
            else if (tmp_delta_addr.type == DELTA_ONLY_CHUNK) {
#if (DEBUG_FLAG == 1)
                // // debug: check the decompress data
                // uint8_t check_decomp_hash[CHUNK_HASH_SIZE];
                // crypto_util_->GenerateHMAC( pending_delta_chunk, tmp_delta_addr.size, check_decomp_hash);
                // // SyncEnclave::Logging("check pending hash", "\n");
                // Ocall_PrintfBinary(check_decomp_hash, CHUNK_HASH_SIZE);
#endif

                original_size = this->DeltaDecode(plain_base, base_size, 
                    pending_delta_chunk, tmp_delta_addr.size, original_chunk);
                
                // debug
                tmp_debug_entry->dedupFlag = DELTA_ONLY_CHUNK;
            }

#if (DEBUG_FLAG == 1)  
            // // SyncEnclave::Logging("DELTA_ONLY_CHUNK decode size ", "original = %d; decode = %d; type = %d\n", tmp_delta_addr.size, original_size, tmp_debug_entry->dedupFlag);

            // debug
            // // SyncEnclave::Logging("tail decode size ", "%d\n", original_size);
            // uint8_t check_hash[CHUNK_HASH_SIZE];
            // crypto_util_->GenerateHMAC( original_chunk, original_size,
            //     check_hash);
            // Ocall_PrintfBinary(check_hash, CHUNK_HASH_SIZE);
#endif

            this->ProcessOneChunk(container_buf, original_chunk, original_size, 
                tmp_update_entry, tmp_debug_entry);
            tmp_debug_entry ++;
            debug_num ++;

            // move on
            tmp_update_entry ++;
            update_num ++;
        }

        // reset
        req_container->idNum = 0;
        tmp_container_map.clear();
        base_addr_list_.clear();
    }

    // SyncEnclave::Logging("before index update", "\n");

    update_index->queryNum = update_num;

    debug_check_quary->queryNum = debug_num;

    Ocall_CheckDebugIndex((void*)debug_check_quary);

    // SyncEnclave::Logging("after debug index", "\n");


#if (DEBUG_FLAG == 1)  
    // debug
    // // SyncEnclave::Logging("fp index insert: update num = ", "%d (%d)\n", update_index->queryNum, update_num);
#endif

    // update the fp index here
    Ocall_UpdateOutFPIndex((void*)update_index);

    // SyncEnclave::Logging("after update index", "\n");


    if (pending_delta_list_.size() != 0) {
#if (DEBUG_FLAG == 1)          
        SyncEnclave::Logging("error ", "pending delta list not empty.\n");
#endif        
    }

    return ;
}

void EcallStreamWriter::ProcessOneChunk(Container_t* container_buf, uint8_t* chunk_data, 
    uint32_t chunk_size, OutChunkQueryEntry_t* chunk_addr,
    OutChunkQueryEntry_t* debug_check_entry) {
    uint8_t* cur_iv = this->PickNewIV();

    // perform local compress
    uint8_t tmp_compress_chunk[MAX_CHUNK_SIZE];
    int tmp_compress_size = 0;

    // enc with data key
    uint8_t tmp_enc_chunk[MAX_CHUNK_SIZE];
    uint32_t tmp_enc_size = 0;

    // generate chunk hash first
    uint8_t cur_hash[CHUNK_HASH_SIZE];
    crypto_util_->GenerateHMAC(chunk_data, chunk_size, cur_hash);
#if EXTRA_HASH_FUNCTION == 1
    // use the extra hash function
    uint8_t tmp_hash_val[CHUNK_HASH_SIZE];
    crypto_util_->GenerateHash(chunk_data, chunk_size, tmp_hash_val);
#endif
#if (DEBUG_FLAG == 1)
    // // debug
    // // SyncEnclave::Logging("process one chunk ", "hash = \n");
    // Ocall_PrintfBinary(cur_hash, CHUNK_HASH_SIZE);
#endif

    memcpy(debug_check_entry->chunkHash, cur_hash, CHUNK_HASH_SIZE);

    // debug
    // memcpy(chunk_addr->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#if (INDEX_ENC == 1)
    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
        SyncEnclave::index_query_key_, chunk_addr->chunkHash);
#endif

#if (INDEX_ENC == 0)
    memcpy(chunk_addr->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif    

    // generate features
    uint64_t cur_features[SUPER_FEATURE_PER_CHUNK];
    this->ExtractFeature(rabin_ctx_, chunk_data, chunk_size, cur_features);

    tmp_compress_size = LZ4_compress_fast((char*)chunk_data, (char*)tmp_compress_chunk, 
        chunk_size, chunk_size, 3);
    
    if (tmp_compress_size > 0) {
        crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_compress_chunk, tmp_compress_size,
            SyncEnclave::enclave_key_, tmp_enc_chunk, cur_iv);
        tmp_enc_size = tmp_compress_size;
    }
    else {
        // this chunk cannot be compressed
        crypto_util_->EncryptWithKeyIV(cipher_ctx_, chunk_data, chunk_size,
            SyncEnclave::enclave_key_, tmp_enc_chunk, cur_iv);
        tmp_enc_size = chunk_size;
    }

    // save chunk to container
    this->SaveChunk(container_buf, tmp_enc_chunk, tmp_enc_size, cur_iv, cur_hash, 
        cur_features, &chunk_addr->value);
    
    _comp_similar_size += tmp_enc_size;

    return ;
}


/**
 * @brief process one recovered chunk
 * 
 * @param chunk_data 
 * @param chunk_size 
 */
void EcallStreamWriter::ProcessOneChunk(Container_t* container_buf, uint8_t* chunk_data, 
    uint32_t chunk_size, OutChunkQueryEntry_t* chunk_addr) {
    uint8_t* cur_iv = this->PickNewIV();

    // perform local compress
    uint8_t tmp_compress_chunk[MAX_CHUNK_SIZE];
    int tmp_compress_size = 0;

    // enc with data key
    uint8_t tmp_enc_chunk[MAX_CHUNK_SIZE];
    uint32_t tmp_enc_size = 0;

    // generate chunk hash first
    uint8_t cur_hash[CHUNK_HASH_SIZE];
    crypto_util_->GenerateHMAC(chunk_data, chunk_size, cur_hash);
#if EXTRA_HASH_FUNCTION == 1
    // use the extra hash function
    uint8_t tmp_hash_val[CHUNK_HASH_SIZE];
    crypto_util_->GenerateHash(chunk_data, chunk_size, tmp_hash_val);
#endif
#if (DEBUG_FLAG == 1)
    // // debug
    // // SyncEnclave::Logging("process one chunk ", "hash = \n");
    // Ocall_PrintfBinary(cur_hash, CHUNK_HASH_SIZE);
#endif
    // debug
    // memcpy(chunk_addr->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#if (INDEX_ENC == 1)
    crypto_util_->IndexAESCMCEnc(cipher_ctx_, cur_hash, CHUNK_HASH_SIZE,
        SyncEnclave::index_query_key_, chunk_addr->chunkHash);
#endif

#if (INDEX_ENC == 0)
    memcpy(chunk_addr->chunkHash, cur_hash, CHUNK_HASH_SIZE);
#endif    

    // generate features
    uint64_t cur_features[SUPER_FEATURE_PER_CHUNK];
    this->ExtractFeature(rabin_ctx_, chunk_data, chunk_size, cur_features);

    tmp_compress_size = LZ4_compress_fast((char*)chunk_data, (char*)tmp_compress_chunk, 
        chunk_size, chunk_size, 3);
    
    if (tmp_compress_size > 0) {
        crypto_util_->EncryptWithKeyIV(cipher_ctx_, tmp_compress_chunk, tmp_compress_size,
            SyncEnclave::enclave_key_, tmp_enc_chunk, cur_iv);
        tmp_enc_size = tmp_compress_size;
    }
    else {
        // this chunk cannot be compressed
        crypto_util_->EncryptWithKeyIV(cipher_ctx_, chunk_data, chunk_size,
            SyncEnclave::enclave_key_, tmp_enc_chunk, cur_iv);
        tmp_enc_size = chunk_size;
    }

    // save chunk to container
    this->SaveChunk(container_buf, tmp_enc_chunk, tmp_enc_size, cur_iv, cur_hash, 
        cur_features, &chunk_addr->value);
    
    _comp_similar_size += tmp_enc_size;

    return ;
}

/**
 * @brief delta decoding
 * 
 * @param base_chunk 
 * @param base_size 
 * @param delta_chunk 
 * @param delta_size 
 * @param output_chunk 
 * @return uint32_t 
 */
uint32_t EcallStreamWriter::DeltaDecode(uint8_t* base_chunk, uint32_t base_size,
    uint8_t* delta_chunk, uint32_t delta_size, uint8_t* output_chunk) {
    
    // decompress the base chunk first
    uint8_t original_base[MAX_CHUNK_SIZE];
    int original_base_size = 0;

    original_base_size = LZ4_decompress_safe((char*)base_chunk, (char*)original_base, 
        base_size, MAX_CHUNK_SIZE);
    if (original_base_size > 0) {

    }
    else {
        memcpy(original_base, base_chunk, base_size);
        original_base_size = base_size;
#if (DEBUG_FLAG == 1)          
        // SyncEnclave::Logging("cannot decomp ","decompsize = %d\n", original_base_size);
#endif        
    }
    
    uint64_t ret_size = 0;
    int ret = xd3_decode_memory(delta_chunk, delta_size, original_base, original_base_size,
        output_chunk, &ret_size, MAX_CHUNK_SIZE, delta_flag_);    
    if (ret != 0) {
#if (DEBUG_FLAG == 1)          
        SyncEnclave::Logging(my_name_.c_str(), "delta decoding fails: %d.\n", ret);
#endif        
    }

    _uncomp_delta_size += delta_size;
    _uncomp_similar_size += ret_size;

    return ret_size;
}

/**
 * @brief generate features for one chunk
 * 
 * @param rabin_ctx 
 * @param data 
 * @param size 
 * @param features 
 */
void EcallStreamWriter::ExtractFeature(RabinCtx_t &rabin_ctx, uint8_t* data, uint32_t size, 
    uint64_t* features) {

    finesse_util_->ExtractFeature(rabin_ctx_, data, size,
            features);
#if (DEBUG_FLAG == 1)  
    // for(size_t i = 0; i < SUPER_FEATURE_PER_CHUNK; i++) {
    //     // SyncEnclave::Logging("chunk features", "%d\t", features[i]);
    // }
    // // SyncEnclave::Logging("chunk features", "\n");
#endif
    return ;
}

/**
 * @brief save the recovered enc chunk
 * 
 * @param chunk_data 
 * @param chunk_size 
 * @param chunk_iv 
 * @param chunk_hash 
 * @param features 
 */
void EcallStreamWriter::SaveChunk(Container_t* container_buf, uint8_t* chunk_data, 
    uint32_t chunk_size, uint8_t* chunk_iv, uint8_t* chunk_hash, uint64_t* features, 
    RecipeEntry_t* chunk_addr) {

    // prepare the recipe entry value
    RecipeEntry_t tmp_entry;
    tmp_entry.length = chunk_size;

    uint32_t save_offset = container_buf->currentSize;
    uint32_t write_offset = save_offset;
    uint32_t save_meta_offset = container_buf->currentMetaSize;
    uint32_t write_meta_offset = save_meta_offset;

    _total_write_size += chunk_size;

    uint32_t feature_size = SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
    if ((CRYPTO_BLOCK_SIZE + chunk_size + save_offset + save_meta_offset + CHUNK_HASH_SIZE + feature_size < MAX_CONTAINER_SIZE) && (save_meta_offset + CHUNK_HASH_SIZE + feature_size < MAX_META_SIZE)) {
        // current container buf can store this chunk

        // write the metadata session (features || chunkhash)
        memcpy(container_buf->metadata + write_meta_offset, (uint8_t*)features, 
            SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
        write_meta_offset += feature_size;
        memcpy(container_buf->metadata + write_meta_offset, chunk_hash,
            CHUNK_HASH_SIZE);

        // write the chunk data and iv
        memcpy(container_buf->body + write_offset, chunk_data, chunk_size);
        write_offset += chunk_size;
        memcpy(container_buf->body + write_offset, chunk_iv, CRYPTO_BLOCK_SIZE);
        // write_offset += CRYPTO_BLOCK_SIZE;
        memcpy(tmp_entry.containerName, container_buf->containerID, CONTAINER_ID_LENGTH);
    } else {
        // do ocall: write current container to disk
        Ocall_WriteSyncContainer(container_buf);
        
        // reset
        container_buf->currentSize = 0;
        container_buf->currentMetaSize = 0;

        save_offset = 0;
        write_offset = 0;
        save_meta_offset = 0;
        write_meta_offset = 0;

        // write new chunk
        // write the metadata session (features || chunkhash)
        memcpy(container_buf->metadata + write_meta_offset, (uint8_t*)features, 
            SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
        write_meta_offset += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
        memcpy(container_buf->metadata + write_meta_offset, chunk_hash,
            CHUNK_HASH_SIZE);

        // write the chunk data and iv
        memcpy(container_buf->body + write_offset, chunk_data, chunk_size);
        write_offset += chunk_size;
        memcpy(container_buf->body + write_offset, chunk_iv, CRYPTO_BLOCK_SIZE);
        // write_offset += CRYPTO_BLOCK_SIZE;
        memcpy(tmp_entry.containerName, container_buf->containerID, CONTAINER_ID_LENGTH);
    }

    container_buf->currentSize += chunk_size;
    container_buf->currentSize += CRYPTO_BLOCK_SIZE;
    container_buf->currentMetaSize += feature_size;
    container_buf->currentMetaSize += CHUNK_HASH_SIZE;

    tmp_entry.offset = save_offset;

#if (DEBUG_FLAG == 1)  
    // // SyncEnclave::Logging("write addr ", "value: %d, %d\n", tmp_entry.offset, tmp_entry.length);

    // // debug: check the recover correctness
    // // SyncEnclave::Logging("check write hash", "\n");
    // uint8_t check_write_hash[CHUNK_HASH_SIZE];
    // memcpy(check_write_hash, chunk_hash, CHUNK_HASH_SIZE);
    // Ocall_PrintfBinary(check_write_hash, CHUNK_HASH_SIZE);
#endif

    // prepare the index update
#if (INDEX_ENC == 1)    
    crypto_util_->AESCBCEnc(cipher_ctx_, (uint8_t*)&tmp_entry, sizeof(RecipeEntry_t),
        SyncEnclave::index_query_key_, (uint8_t*)chunk_addr);
#endif

#if (INDEX_ENC == 0)
    memcpy((char*)chunk_addr, (char*)&tmp_entry, sizeof(RecipeEntry_t));
#endif    

    return ;
}