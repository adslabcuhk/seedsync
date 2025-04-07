/**
 * @file sync_io.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-10-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/sync_io.h"


/**
 * @brief init the sync io
 * 
 * @param out_chunk_index 
 * @param out_feature_index 
 */
SyncIO::SyncIO(AbsDatabase* out_chunk_index, AbsDatabase* debug_index,
    SyncStorage* sync_storage) {

    out_chunk_index_ = out_chunk_index;
    debug_index_ = debug_index;
    sync_storage_ = sync_storage;
    pthread_rwlock_init(&chunk_index_lck_, NULL);
    pthread_rwlock_init(&debug_index_lck_, NULL);
    pthread_mutex_init(&sync_storage_lck_, NULL);
}

/**
 * @brief destroy the sync io
 * 
 */
SyncIO::~SyncIO() {
    pthread_rwlock_destroy(&chunk_index_lck_);
    pthread_rwlock_destroy(&debug_index_lck_);
    pthread_mutex_destroy(&sync_storage_lck_);
}

/**
 * @brief query the chunk index
 * 
 * @param out_chunk_query 
 */
void SyncIO::SyncIO_QueryChunkIndex(void* out_chunk_query) {
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
            CHUNK_HASH_HMAC_SIZE, value);
        if (res) {
            // cout<<"exist in chunk index"<<endl;
            tmp_entry->dedupFlag = DUPLICATE;
            memcpy(&tmp_entry->value, &value[0], sizeof(RecipeEntry_t));
        }
        else {
            tmp_entry->dedupFlag = UNIQUE;
            // cout<<"unique in chunk index"<<endl;
        }

        tmp_entry ++;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    return ;
}

/**
 * @brief query the chunk addr
 * 
 * @param out_chunk_query 
 */
void SyncIO::SyncIO_QueryChunkAddr(void* out_chunk_query) {
    pthread_rwlock_rdlock(&chunk_index_lck_);

    OutChunkQuery_t* out_query_ptr = (OutChunkQuery_t*)out_chunk_query;
    OutChunkQueryEntry_t* tmp_entry = out_query_ptr->OutChunkQueryBase;
    bool res = false;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    // cout<<"ocall chunk query num "<<out_query_ptr->queryNum<<endl;
    for (size_t i = 0; i < out_query_ptr->queryNum; i++) {
        res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_HMAC_SIZE, value);
        if (res) {
            // cout<<"chunk exist"<<endl;
            memcpy(&tmp_entry->value, &value[0], sizeof(RecipeEntry_t));

            tmp_entry->existFlag = 100;
        }
        else {
            cout<<"cannot get the chunk addr"<<endl;

            tmp_entry->existFlag = 101;
            // tool::Logging("chunk type ", "%d, %d-th in the quary batch\n", tmp_entry->dedupFlag, i);
            // debug
            // uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
            // memcpy(check_hash, tmp_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);
            // tool::PrintBinaryArray(check_hash, CHUNK_HASH_HMAC_SIZE);
            // // check current entry: should be null as not valid
            // tool::Logging("check the entry ", "offset=%d; len=%d; type=%d; %d-th\n", tmp_entry->value.offset, tmp_entry->value.length, tmp_entry->dedupFlag, i);
            // tool::PrintBinaryArray((uint8_t*)tmp_entry->value.containerName, CONTAINER_ID_LENGTH);
        }

        tmp_entry ++;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    return ;    
}

void SyncIO::SyncIO_QueryEncodeChunkAddr(void* out_chunk_query) {
    pthread_rwlock_rdlock(&chunk_index_lck_);

    OutChunkQuery_t* out_query_ptr = (OutChunkQuery_t*)out_chunk_query;
    OutChunkQueryEntry_t* tmp_entry = out_query_ptr->OutChunkQueryBase;
    bool res = false;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    // cout<<"ocall chunk query num "<<out_query_ptr->queryNum<<endl;
    for (size_t i = 0; i < out_query_ptr->queryNum; i++) {
        res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_HMAC_SIZE, value);
        if (res) {
            // cout<<"chunk exist"<<endl;
            memcpy(&tmp_entry->value, &value[0], sizeof(RecipeEntry_t));
        }
        else {
            cout<<"cannot get the chunk addr"<<endl;
            // tool::Logging("chunk type ", "%d, %d-th in the quary batch\n", tmp_entry->dedupFlag, i);
            // debug
            uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
            memcpy(check_hash, tmp_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);
            tool::PrintBinaryArray(check_hash, CHUNK_HASH_HMAC_SIZE);
            // check current entry: should be null as not valid
            tool::Logging("check the entry ", "offset=%d; len=%d; type=%d; %d-th\n", tmp_entry->value.offset, tmp_entry->value.length, tmp_entry->dedupFlag, i);
            tool::PrintBinaryArray((uint8_t*)tmp_entry->value.containerName, CONTAINER_ID_LENGTH);
        }

        tmp_entry ++;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    return ; 
}


/**
 * @brief issue a single query to out-enclave chunk index
 * 
 * @param query_entry 
 */
void SyncIO::SyncIO_SingleQueryChunkIndex(void* query_entry) {
    pthread_rwlock_rdlock(&chunk_index_lck_);

    OutChunkQueryEntry_t* tmp_entry = (OutChunkQueryEntry_t*)query_entry;
    bool res = false;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    // cout<<" chunk query"<<endl;
    res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
        CHUNK_HASH_HMAC_SIZE, value);
    if (res) {
        // cout<<"exist in chunk index"<<endl;
        tmp_entry->dedupFlag = DUPLICATE;
        memcpy(&tmp_entry->value, &value[0], sizeof(RecipeEntry_t));
    }
    else {
        tmp_entry->dedupFlag = UNIQUE;
        // cout<<"unique in chunk index"<<endl;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    return ;
}

/**
 * @brief fetch container
 * 
 * @param req_container 
 */
void SyncIO::SyncIO_SyncGetReqContainer(void* req_container) {
    pthread_mutex_lock(&sync_storage_lck_);
    ReqContainer_t* req_container_ptr = (ReqContainer_t*)req_container;
    sync_storage_->GetReqContainers(req_container_ptr);
    pthread_mutex_unlock(&sync_storage_lck_);

    return ;
}

/**
 * @brief fetch the containers with sizes 
 * 
 * @param req_container 
 */
void SyncIO::SyncIO_SyncGetReqContainerWithSize(void* req_container) {
    pthread_mutex_lock(&sync_storage_lck_);
    ReqContainer_t* req_container_ptr = (ReqContainer_t*)req_container;
    sync_storage_->GetReqContainersWithSize(req_container_ptr);
    pthread_mutex_unlock(&sync_storage_lck_);

    return ;
}

/**
 * @brief fetch a single container
 * 
 * @param req_container 
 */
void SyncIO::SyncIO_SingleGetReqContainer(void* req_container) {
    pthread_mutex_lock(&sync_storage_lck_);
    Container_t* req_container_ptr = (Container_t*)req_container;
    sync_storage_->GetContainer(req_container_ptr);

    // cout<<"req size from ocall"<<req_container_ptr->currentSize<<" "<<req_container_ptr->currentMetaSize<<endl;
    pthread_mutex_unlock(&sync_storage_lck_);

    return ;
}

/**
 * @brief write the sync container
 * 
 * @param sync_container 
 */
void SyncIO::SyncIO_WriteSyncContainer(Container_t* newContainer) {
// #if (CONTAINER_META_SESSION == 1)
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
    cout<<"save container file: metasize = "<<newContainer->currentMetaSize<<", datasize = "<<newContainer->currentSize<<endl;
    // uint8_t check_id[CONTAINER_ID_LENGTH];
    // memcpy(check_id, newContainer->containerID, CONTAINER_ID_LENGTH);
    // tool::PrintBinaryArray(check_id, CONTAINER_ID_LENGTH);
    // // debug
    // size_t num = newContainer->currentMetaSize / (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
    // for (size_t i = 0; i < num; i++) {
    //     uint64_t check_feature[SUPER_FEATURE_PER_CHUNK];
    //     memcpy(check_feature, newContainer->metadata + i * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)), SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
    //     cout<<"feature: ";
    //     for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
    //         tool::Logging(my_name_.c_str(), "%d\t", check_feature[j]);
    //     }
    //     cout<<endl;
    // }

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
// #endif
    return ;
}


/**
 * @brief update the outside fp index
 * 
 * @param update_index 
 */
void SyncIO::SyncIO_UpdateOutFPIndex(void* update_index) {
    pthread_rwlock_wrlock(&chunk_index_lck_);

    OutChunkQuery_t* update_index_ptr = (OutChunkQuery_t*) update_index;
    OutChunkQueryEntry_t* tmp_entry = update_index_ptr->OutChunkQueryBase;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    bool res = false;
    for (size_t i = 0; i < update_index_ptr->queryNum; i++) {
        res = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_HMAC_SIZE, value);

        if (!res) {
            // // debug
            // uint8_t check_insert_hash[CHUNK_HASH_HMAC_SIZE];
            // memcpy(check_insert_hash, tmp_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);
            // // tool::Logging("insert chunk hash for a batch ", "\n");
            // tool::PrintBinaryArray(check_insert_hash, CHUNK_HASH_HMAC_SIZE);
            // uint8_t check_container_name[CONTAINER_ID_LENGTH];
            // memcpy(check_container_name, tmp_entry->value.containerName, CONTAINER_ID_LENGTH);
            // tool::PrintBinaryArray(check_container_name, CONTAINER_ID_LENGTH);

            out_chunk_index_->InsertBothBuffer((char*)tmp_entry->chunkHash,
                CHUNK_HASH_HMAC_SIZE, (char*)&tmp_entry->value, sizeof(RecipeEntry_t));

            // // debug
            // string check_value;
            // bool check_insert = out_chunk_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            //     CHUNK_HASH_HMAC_SIZE, check_value);
            // if (check_insert) {
            //     tool::Logging("insert fine.", "\n");
            // }
        }

        tmp_entry ++;
    }

    pthread_rwlock_unlock(&chunk_index_lck_);

    // reset the update index
    update_index_ptr->queryNum = 0;

    return ;
}

// for debugging
void SyncIO::SyncIO_InsertDebugIndex(void* debug_index) {
    pthread_rwlock_wrlock(&debug_index_lck_);
    
    OutChunkQuery_t* debug_index_ptr = (OutChunkQuery_t*) debug_index;
    OutChunkQueryEntry_t* tmp_entry = debug_index_ptr->OutChunkQueryBase;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    bool res = false;
    // tool::Logging("debug index entry", "%d\n", debug_index_ptr->queryNum);
    for (size_t i = 0; i < debug_index_ptr->queryNum; i++) {
        res = debug_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_HMAC_SIZE, value);

        if (!res) {
            debug_index_->InsertBothBuffer((char*)tmp_entry->chunkHash,
                CHUNK_HASH_HMAC_SIZE, (char*)&tmp_entry->value, sizeof(RecipeEntry_t));
        }
        else {
            // tool::Logging("exist in debug index: duplicate", "\n");
        }

        tmp_entry ++;
    }

    pthread_rwlock_unlock(&debug_index_lck_);

    debug_index_ptr->queryNum = 0;

    return ;
}

void SyncIO::SyncIO_CheckDebugIndex(void* debug_index) {
    pthread_rwlock_rdlock(&debug_index_lck_);

    OutChunkQuery_t* debug_index_ptr = (OutChunkQuery_t*) debug_index;
    OutChunkQueryEntry_t* tmp_entry = debug_index_ptr->OutChunkQueryBase;
    string value;
    value.resize(sizeof(RecipeEntry_t), 0);
    bool res = false;
    // tool::Logging("debug check entry", "%d\n", debug_index_ptr->queryNum);

    for (size_t i = 0; i < debug_index_ptr->queryNum; i++) {
        res = debug_index_->QueryBuffer((char*)tmp_entry->chunkHash,
            CHUNK_HASH_HMAC_SIZE, value);

        if (res) {
            // tool::Logging("exist in debug index: duplicate", "\n");
        }
        else {
            // pass the chunk type to duplicate flag for debugging
            tool::Logging("not exist in debug index: recover error", "%d-th, type=%d\n", i, tmp_entry->dedupFlag);
            tool::PrintBinaryArray(tmp_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);
        }

        tmp_entry ++;
    }

    pthread_rwlock_unlock(&debug_index_lck_);

    debug_index_ptr->queryNum = 0;

    return ;
}

