/**
 * @file syncOCall.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief
 * @version 0.1
 * @date 2024-01-18
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../include/syncOcall.h"

namespace SyncOutEnclave {
AbsDatabase* out_chunk_index_ = NULL;
AbsDatabase* out_feature_index_ = NULL;
SyncStorage* sync_storage_ = NULL;

string my_name_ = "SyncOcall";

// // for sealing
// ofstream out_sealed_file_hdl_;
// ifstream in_sealed_file_hdl_;

ofstream outSealedFile_;
ifstream inSealedFile_;

// for lck
// pthread_rwlock_t seg_index_lck_;
pthread_rwlock_t chunk_index_lck_;
pthread_rwlock_t feature_index_lck_;
pthread_mutex_t sync_storage_lck_;
};

using namespace SyncOutEnclave;

/**
 * @brief init the sync ocall var
 *
 * @param out_chunk_index
 * @param out_feature_index
 */
void SyncOutEnclave::Init(AbsDatabase* out_chunk_index, AbsDatabase* out_feature_index,
    SyncStorage* sync_storage) {
    out_chunk_index_ = out_chunk_index;
    out_feature_index_ = out_feature_index;
    sync_storage_ = sync_storage;
    pthread_rwlock_init(&chunk_index_lck_, NULL);
    pthread_rwlock_init(&feature_index_lck_, NULL);
    pthread_mutex_init(&sync_storage_lck_, NULL);

    return;
}

/**
 * @brief destroy the sync ocall var
 *
 */
void SyncOutEnclave::Destroy() {
    pthread_rwlock_destroy(&chunk_index_lck_);
    pthread_rwlock_destroy(&feature_index_lck_);
    pthread_mutex_destroy(&sync_storage_lck_);

    return;
}

/**
 * @brief exit the enclave with error message
 *
 * @param error_msg the error message
 */
void Ocall_SGX_Exit_Error(const char* error_msg) {
    // tool::Logging(my_name_.c_str(), "%s\n", error_msg);
    exit(EXIT_FAILURE);
}

/**
 * @brief Print the content of the buffer
 *
 * @param buffer the input buffer
 * @param len the length in byte
 */
void Ocall_PrintfBinary(const uint8_t* buffer, size_t len) {
    fprintf(stderr, "**Enclave**: ");
    for (size_t i = 0; i < len; i++) {
        fprintf(stderr, "%02x", buffer[i]);
    }
    fprintf(stderr, "\n");
    return;
}

/**
 * @brief printf interface for Ocall
 *
 * @param str input string
 */
void Ocall_Printf(const char* str) {
    fprintf(stderr, "**Enclave**: %s", str);
}

/**
 * @brief get current time from the outside
 *
 * @return long current time (usec)
 */
void Ocall_GetCurrentTime(uint64_t* retTime) {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    *retTime = currentTime.tv_sec * SEC_2_US + currentTime.tv_usec;
    return;
}

/**
 * @brief init the file output stream
 *
 * @param ret the return result
 * @param sealedFileName the sealed file name
 */
void Ocall_InitWriteSealedFile(bool* ret, const char* sealedFileName) {
    if (outSealedFile_.is_open()) {
        // tool::Logging(my_name_.c_str(), "sealed file is already opened: %s\n", sealedFileName);
        *ret = false;
        return;
    }

    outSealedFile_.open(sealedFileName, ios_base::out | ios_base::binary);
    if (!outSealedFile_.is_open()) {
        // tool::Logging(my_name_.c_str(), "cannot open the sealed file.\n");
        *ret = false;
        return;
    }
    *ret = true;
    return;
}

/**
 * @brief write the data to the disk file
 *
 * @param sealedFileName the name of the sealed file
 * @param sealedDataBuffer sealed data buffer
 * @param sealedDataSize sealed data size
 * @return true success
 * @return false fail
 */
void Ocall_WriteSealedData(const char* sealedFileName, uint8_t* sealedDataBuffer, size_t sealedDataSize) {
    outSealedFile_.write((char*)sealedDataBuffer, sealedDataSize);
    return;
}

/**
 * @brief close the file output stream
 *
 * @param sealedFileName the sealed file name
 */
void Ocall_CloseWriteSealedFile(const char* sealedFileName) {
    if (outSealedFile_.is_open()) {
        outSealedFile_.close();
        outSealedFile_.clear();
    }
    return;
}

/**
 * @brief Init the unseal file stream
 *
 * @param fileSize the file size
 * @param sealedFileName the sealed file name
 * @return uint32_t the file size
 */
void Ocall_InitReadSealedFile(size_t* fileSize, const char* sealedFileName) {
    // return OutEnclave::ocallHandlerObj_->InitReadSealedFile(sealedFileName);
    string fileName(sealedFileName);
    // tool::Logging(my_name_.c_str(), "print the file name: %s\n", fileName.c_str());
    inSealedFile_.open(fileName, ios_base::binary);

    if (!inSealedFile_.is_open()) {
        // tool::Logging(my_name_.c_str(), "sealed file does not exist.\n");
        *fileSize = 0;
        return;
    }

    size_t beginSize = inSealedFile_.tellg();
    inSealedFile_.seekg(0, ios_base::end);
    *fileSize = inSealedFile_.tellg();
    *fileSize = *fileSize - beginSize;

    // reset
    inSealedFile_.clear();
    inSealedFile_.seekg(0, ios_base::beg);

    return;
}

/**
 * @brief close the file input stream
 *
 * @param sealedFileName the sealed file name
 */
void Ocall_CloseReadSealedFile(const char* sealedFileName) {
    if (inSealedFile_.is_open()) {
        inSealedFile_.close();
    }
    return;
}

/**
 * @brief read the sealed data from the file
 *
 * @param sealedFileName the sealed file
 * @param dataBuffer the data buffer
 * @param sealedDataSize the size of sealed data
 */
void Ocall_ReadSealedData(const char* sealedFileName, uint8_t* dataBuffer,
    uint32_t sealedDataSize) {
    inSealedFile_.read((char*)dataBuffer, sealedDataSize);
    return;
}

/**
 * @brief generate the UUID
 *
 * @param id the uuid buffer
 * @param len the id len
 */
void Ocall_CreateUUID(uint8_t* id, size_t len) {
    tool::CreateUUID((char*)id, len);
    return;
}

/**
 * @brief query the out-enclave chunk index
 *
 * @param out_chunk_query
 */
void Ocall_QueryChunkIndex(void* out_chunk_query) {
    // add rw lock
    pthread_rwlock_rdlock(&chunk_index_lck_);

    OutChunkQuery_t* out_query_ptr = (OutChunkQuery_t*)out_chunk_query;
    OutChunkQueryEntry_t* tmp_entry = out_query_ptr->OutChunkQueryBase;
    bool res = false;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    // cout<<"ocall chunk query num "<<out_query_ptr->queryNum<<endl;
    for (size_t i = 0; i < out_query_ptr->queryNum; i++) {
        res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_SIZE, value);
        if (res) {
            // cout<<"exist in chunk index"<<endl;
            tmp_entry->dedupFlag = DUPLICATE;
            memcpy(&tmp_entry->value, &value[0], sizeof(RecipeEntry_t));
            // tool::Logging("OCall query", "Query chunk in the query batch, container name: %s\n", (char*)tmp_entry->value.containerName);
        } else {
            tmp_entry->dedupFlag = UNIQUE;
            // cout<<"unique in chunk index"<<endl;
            // tool::Logging("OCall query", "Unique chunk in the query batch\n");
        }

        tmp_entry++;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    return;
}

/**
 * @brief query the out-enclave chunk index
 *
 * @param out_chunk_query
 */
void Ocall_QueryChunkAddr(void* out_chunk_query) {
    pthread_rwlock_rdlock(&chunk_index_lck_);

    OutChunkQuery_t* out_query_ptr = (OutChunkQuery_t*)out_chunk_query;
    OutChunkQueryEntry_t* tmp_entry = out_query_ptr->OutChunkQueryBase;
    bool res = false;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    // cout<<"ocall chunk query num "<<out_query_ptr->queryNum<<endl;
    for (size_t i = 0; i < out_query_ptr->queryNum; i++) {
        res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_SIZE, value);
        if (res) {
            // cout<<"chunk exist"<<endl;
            memcpy(&tmp_entry->value, &value[0], sizeof(RecipeEntry_t));

            tmp_entry->dedupFlag = 100;

        } else {
            // cout << "cannot get the chunk addr" << endl;

            tmp_entry->dedupFlag = 101;

#if (DEBUG_FLAG == 1)            
            cout << "cannot get the chunk addr" << endl;
            // // tool::Logging("chunk type ", "%d, %d-th in the quary batch\n", tmp_entry->dedupFlag, i);         
            // debug
            uint8_t check_hash[CHUNK_HASH_SIZE];
            memcpy(check_hash, tmp_entry->chunkHash, CHUNK_HASH_SIZE);
            tool::PrintBinaryArray(check_hash, CHUNK_HASH_SIZE);
            // check current entry: should be null as not valid
            // tool::Logging("check the entry ", "offset=%d; len=%d; type=%d; %d-th\n", tmp_entry->value.offset, tmp_entry->value.length, tmp_entry->dedupFlag, i);
            tool::PrintBinaryArray((uint8_t*)tmp_entry->value.containerName, CONTAINER_ID_LENGTH);
#endif            
        }

        tmp_entry++;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    return;
}


void Ocall_QueryEncodeChunkAddr(void* out_chunk_query) {
    pthread_rwlock_rdlock(&chunk_index_lck_);

    OutChunkQuery_t* out_query_ptr = (OutChunkQuery_t*)out_chunk_query;
    OutChunkQueryEntry_t* tmp_entry = out_query_ptr->OutChunkQueryBase;
    bool res = false;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    // cout<<"ocall chunk query num "<<out_query_ptr->queryNum<<endl;
    for (size_t i = 0; i < out_query_ptr->queryNum; i++) {
        res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_SIZE, value);
        if (res) {
            // cout<<"chunk exist"<<endl;
            memcpy(&tmp_entry->value, &value[0], sizeof(RecipeEntry_t));
            tmp_entry->existFlag = 100;
        } else {
            tmp_entry->existFlag = 101;
            cout<<"chunk not exist"<<endl;
#if (DEBUG_FLAG == 1)            
            cout << "cannot get the chunk addr" << endl;
            // // tool::Logging("chunk type ", "%d, %d-th in the quary batch\n", tmp_entry->dedupFlag, i);         
            // debug
            uint8_t check_hash[CHUNK_HASH_SIZE];
            memcpy(check_hash, tmp_entry->chunkHash, CHUNK_HASH_SIZE);
            tool::PrintBinaryArray(check_hash, CHUNK_HASH_SIZE);
            // check current entry: should be null as not valid
            // tool::Logging("check the entry ", "offset=%d; len=%d; type=%d; %d-th\n", tmp_entry->value.offset, tmp_entry->value.length, tmp_entry->dedupFlag, i);
            tool::PrintBinaryArray((uint8_t*)tmp_entry->value.containerName, CONTAINER_ID_LENGTH);
#endif            
        }

        tmp_entry++;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    return;
}

/**
 * @brief issue a single query to out-enclave chunk index
 *
 * @param query_entry
 */
void Ocall_SingleQueryChunkIndex(void* query_entry) {
    pthread_rwlock_rdlock(&chunk_index_lck_);

    OutChunkQueryEntry_t* tmp_entry = (OutChunkQueryEntry_t*)query_entry;
    bool res = false;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    // cout<<" chunk query"<<endl;
    res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
        CHUNK_HASH_SIZE, value);
    if (res) {
        // cout<<"exist in chunk index"<<endl;
        // tool::PrintBinaryArray(tmp_entry->chunkHash, CHUNK_HASH_SIZE);
        tmp_entry->dedupFlag = DUPLICATE;
        memcpy(&tmp_entry->value, &value[0], sizeof(RecipeEntry_t));
    } else {
        tmp_entry->dedupFlag = UNIQUE;
        // cout<<"unique in chunk index"<<endl;
        // tool::PrintBinaryArray(tmp_entry->chunkHash, CHUNK_HASH_SIZE);
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    return;
}

bool CmpPair2(pair<string, uint32_t>& a, pair<string, uint32_t>& b) {
    return a.second > b.second;
}

/**
 * @brief query the out-enclave feature index
 *
 * @param out_feature_query
 */
void Ocall_QueryFeatureIndex(void* out_feature_query) {
    OutFeatureQuery_t* out_query_ptr = (OutFeatureQuery_t*)out_feature_query;
    OutFeatureQueryEntry_t* tmp_entry = out_query_ptr->OutFeatureQueryBase;
    bool res = false;
    string base_hash;
    base_hash.resize(CHUNK_HASH_SIZE, 0);
    cout << "ocall feature query num " << out_query_ptr->queryNum << endl;

    for (size_t i = 0; i < out_query_ptr->queryNum; i++) {

        if (tmp_entry->dedupFlag == ENCLAVE_SIMILAR_CHUNK) {
            continue;
        }

        // find base chunk
        {
            bool is_similar = false;
            // uint8_t tmp_base_hash[CHUNK_HASH_SIZE];
            string tmp_base_hash;
            bool is_first_match = true;
            uint8_t first_match_base_hash[CHUNK_HASH_SIZE];
            unordered_map<string, uint32_t> base_freq_map;

            for (size_t i = 0; i < SUPER_FEATURE_PER_CHUNK; i++) {

                // if (this->Query(tmp_entry->features[i], tmp_base_hash)) {
                if (out_feature_index_->QueryBuffer((char*)&tmp_entry->features[i],
                        sizeof(uint64_t), tmp_base_hash)) {
                    if (is_first_match) {

                        memcpy(first_match_base_hash, &tmp_base_hash[0], CHUNK_HASH_SIZE);
                        is_first_match = false;
                    }

                    if (base_freq_map.find(tmp_base_hash) != base_freq_map.end()) {
                        base_freq_map[tmp_base_hash]++;
                    } else {
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
                sort(tmp_freq_vec.begin(), tmp_freq_vec.end(), CmpPair2);
                if (tmp_freq_vec[0].second == 1) {
                    memcpy(tmp_entry->baseHash, first_match_base_hash, CHUNK_HASH_SIZE);
                } else {
                    memcpy(tmp_entry->baseHash, tmp_freq_vec[0].first.c_str(),
                        CHUNK_HASH_SIZE);
                }

                tmp_entry->dedupFlag = SIMILAR_CHUNK;
            } else {
                tmp_entry->dedupFlag = NON_SIMILAR_CHUNK;
                // insert the feature index
                for (size_t i = 0; i < SUPER_FEATURE_PER_CHUNK; i++) {
                    out_feature_index_->InsertBothBuffer((char*)&tmp_entry->features[i],
                        sizeof(uint64_t), (char*)tmp_entry->baseHash, CHUNK_HASH_SIZE);
                }
            }

            base_freq_map.clear();
        }
        // move on
        tmp_entry++;
    }

    return;
}

/**
 * @brief fetch the containers into enclave
 *
 * @param req_container
 */
void Ocall_SyncGetReqContainer(void* req_container) {
    pthread_mutex_lock(&sync_storage_lck_);
    ReqContainer_t* req_container_ptr = (ReqContainer_t*)req_container;
    sync_storage_->GetReqContainers(req_container_ptr);
    pthread_mutex_unlock(&sync_storage_lck_);

    return;
}

/**
 * @brief fetch the containers with sizes into enclave
 *
 * @param req_container
 */
void Ocall_SyncGetReqContainerWithSize(void* req_container) {
    pthread_mutex_lock(&sync_storage_lck_);
    ReqContainer_t* req_container_ptr = (ReqContainer_t*)req_container;
    sync_storage_->GetReqContainersWithSize(req_container_ptr);
    pthread_mutex_unlock(&sync_storage_lck_);

    return;
}

/**
 * @brief fetch a single container into enclave
 *
 * @param req_container
 */
void Ocall_SingleGetReqContainer(void* req_container) {
    pthread_mutex_lock(&sync_storage_lck_);
    Container_t* req_container_ptr = (Container_t*)req_container;
    sync_storage_->GetContainer(req_container_ptr);

    // cout<<"req size from ocall"<<req_container_ptr->currentSize<<" "<<req_container_ptr->currentMetaSize<<endl;
    pthread_mutex_unlock(&sync_storage_lck_);

    return;
}

/**
 * @brief read the chunk batch
 *
 * @param chunk_data
 * @param chunk_num
 */
void Ocall_ReadChunkBatch(uint8_t* chunk_data, uint32_t chunk_num) {
    while (true) {
        if (sync_storage_->ReadChunkBatch(chunk_data, chunk_num)) {
            break;
        }
    }

    return;
}

/**
 * @brief write the chunk batch
 *
 * @param chunk_data
 * @param chunk_num
 */
void Ocall_WriteChunkBatch(uint8_t* chunk_data, uint32_t chunk_num) {
    while (true) {
        if (sync_storage_->WriteChunkBatch(chunk_data, chunk_num)) {
            break;
        }
    }

    return;
}

/**
 * @brief write the sync container
 *
 * @param sync_container
 */
void Ocall_WriteSyncContainer(Container_t* newContainer) {
    pthread_mutex_lock(&sync_storage_lck_);

    FILE* containerFile = NULL;
    string fileName((char*)newContainer->containerID, CONTAINER_ID_LENGTH);
    string fileFullName = config.GetContainerRootPath() + fileName
        + config.GetContainerSuffix();
    containerFile = fopen(fileFullName.c_str(), "wb");
    if (!containerFile) {
        tool::Logging(my_name_.c_str(), "cannot open container file: %s\n", fileFullName.c_str());
        exit(EXIT_FAILURE);
    }
    // write the metadata session
    // write the metadata session size
#if (DEBUG_FLAG == 1)     
    cout << "save container file: metasize = " << newContainer->currentMetaSize << ", datasize = " << newContainer->currentSize << endl;
    // uint8_t check_id[CONTAINER_ID_LENGTH];
    // memcpy(check_id, newContainer->containerID, CONTAINER_ID_LENGTH);
    // tool::PrintBinaryArray(check_id, CONTAINER_ID_LENGTH);
    // // debug
    // size_t num = newContainer->currentMetaSize / (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
    // for (size_t i = 0; i < num; i++) {
    //     uint64_t check_feature[SUPER_FEATURE_PER_CHUNK];
    //     memcpy(check_feature, newContainer->metadata + i * (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)), SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
    //     cout<<"feature: ";
    //     for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
    //         // tool::Logging(my_name_.c_str(), "%d\t", check_feature[j]);
    //     }
    //     cout<<endl;
    // }
#endif
    fwrite((char*)&newContainer->currentMetaSize, sizeof(uint32_t), 1, containerFile);
    fwrite((char*)newContainer->metadata, newContainer->currentMetaSize, 1,
        containerFile);
    // write the data
    fwrite((char*)newContainer->body, newContainer->currentSize, 1,
        containerFile);
    fclose(containerFile);

    // reset the current container
    tool::CreateUUID(newContainer->containerID, CONTAINER_ID_LENGTH);
    newContainer->currentSize = 0;
    newContainer->currentMetaSize = 0;

    pthread_mutex_unlock(&sync_storage_lck_);
    return;
}

/**
 * @brief update the outside fp index
 *
 * @param update_index
 */
void Ocall_UpdateOutFPIndex(void* update_index)
{
    pthread_rwlock_wrlock(&chunk_index_lck_);

    OutChunkQuery_t* update_index_ptr = (OutChunkQuery_t*)update_index;
    OutChunkQueryEntry_t* tmp_entry = update_index_ptr->OutChunkQueryBase;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    bool res = false;
    for (size_t i = 0; i < update_index_ptr->queryNum; i++) {
        res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_SIZE, value);

        if (!res) {
#if (DEBUG_FLAG == 1)
            // // debug
            // uint8_t check_insert_hash[CHUNK_HASH_SIZE];
            // memcpy(check_insert_hash, tmp_entry->chunkHash, CHUNK_HASH_SIZE);
            // // // tool::Logging("insert chunk hash for a batch ", "\n");
            // tool::PrintBinaryArray(check_insert_hash, CHUNK_HASH_SIZE);
            // uint8_t check_container_name[CONTAINER_ID_LENGTH];
            // memcpy(check_container_name, tmp_entry->value.containerName, CONTAINER_ID_LENGTH);
            // tool::PrintBinaryArray(check_container_name, CONTAINER_ID_LENGTH);
#endif

            // // debug
            // uint8_t check_insert_hash[CHUNK_HASH_SIZE];
            // memcpy(check_insert_hash, tmp_entry->chunkHash, CHUNK_HASH_SIZE);
            // // // tool::Logging("insert chunk hash for a batch ", "\n");
            // tool::PrintBinaryArray(check_insert_hash, CHUNK_HASH_SIZE);
            // uint8_t check_container_name[CONTAINER_ID_LENGTH];
            // memcpy(check_container_name, tmp_entry->value.containerName, CONTAINER_ID_LENGTH);
            // tool::PrintBinaryArray(check_container_name, CONTAINER_ID_LENGTH);
            
            out_chunk_index_->InsertBothBuffer((char*)tmp_entry->chunkHash,
                CHUNK_HASH_SIZE, (char*)&tmp_entry->value, sizeof(RecipeEntry_t));

#if (DEBUG_FLAG == 1)
            // // debug
            // string check_value;
            // bool check_insert = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            //     CHUNK_HASH_SIZE, check_value);
            // if (check_insert) {
            //     // tool::Logging("insert fine.", "\n");
            // }
#endif            
            // // debug
            // string check_value;
            // bool check_insert = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            //     CHUNK_HASH_SIZE, check_value);
            // if (check_insert) {
            //     tool::Logging("insert fine.", "\n");
            // }
        }

        tmp_entry++;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    // reset the update index
    update_index_ptr->queryNum = 0;

    return;
}

// for debugging
void Ocall_InsertDebugIndex(void* debug_index) {
    pthread_rwlock_wrlock(&feature_index_lck_);

    OutChunkQuery_t* debug_index_ptr = (OutChunkQuery_t*)debug_index;
    OutChunkQueryEntry_t* tmp_entry = debug_index_ptr->OutChunkQueryBase;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    bool res = false;
    // // tool::Logging("debug index entry", "%d\n", debug_index_ptr->queryNum);
    for (size_t i = 0; i < debug_index_ptr->queryNum; i++) {
        res = out_feature_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_SIZE, value);

        if (!res) {
            out_feature_index_->InsertBothBuffer((char*)tmp_entry->chunkHash,
                CHUNK_HASH_SIZE, (char*)&tmp_entry->value, sizeof(RecipeEntry_t));
        } else {
            // // tool::Logging("exist in debug index: duplicate", "\n");
        }

        tmp_entry++;
    }

    pthread_rwlock_unlock(&feature_index_lck_);

    debug_index_ptr->queryNum = 0;

    return;
}

void Ocall_CheckDebugIndex(void* debug_index) {
    pthread_rwlock_rdlock(&feature_index_lck_);

    OutChunkQuery_t* debug_index_ptr = (OutChunkQuery_t*)debug_index;
    OutChunkQueryEntry_t* tmp_entry = debug_index_ptr->OutChunkQueryBase;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    bool res = false;
    // // tool::Logging("debug check entry", "%d\n", debug_index_ptr->queryNum);

    for (size_t i = 0; i < debug_index_ptr->queryNum; i++) {
        res = out_feature_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_SIZE, value);

        if (res) {
            // // tool::Logging("exist in debug index: duplicate", "\n");
        } else {
            // pass the chunk type to duplicate flag for debugging
            tool::Logging("not exist in debug index: recover error", "%d-th, type=%d\n", i, tmp_entry->dedupFlag);
            // tool::PrintBinaryArray(tmp_entry->chunkHash, CHUNK_HASH_SIZE);
        }

        tmp_entry++;
    }

    pthread_rwlock_unlock(&feature_index_lck_);

    debug_index_ptr->queryNum = 0;

    return;
}
