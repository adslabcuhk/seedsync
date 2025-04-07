/**
 * @file ecallStreamEncode.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-06-08
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/ecallStreamEncode.h"

/**
 * @brief Construct a new Ecall Stream Encode object
 * 
 */
EcallStreamEncode::EcallStreamEncode() {
    crypto_util_ = new EcallCrypto(CIPHER_TYPE, HASH_TYPE);
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    md_ctx_ = EVP_MD_CTX_new();

    plain_in_buf_ = (uint8_t*)malloc(SyncEnclave::send_chunk_batch_size_ * (MAX_CHUNK_SIZE + sizeof(uint32_t) + sizeof(uint8_t) + CHUNK_HASH_HMAC_SIZE));
    // SyncEnclave::Logging("plain in buf size ", "%d\n", SyncEnclave::send_chunk_batch_size_ *
    //     (MAX_CHUNK_SIZE + sizeof(uint32_t) + sizeof(uint8_t) + CHUNK_HASH_HMAC_SIZE));
    out_offset_ = 0;

    SyncEnclave::Logging(my_name_.c_str(), "init EcallStreamEncode.\n");
}

/**
 * @brief Destroy the Ecall Stream Encode object
 * 
 */
EcallStreamEncode::~EcallStreamEncode() {
    delete(crypto_util_);
    EVP_CIPHER_CTX_free(cipher_ctx_);
    EVP_MD_CTX_free(md_ctx_);

    free(plain_in_buf_);
}

/**
 * @brief process a batch of data
 * 
 * @param in_buf 
 * @param in_size 
 * @param req_container 
 * @param addr_query 
 * @param out_buf 
 * @param out_size 
 */
void EcallStreamEncode::ProcessBatch(uint8_t* in_buf, uint32_t in_size, 
    ReqContainer_t* req_container, OutChunkQuery_t* addr_query, 
    uint8_t* out_buf, uint32_t* out_size) {
    
    // reset
    out_offset_ = 0;

    // decrypt the batch with session key first
    // crypto_util_->DecryptWithKey(cipher_ctx_, in_buf, in_size, session_key_, 
    //     plain_in_buf_);

    // debug
    memcpy(plain_in_buf_, in_buf, in_size);
    
    uint8_t* id_buf = req_container->idBuffer;
    uint8_t** container_array = req_container->containerArray;

    string tmp_container_id;
    unordered_map<string, uint32_t> tmp_container_map;
    tmp_container_map.reserve(CONTAINER_CAPPING_VALUE);

    size_t in_num = in_size / sizeof(StreamPhase4MQ_t);
    // SyncEnclave::Logging("in num ", "%d\n", in_num);

    // debug: reset the outquery entry
    OutChunkQueryEntry_t* reset_query_entry = addr_query->OutChunkQueryBase;
    for (size_t i = 0; i < in_num; i++) {
        memset((char*)reset_query_entry, 0, sizeof(OutChunkQueryEntry_t));
        reset_query_entry ++;
    }

    // query the chunk index to get the chunk Addr
    OutChunkQueryEntry_t* tmp_query_entry = addr_query->OutChunkQueryBase;
    uint32_t query_num = 0;
    StreamPhase4MQ_t* get_hash_entry = (StreamPhase4MQ_t*) plain_in_buf_;
    for (size_t i = 0; i < in_num; i++) {
        // SyncEnclave::Logging("check sim tag in get hash entry", "%d\n", get_hash_entry->sim_tag);
        if (get_hash_entry->sim_tag == SIMILAR_CHUNK || 
            get_hash_entry->sim_tag == BATCH_SIMILAR_CHUNK) {
            crypto_util_->IndexAESCMCEnc(cipher_ctx_, get_hash_entry->chunkHash,
                CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
            // prepare the sim_tag in local_addr_list_ here
            // tmp_local_addr_entry.sim_tag = SIMILAR_CHUNK;
            // local_addr_list_.push_back(tmp_local_addr_entry);
            tmp_query_entry->dedupFlag = get_hash_entry->sim_tag;
            
            // move on output buf
            tmp_query_entry ++;
            query_num ++;

            crypto_util_->IndexAESCMCEnc(cipher_ctx_, get_hash_entry->baseHash,
                CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
            // prepare the sim_tag in local_addr_list_ here
            // tmp_local_addr_entry.sim_tag = BASE_CHUNK;
            tmp_query_entry->dedupFlag = BASE_CHUNK;
        }
        else {
            crypto_util_->IndexAESCMCEnc(cipher_ctx_, get_hash_entry->chunkHash,
                CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, tmp_query_entry->chunkHash);
            // prepare the sim_tag in local_addr_list_ here
            // tmp_local_addr_entry.sim_tag = NON_SIMILAR_CHUNK;

            // // debug
            // SyncEnclave::Logging("check hash in get hash entry", "\n");
            // Ocall_PrintfBinary(get_hash_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);

            tmp_query_entry->dedupFlag = NON_SIMILAR_CHUNK;
        }

        // local_addr_list_.push_back(tmp_local_addr_entry);
        tmp_query_entry ++;
        query_num ++;
        // move on input buf
        get_hash_entry ++;
    }
    // SyncEnclave::Logging("query num ", "%d\n", query_num);
    addr_query->queryNum = query_num;
    Ocall_QueryChunkAddr((void*)addr_query);

    // point back to base
    tmp_query_entry = addr_query->OutChunkQueryBase;
    RecipeEntry_t dec_query_entry;
    SyncRecipeEntry_t tmp_local_addr_entry;
    for (size_t i = 0; i < addr_query->queryNum; i++) {
        crypto_util_->AESCBCDec(cipher_ctx_, (uint8_t*)&tmp_query_entry->value, 
            sizeof(RecipeEntry_t), SyncEnclave::index_query_key_, 
            (uint8_t*)&dec_query_entry);
        
        // SyncEnclave::Logging("encode addr", "offset=%d; len=%d; type=%d\n", dec_query_entry.offset, dec_query_entry.length, tmp_query_entry->dedupFlag);
        // Ocall_PrintfBinary(dec_query_entry.containerName, CONTAINER_ID_LENGTH);

        // prepare the addr in local_addr_list_ here
        tmp_container_id.assign((char*)dec_query_entry.containerName, CONTAINER_ID_LENGTH);
        tmp_local_addr_entry.offset = dec_query_entry.offset;
        tmp_local_addr_entry.length = dec_query_entry.length;
        tmp_local_addr_entry.sim_tag = tmp_query_entry->dedupFlag;
        if (tmp_query_entry->dedupFlag == BASE_CHUNK) {
            // memcpy(tmp_local_addr_entry.baseHash, tmp_query_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);
            crypto_util_->IndexAESCMCDec(cipher_ctx_, tmp_query_entry->chunkHash,
                CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, tmp_local_addr_entry.baseHash);

            // // debug: check baseHash
            // uint8_t check_base[CHUNK_HASH_HMAC_SIZE];
            // memcpy(check_base, tmp_local_addr_entry.baseHash, CHUNK_HASH_HMAC_SIZE);
            // SyncEnclave::Logging("encode check base1", "\n");
            // Ocall_PrintfBinary(check_base, CHUNK_HASH_HMAC_SIZE);
        }

        auto find_cont = tmp_container_map.find(tmp_container_id);
        if (find_cont == tmp_container_map.end()) {
            // unique container for load buffer
            // SyncEnclave::Logging("uni container","\n");
            // Ocall_PrintfBinary((uint8_t*)tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
            tmp_local_addr_entry.containerID = req_container->idNum;
            tmp_container_map[tmp_container_id] = req_container->idNum;
            memcpy(id_buf + req_container->idNum * CONTAINER_ID_LENGTH, 
                tmp_container_id.c_str(), CONTAINER_ID_LENGTH);
            req_container->idNum ++;
        }
        else {
            // exist in load buffer, get the local id
            // SyncEnclave::Logging("exist container","\n");
            tmp_local_addr_entry.containerID = find_cont->second;
        }
        local_addr_list_.push_back(tmp_local_addr_entry);

        if (req_container->idNum == CONTAINER_CAPPING_VALUE) {
            // the batch end with a similar chunk, so we load the base chunk (+1)
            // fetch containers
            Ocall_SyncGetReqContainer((void*)req_container);
            // SyncEnclave::Logging("after ocall req cont1","\n");

            // process the chunks
            for (size_t k = 0; k < local_addr_list_.size(); k++) {
                // data sending format: [chunkSize; chunkType; chunkData]
                if (local_addr_list_[k].sim_tag == NON_SIMILAR_CHUNK) {
                    // decrypt and directly send the compressed chunk
                    uint32_t local_id = local_addr_list_[k].containerID;
                    uint32_t local_offset = local_addr_list_[k].offset;
                    uint32_t local_size = local_addr_list_[k].length;
                    uint32_t meta_offset = 0;
                    memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
                    
                    uint8_t* local_chunk_data = container_array[local_id] + local_offset
                        + meta_offset + sizeof(uint32_t);

                    // debug: check the compress data
                    // SyncEnclave::Logging("check compress:", "offset=%d, len=%d\n", local_offset, local_size);
                    //TODO: compute HMAC to verify non-similar chunk
                    uint8_t currentNonSimilarChunkHMAC[CHUNK_HASH_HMAC_SIZE];
                    crypto_util_->GenerateHMAC(local_chunk_data, local_size, currentNonSimilarChunkHMAC);
                    this->ProcessNonSimChunk(local_chunk_data, local_size);
                }
                if (local_addr_list_[k].sim_tag == SIMILAR_CHUNK || 
                    local_addr_list_[k].sim_tag == BATCH_SIMILAR_CHUNK) {
                    uint32_t sim_id = local_addr_list_[k].containerID;
                    uint32_t sim_offset = local_addr_list_[k].offset;
                    uint32_t sim_size = local_addr_list_[k].length;
                    uint32_t meta_offset = 0;
                    memcpy((char*)&meta_offset, container_array[sim_id], sizeof(uint32_t));

                    uint8_t* sim_chunk_data = container_array[sim_id] + sim_offset
                        + meta_offset + sizeof(uint32_t);

                    uint32_t base_id = local_addr_list_[k + 1].containerID;
                    uint32_t base_offset = local_addr_list_[k + 1].offset;
                    uint32_t base_size = local_addr_list_[k + 1].length;
                    meta_offset = 0;
                    memcpy((char*)&meta_offset, container_array[base_id], sizeof(uint32_t));

                    uint8_t* base_chunk_data = container_array[base_id] + base_offset
                        + meta_offset + sizeof(uint32_t);
                    //TODO: compute HMAC to verify base chunk and similar chunk
                    uint8_t currentSimilarChunkHMAC[CHUNK_HASH_HMAC_SIZE];
                    crypto_util_->GenerateHMAC(sim_chunk_data, sim_size, currentSimilarChunkHMAC);
                    uint8_t currentBaseChunkHMAC[CHUNK_HASH_HMAC_SIZE];
                    crypto_util_->GenerateHMAC(base_chunk_data, base_size, currentBaseChunkHMAC);
                    this->PocessSimChunk(base_chunk_data, base_size, 
                        local_addr_list_[k + 1].baseHash, sim_chunk_data, sim_size);
                }
            }

            // reset 
            req_container->idNum = 0;
            tmp_container_map.clear();
            local_addr_list_.clear();
        }

        if (req_container->idNum == CONTAINER_CAPPING_VALUE - 1) {
            // check whether the tail chunk is a similar chunk
            if (local_addr_list_[local_addr_list_.size() - 1].sim_tag == SIMILAR_CHUNK) {
                // if the tail chunk is a similar chunk, we need to fetch the base chunk
                // for encoding
                continue;
            }
            else {
                // fetch containers
                Ocall_SyncGetReqContainer((void*)req_container);
                // SyncEnclave::Logging("after ocall req cont2","\n");
 
                for (size_t k = 0; k < local_addr_list_.size(); k++) {
                    // data sending format: [chunkSize; chunkType; chunkData]
                    if (local_addr_list_[k].sim_tag == NON_SIMILAR_CHUNK) {
                        // decrypt and directly send the compressed chunk
                        uint32_t local_id = local_addr_list_[k].containerID;
                        uint32_t local_offset = local_addr_list_[k].offset;
                        uint32_t local_size = local_addr_list_[k].length;
                        uint32_t meta_offset = 0;
                        memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));

                        uint8_t* local_chunk_data = container_array[local_id] + local_offset
                            + meta_offset + sizeof(uint32_t);
                        // SyncEnclave::Logging("check compress:", "offset=%d, len=%d\n", local_offset, local_size);

                        this->ProcessNonSimChunk(local_chunk_data, local_size);
                    }
                    if (local_addr_list_[k].sim_tag == SIMILAR_CHUNK || 
                        local_addr_list_[k].sim_tag == BATCH_SIMILAR_CHUNK) {
                        uint32_t sim_id = local_addr_list_[k].containerID;
                        uint32_t sim_offset = local_addr_list_[k].offset;
                        uint32_t sim_size = local_addr_list_[k].length;
                        uint32_t meta_offset = 0;
                        memcpy((char*)&meta_offset, container_array[sim_id], sizeof(uint32_t));

                        uint8_t* sim_chunk_data = container_array[sim_id] + sim_offset
                            + meta_offset + sizeof(uint32_t);

                        uint32_t base_id = local_addr_list_[k + 1].containerID;
                        uint32_t base_offset = local_addr_list_[k + 1].offset;
                        uint32_t base_size = local_addr_list_[k + 1].length;
                        meta_offset = 0;
                        memcpy((char*)&meta_offset, container_array[base_id], sizeof(uint32_t));

                        uint8_t* base_chunk_data = container_array[base_id] + base_offset
                            + meta_offset + sizeof(uint32_t);

                        this->PocessSimChunk(base_chunk_data, base_size, 
                            local_addr_list_[k + 1].baseHash, sim_chunk_data, sim_size);
                    }
                }

                // reset 
                req_container->idNum = 0;
                tmp_container_map.clear();
                local_addr_list_.clear();
            }
        }

        // move on 
        tmp_query_entry ++;
    }

    // deal with tail
    if (req_container->idNum != 0) {
        Ocall_SyncGetReqContainer((void*)req_container);
        // SyncEnclave::Logging("after ocall req cont3","\n");

        // // debug
        // for (size_t k = 0; k < req_container->idNum; k++) {
        //     // SyncEnclave::Logging("cont name","\n");
        //     uint8_t check_name[CONTAINER_ID_LENGTH];
        //     memcpy(check_name, id_buf + k * CONTAINER_ID_LENGTH, CONTAINER_ID_LENGTH);
        //     // Ocall_PrintfBinary(check_name, CONTAINER_ID_LENGTH); 
        // }

        for (size_t k = 0; k < local_addr_list_.size(); k++) {
            // SyncEnclave::Logging("check sim tag ", "%d\n", local_addr_list_[k].sim_tag);

            // data sending format: [chunkSize; chunkType; chunkData]
            if (local_addr_list_[k].sim_tag == NON_SIMILAR_CHUNK) {
                // SyncEnclave::Logging("non-sim chunk ", "\n");
                // decrypt and directly send the compressed chunk
                uint32_t local_id = local_addr_list_[k].containerID;
                uint32_t local_offset = local_addr_list_[k].offset;
                uint32_t local_size = local_addr_list_[k].length;
                uint32_t meta_offset = 0;
                memcpy((char*)&meta_offset, container_array[local_id], sizeof(uint32_t));
                // SyncEnclave::Logging("meta offset ", "%d\n", meta_offset);
                // SyncEnclave::Logging("local offset ", "%d\n", local_offset);
                // SyncEnclave::Logging("local size ", "%d\n", local_size);
                // SyncEnclave::Logging("local id ", "%d\n", local_id);

                uint8_t* local_chunk_data = container_array[local_id] + local_offset 
                    + meta_offset + sizeof(uint32_t);
                
                // SyncEnclave::Logging("check compress:", "metaoffset=%d, offset=%d, len=%d\n", meta_offset, local_offset, local_size);

                this->ProcessNonSimChunk(local_chunk_data, local_size);
            }
            if (local_addr_list_[k].sim_tag == SIMILAR_CHUNK ||
                local_addr_list_[k].sim_tag == BATCH_SIMILAR_CHUNK) {
                
                // if (local_addr_list_[k].sim_tag == SIMILAR_CHUNK) {
                //     SyncEnclave::Logging("sim chunk ", "\n");
                // }

                // if (local_addr_list_[k].sim_tag == BATCH_SIMILAR_CHUNK) {
                //     SyncEnclave::Logging("batch sim chunk ", "\n");
                // }
                
                uint32_t sim_id = local_addr_list_[k].containerID;
                uint32_t sim_offset = local_addr_list_[k].offset;
                uint32_t sim_size = local_addr_list_[k].length;
                uint32_t meta_offset = 0;
                memcpy((char*)&meta_offset, container_array[sim_id], sizeof(uint32_t));
                // SyncEnclave::Logging("meta offset ", "%d\n", meta_offset);

                uint8_t* sim_chunk_data = container_array[sim_id] + sim_offset 
                    + meta_offset + sizeof(uint32_t);

                uint32_t base_id = local_addr_list_[k + 1].containerID;
                uint32_t base_offset = local_addr_list_[k + 1].offset;
                uint32_t base_size = local_addr_list_[k + 1].length;
                meta_offset = 0;
                memcpy((char*)&meta_offset, container_array[base_id], sizeof(uint32_t));
                // SyncEnclave::Logging("meta offset ", "%d\n", meta_offset);

                uint8_t* base_chunk_data = container_array[base_id] + base_offset 
                    + meta_offset + sizeof(uint32_t);

                this->PocessSimChunk(base_chunk_data, base_size, 
                    local_addr_list_[k + 1].baseHash, sim_chunk_data, sim_size);
            }
        }

        // reset 
        req_container->idNum = 0;
        tmp_container_map.clear();
        local_addr_list_.clear();
    }

    // encrypt the send batch
    crypto_util_->EncryptWithKey(cipher_ctx_, plain_in_buf_, out_offset_, 
        session_key_, out_buf);
    *out_size = out_offset_;

    return ;
}

/**
 * @brief process a similar chunk
 * 
 * @param base_chunk 
 * @param base_size 
 * @param input_chunk 
 * @param input_size 
 */
void EcallStreamEncode::PocessSimChunk(uint8_t* base_chunk_data, uint32_t base_size,
    uint8_t* base_hash, uint8_t* sim_chunk_data, uint32_t sim_size) {
    
    _total_similar_num ++;
    
    // SyncEnclave::Logging("in ProcessSim", "\n");
    uint8_t delta_chunk[MAX_CHUNK_SIZE];
    uint32_t delta_size = 0;
    uint8_t chunk_type;
    // delta encode, compress the delta chunk
    if (!this->DeltaEncode(base_chunk_data, base_size, sim_chunk_data, sim_size,
        delta_chunk, delta_size, chunk_type)) {
        SyncEnclave::Logging("delta encode fails", "\n");

        // prepare the [chunkSize; chunkType]
        memcpy(plain_in_buf_ + out_offset_, &sim_size, sizeof(uint32_t));
        out_offset_ += sizeof(uint32_t);
        memcpy(plain_in_buf_ + out_offset_, &chunk_type, sizeof(uint8_t));
        out_offset_ += sizeof(uint8_t);
        // get the [chunk data]
        // decrypt with data key
        crypto_util_->DecryptionWithKeyIV(cipher_ctx_, sim_chunk_data, 
            sim_size, SyncEnclave::enclave_key_,
            plain_in_buf_ + out_offset_, sim_chunk_data + sim_size);
        out_offset_ += sim_size;
    }
    else {
        // SyncEnclave::Logging(" ", "prev len = %d, delta len = %d", sim_size, delta_size);
        
        // prepare the [chunkSize; chunkType; baseHash; delta chunk]
        memcpy(plain_in_buf_ + out_offset_, &delta_size, sizeof(uint32_t));
        out_offset_ += sizeof(uint32_t);

        // debug: check chunktype
        // SyncEnclave::Logging("chunk type ", "%d", chunk_type);

        memcpy(plain_in_buf_ + out_offset_, &chunk_type, sizeof(uint8_t));
        out_offset_ += sizeof(uint8_t);
        memcpy(plain_in_buf_ + out_offset_, base_hash, CHUNK_HASH_HMAC_SIZE);
        out_offset_ += CHUNK_HASH_HMAC_SIZE;

        // // debug: check baseHash
        // uint8_t check_base[CHUNK_HASH_HMAC_SIZE];
        // memcpy(check_base, base_hash, CHUNK_HASH_HMAC_SIZE);
        // SyncEnclave::Logging("encode check base2", "\n");
        // Ocall_PrintfBinary(check_base, CHUNK_HASH_HMAC_SIZE);
        // uint8_t check_sim_hash[CHUNK_HASH_HMAC_SIZE];
        // crypto_util_->GenerateHMAC( delta_chunk, delta_size, check_sim_hash);
        // Ocall_PrintfBinary(check_sim_hash, CHUNK_HASH_HMAC_SIZE);

        memcpy(plain_in_buf_ + out_offset_, delta_chunk, delta_size);
        out_offset_ += delta_size;
    }

    return ;
}

/**
 * @brief process a non-similar chunk
 * 
 * @param input_chunk 
 * @param input_size 
 */
void EcallStreamEncode::ProcessNonSimChunk(uint8_t* input_chunk, uint32_t input_size) {

    // SyncEnclave::Logging("in ProcessNonSim", "\n");

    // prepare the [chunkSize; chunkType]
    memcpy(plain_in_buf_ + out_offset_, &input_size, sizeof(uint32_t));
    out_offset_ += sizeof(uint32_t);
    uint8_t chunk_type = COMP_ONLY_CHUNK;
    memcpy(plain_in_buf_ + out_offset_, &chunk_type, sizeof(uint8_t));
    out_offset_ += sizeof(uint8_t);
    // get the [chunk data]

    // // debug
    // uint8_t dec_buf[MAX_CHUNK_SIZE];
    // crypto_util_->DecryptionWithKeyIV(cipher_ctx_, input_chunk, 
    //     input_size, SyncEnclave::enclave_key_,
    //     dec_buf, input_chunk + input_size);
    
    // SyncEnclave::Logging("dec1","\n");

    // decrypt with data key (will enc with session key later)
    crypto_util_->DecryptionWithKeyIV(cipher_ctx_, input_chunk, 
        input_size, SyncEnclave::enclave_key_,
        plain_in_buf_ + out_offset_, input_chunk + input_size);
    out_offset_ += input_size;


    // debug: check hash
    uint8_t check_dec_chunk[MAX_CHUNK_SIZE];
    crypto_util_->DecryptionWithKeyIV(cipher_ctx_, input_chunk, input_size,
        SyncEnclave::enclave_key_, check_dec_chunk, input_chunk + input_size);

    uint8_t check_origin_hash[CHUNK_HASH_HMAC_SIZE];
    uint8_t check_decomp_chunk[MAX_CHUNK_SIZE];
    int check_decomp_size = LZ4_decompress_safe((char*)check_dec_chunk, (char*)check_decomp_chunk,
        input_size, MAX_CHUNK_SIZE);
    if (check_decomp_size > 0) {
        // can be decompressed
        crypto_util_->GenerateHMAC(check_decomp_chunk, check_decomp_size, check_origin_hash);
        // Ocall_PrintfBinary(check_origin_hash, CHUNK_HASH_HMAC_SIZE);
    }
    else {
        // cannot be decompressed
        SyncEnclave::Logging("cannot decomp", "size = %d\n", input_size);
        crypto_util_->GenerateHMAC(check_dec_chunk, input_size, check_origin_hash);
        Ocall_PrintfBinary(check_origin_hash, CHUNK_HASH_HMAC_SIZE);
    }

    // check whether read chunk's hash exist in the global index
    OutChunkQueryEntry_t debug_query_entry;
    crypto_util_->IndexAESCMCEnc(cipher_ctx_, check_origin_hash,
        CHUNK_HASH_HMAC_SIZE, SyncEnclave::index_query_key_, debug_query_entry.chunkHash);
    Ocall_SingleQueryChunkIndex((void*)&debug_query_entry);

    // SyncEnclave::Logging("dec2","%d\n", out_offset_);

    return ;
}

/**
 * @brief perform delta encoding
 * 
 * @param base_chunk 
 * @param base_size 
 * @param input_chunk 
 * @param input_size 
 * @param delta_chunk 
 * @param delta_size 
 * @return true 
 * @return false 
 */
bool EcallStreamEncode::DeltaEncode(uint8_t* base_chunk, uint32_t base_size,
    uint8_t* input_chunk, uint32_t input_size, uint8_t* delta_chunk,
    uint32_t& delta_size, uint8_t& chunk_type) {
    // decrypt & decompress base and input chunk
    uint8_t plain_base[MAX_CHUNK_SIZE];
    uint8_t* base_iv = base_chunk + base_size;
    uint8_t origin_base[MAX_CHUNK_SIZE];
    int decompress_base_size = 0;

    uint8_t plain_similar[MAX_CHUNK_SIZE];
    uint8_t* sim_iv = input_chunk + input_size;
    uint8_t origin_similar[MAX_CHUNK_SIZE];
    int decompress_sim_size = 0;

    crypto_util_->DecryptionWithKeyIV(cipher_ctx_, base_chunk, base_size,
        SyncEnclave::enclave_key_, plain_base, base_iv);

    // debug
    // uint8_t check_plain_base_hash[CHUNK_HASH_HMAC_SIZE];
    // crypto_util_->GenerateHMAC( plain_base, base_size, check_plain_base_hash);
    // SyncEnclave::Logging("check plain base hash", "\n");
    // Ocall_PrintfBinary(check_plain_base_hash, CHUNK_HASH_HMAC_SIZE);

    decompress_base_size = LZ4_decompress_safe((char*)plain_base, (char*)origin_base,
        base_size, MAX_CHUNK_SIZE);
    if (decompress_base_size > 0) {
        // base can be decompressed
        // use origin_base for delta encoding
    }
    else {
        // base cannot be decompressed
        // use plain_base for delta encoding
        SyncEnclave::Logging("base x decomp", "\n");
        memcpy(origin_base, plain_base, base_size);
        decompress_base_size = base_size;
    }

    // // debug
    // uint8_t check_decomp_base_hash[CHUNK_HASH_HMAC_SIZE];
    // crypto_util_->GenerateHMAC( origin_base, decompress_base_size, check_decomp_base_hash);
    // SyncEnclave::Logging("check decomp base hash", "\n");
    // Ocall_PrintfBinary(check_decomp_base_hash, CHUNK_HASH_HMAC_SIZE);

    crypto_util_->DecryptionWithKeyIV(cipher_ctx_, input_chunk, input_size,
        SyncEnclave::enclave_key_, plain_similar, sim_iv);
    decompress_sim_size = LZ4_decompress_safe((char*)plain_similar, (char*)origin_similar,
        input_size, MAX_CHUNK_SIZE);

    if (decompress_sim_size > 0) {
        // similar chunk can be decompressed
        // use origin_similar for delta encoding
    }
    else {
        // similar chunk cannot be compressed
        // use plain_similar for delta encoding
        SyncEnclave::Logging("sim x decomp", "\n");
        memcpy(origin_similar, plain_similar, input_size);
        decompress_sim_size = input_size;
    }

    // // debug
    // uint8_t check_plain_sim_hash[CHUNK_HASH_HMAC_SIZE];
    // crypto_util_->GenerateHMAC( origin_similar, decompress_sim_size, check_plain_sim_hash);
    // SyncEnclave::Logging("check plain sim hash", "\n");
    // Ocall_PrintfBinary(check_plain_sim_hash, CHUNK_HASH_HMAC_SIZE);

    uint64_t ret_size = 0;
    uint8_t tmp_delta_chunk[MAX_CHUNK_SIZE];
    int ret = xd3_encode_memory(origin_similar, decompress_sim_size, origin_base, decompress_base_size,
        tmp_delta_chunk, &ret_size, MAX_CHUNK_SIZE, delta_flag_);
    
    if (ret != 0) {
        SyncEnclave::Logging(my_name_.c_str(), "delta encoding fails: %d.\n",
            ret);
        chunk_type = COMP_ONLY_CHUNK;
        return false;
    }

    // // debug
    // uint64_t dec_ret_size = 0;
    // uint8_t output_chunk[MAX_CHUNK_SIZE];
    // // int ret = xd3_decode_memory(delta_chunk, delta_size, base_chunk, base_size,
    // //     output_chunk, &ret_size, ENC_MAX_CHUNK_SIZE, delta_flag_);
    // int dec_ret = xd3_decode_memory(tmp_delta_chunk, ret_size, origin_base, decompress_base_size,
    //     output_chunk, &dec_ret_size, MAX_CHUNK_SIZE, delta_flag_);
    // if (dec_ret != 0) {
    //     SyncEnclave::Logging(my_name_.c_str(), "delta decoding fails: %d.\n",
    //         ret);
    //     // exit(EXIT_FAILURE);
    // }
    // else {
    //     // check hash
    //     uint8_t check_dec_hash[CHUNK_HASH_HMAC_SIZE];
    //     crypto_util_->GenerateHMAC( output_chunk, dec_ret_size, check_dec_hash);
    //     Ocall_PrintfBinary(check_dec_hash, CHUNK_HASH_HMAC_SIZE);
    // }

    // local compress the delta chunk
    int comp_delta_size = LZ4_compress_fast((char*)tmp_delta_chunk, (char*)delta_chunk,
        ret_size, ret_size, 3);
    if (comp_delta_size <= 0) {
        // cannot compress
        comp_delta_size = ret_size;
        memcpy(delta_chunk, tmp_delta_chunk, ret_size);
        chunk_type = DELTA_ONLY_CHUNK;
    }
    else {
        chunk_type = DELTA_COMP_CHUNK;
    }


    // SyncEnclave::Logging("size info ", "sim size = %d, base size = %d, delta size = %d, comp delta size = %d\n",
        // decompress_sim_size, decompress_base_size, ret_size, comp_delta_size);

    delta_size = comp_delta_size;

    _total_similar_chunk_size += decompress_sim_size;
    _total_base_chunk_size += decompress_base_size;
    _total_delta_size += ret_size;
    _total_comp_delta_size += comp_delta_size;

    // SyncEnclave::Logging("total size ", "total sim = %d, total base = %d, total delta = %d, total comp delta = %d\n", 
        // _total_similar_chunk_size, _total_base_chunk_size, _total_delta_size, _total_comp_delta_size);

    return true;
}
