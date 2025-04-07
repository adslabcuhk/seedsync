/**
 * @file stream_phase_4_thd.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-05-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/stream_phase_4_thd.h"

struct timeval stream4_stime;
struct timeval stream4_etime;

/**
 * @brief Construct a new Stream Phase 4 Thd:: Stream Phase 4 Thd object
 * 
 * @param phase_sender_obj 
 * @param inputMQ 
 * @param eid_sgx 
 */
StreamPhase4Thd::StreamPhase4Thd(PhaseSender* phase_sender_obj, MessageQueue<StreamPhase3MQ_t>* inputMQ,
    SyncIO* sync_io, LocalityCache* locality_cache) {
    
    // send_thd_obj_ = send_thd_obj;
    phase_sender_obj_ = phase_sender_obj;
    inputMQ_ = inputMQ;
    // sgx_eid_ = eid_sgx;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        sync_config.GetDataBatchSize() * sizeof(StreamPhase4MQ_t));
    send_batch_buf_.dataBuffer = send_batch_buf_.sendBuffer + sizeof(NetworkHead_t);
    send_batch_buf_.header = (NetworkHead_t*) send_batch_buf_.sendBuffer;
    send_batch_buf_.header->currentItemNum = 0;
    send_batch_buf_.header->dataSize = 0;

    sync_io_ = sync_io;
    enclave_locality_cache_ = locality_cache;

    plain_feature_fp_list_ = (uint8_t*)malloc(sync_config.GetDataBatchSize() * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)));
    plain_out_list_ = (uint8_t*) malloc(sync_config.GetDataBatchSize() * sizeof(StreamPhase4MQ_t));

    batch_feature_index_.reserve(sync_config.GetDataBatchSize()* SUPER_FEATURE_PER_CHUNK);

    tool::Logging(my_name_.c_str(), "init StreamPhase4Thd.\n");
}

/**
 * @brief Destroy the Stream Phase 4 Thd object
 * 
 */
StreamPhase4Thd::~StreamPhase4Thd() {
    free(send_batch_buf_.sendBuffer);
    
    free(plain_feature_fp_list_);
    free(plain_out_list_);
}

/**
 * @brief the main process
 * 
 */
void StreamPhase4Thd::Run() {
    bool job_done = false;
    StreamPhase3MQ_t tmp_entry;

    tool::Logging(my_name_.c_str(), "the main thread is running.\n");

    while (true) {
        
        if (inputMQ_->done_ && inputMQ_->IsEmpty()) {
            job_done = true;
        }

        if (inputMQ_->Pop(tmp_entry)) {
            if (tmp_entry.is_file_end == NOT_FILE_END) {
                // cout<<"pop features"<<endl;
                memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize, 
                    tmp_entry.features, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
                send_batch_buf_.header->dataSize += SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
                memcpy(send_batch_buf_.dataBuffer + send_batch_buf_.header->dataSize,
                    tmp_entry.chunkHash, CHUNK_HASH_HMAC_SIZE);
                send_batch_buf_.header->dataSize += CHUNK_HASH_HMAC_SIZE;
                send_batch_buf_.header->currentItemNum ++;

                if (send_batch_buf_.header->currentItemNum == sync_config.GetDataBatchSize()) {
                    ProcessOneBatch();
                    send_batch_buf_.header->currentItemNum = 0;
                    send_batch_buf_.header->dataSize = 0;
                }
                
            } 
            else if (tmp_entry.is_file_end == FILE_END) {
                // cout<<"file end phase-4"<<endl;

                if (send_batch_buf_.header->currentItemNum != 0) {
                    ProcessOneBatch();
                    send_batch_buf_.header->currentItemNum = 0;
                    send_batch_buf_.header->dataSize = 0;
                }

                // send the file end flag
                send_batch_buf_.header->messageType = FILE_END_BASE_HASH;
                // this->SendBatch(send_batch_buf_);
                phase_sender_obj_->SendBatch(&send_batch_buf_);

                cout<<"process time phase-4 "<<stream4_process_time_<<endl;

                // job_done = true;
            }
        }

        if (job_done) {
            break;
        }
    }

    // send the end flag
    send_batch_buf_.header->messageType = SYNC_BASE_HASH_END_FLAG;
    // this->SendBatch(send_batch_buf_);
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    tool::Logging(my_name_.c_str(), "StreamPhase4Thd exits.\n");

    return ;
}

/**
 * @brief process one batch of uni fp list, return the features
 * 
 */
void StreamPhase4Thd::ProcessOneBatch() {
    // cout<<"in phase4 process batch"<<endl;
    gettimeofday(&stream4_stime, NULL);
    // ecall
    uint32_t out_item_num = 0;
    // Ecall_Stream_Phase4_ProcessBatch(sgx_eid_, send_batch_buf_.dataBuffer, 
    //     send_batch_buf_.header->currentItemNum, 
    //     send_batch_buf_.dataBuffer, 
    //     out_item_num);
    ProcessPlainBatch(send_batch_buf_.dataBuffer, 
        send_batch_buf_.header->currentItemNum, 
        send_batch_buf_.dataBuffer, 
        out_item_num);

    // cout<<"after ecall (phase4)"<<endl;

    // tmp fix
    out_item_num = send_batch_buf_.header->currentItemNum;

    // prepare the send buf
    send_batch_buf_.header->messageType = SYNC_BASE_HASH;
    send_batch_buf_.header->dataSize = out_item_num * sizeof(StreamPhase4MQ_t);
    // this->SendBatch(send_batch_buf_);
    phase_sender_obj_->SendBatch(&send_batch_buf_);
    // reset
    send_batch_buf_.header->dataSize = 0;
    send_batch_buf_.header->currentItemNum = 0;

    // cout<<"after send (phase4)"<<endl;

    gettimeofday(&stream4_etime, NULL);
    stream4_process_time_ += tool::GetTimeDiff(stream4_stime, stream4_etime);

    // cout<<"after process batch (phase4)"<<endl;

    return ;
}

/**
 * @brief Set the Done Flag object
 * 
 */
void StreamPhase4Thd::SetDoneFlag() {
    inputMQ_->done_ = true;
    return ;
}

/**
 * @brief process plain batch
 * 
 * @param feature_fp_list 
 * @param feature_num 
 * @param out_list 
 * @param out_num 
 */
void StreamPhase4Thd::ProcessPlainBatch(uint8_t* feature_fp_list, size_t feature_num, 
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
bool StreamPhase4Thd::LocalDetect(uint64_t* features, uint8_t* chunkHash, 
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
bool StreamPhase4Thd::QueryLocalFeature(uint64_t feature, string& baseHash) {
    if (batch_feature_index_.find(feature) != batch_feature_index_.end()) {
        // baseHash.assign(batch_feature_index_[feature], CHUNK_HASH_HMAC_SIZE);
        baseHash.assign(batch_feature_index_[feature].c_str(), CHUNK_HASH_HMAC_SIZE);
        return true;
    }

    return false;
}

/**
 * @brief insert the batch into send MQ
 * 
 * @param send_buf 
 */
// void StreamPhase4Thd::SendBatch(SendMsgBuffer_t& send_buf) {
//     uint8_t flag = META_POOL;
//     while (true) {
//         if (send_thd_obj_->InsertSendMQ(flag, send_buf)) {
//             break;
//         }
//     }

//     return ;
// }