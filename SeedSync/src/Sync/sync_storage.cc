/**
 * @file sync_storage.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief
 * @version 0.1
 * @date 2024-02-11
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../../include/sync_storage.h"

/**
 * @brief Construct a new Sync Storage object
 *
 */
SyncStorage::SyncStorage() {
    // init the local batch buf
    cur_local_num_ = 0;
    local_batch_buf_ = (uint8_t*)malloc(config.GetSendChunkBatchSize() * (MAX_CHUNK_SIZE + sizeof(uint32_t)));

    cur_physical_tail_ = 0;

    container_cache_ = new ReadCache();
}

/**
 * @brief Destroy the Sync Storage object
 *
 */
SyncStorage::~SyncStorage() {
    // free(req_containers_.idBuffer);
    // for (size_t i = 0; i < CONTAINER_CAPPING_VALUE; i++) {
    //     free(req_containers_.containerArray[i]);
    // }
    // free(req_containers_.containerArray);
    delete container_cache_;

    free(local_batch_buf_);
}

/**
 * @brief Get the Required Containers object
 *
 * @param outClient the out-enclave client ptr
 */
void SyncStorage::GetReqContainers(ReqContainer_t* reqContainer) {
    // ReqContainer_t* reqContainer = req_container;
    uint8_t* idBuffer = reqContainer->idBuffer;
    uint8_t** containerArray = reqContainer->containerArray;
    uint32_t idNum = reqContainer->idNum;

    // TODO: add the read file lock here

    // retrieve each container
    string containerNameStr;

    // cout << "total req container num " << idNum << endl;

#if (DEBUG_FLAG == 1)    
    cout << "total req container num " << idNum << endl;
#endif    
    for (size_t i = 0; i < idNum; i++) {
        containerNameStr.assign((char*)(idBuffer + i * CONTAINER_ID_LENGTH),
            CONTAINER_ID_LENGTH);
        // tool::Logging(my_name_.c_str(), "Target get container ID in current batch: %ld (%ld), name: %s\n", i, idNum, containerNameStr.c_str());
        // step-1: check the container cache
        bool cacheHitStatus = container_cache_->ExistsInCache(containerNameStr);
        if (cacheHitStatus) {
            // step-2: exist in the container cache, read from the cache, directly copy the data from the cache
#if (RAW_CONTAINER == 1)
            memcpy(containerArray[i], container_cache_->ReadFromCache(containerNameStr),
                MAX_CONTAINER_SIZE);
#endif

#if (CONTAINER_META_SESSION == 1)
            memcpy(containerArray[i], container_cache_->ReadFromCache(containerNameStr),
                MAX_CONTAINER_SIZE);
#endif
            continue;
        }

        // step-3: not exist in the contain cache, read from disk
        ifstream containerIn;
        string readFileNameStr = config.GetContainerRootPath() + containerNameStr + config.GetContainerSuffix();
        containerIn.open(readFileNameStr, ifstream::in | ifstream::binary);
        // tool::Logging(my_name_.c_str(), "Target get container ID in current batch: %ld (%ld), full name: %s\n", i, idNum, readFileNameStr.c_str());

        if (!containerIn.is_open()) {
            // debug
            uint8_t check_id[CONTAINER_ID_LENGTH];
            memcpy(check_id, (uint8_t*)containerNameStr.c_str(), CONTAINER_ID_LENGTH);

            // tool::Logging(my_name_.c_str(), "cannot open the container: %s\n", readFileNameStr.c_str());
            tool::PrintBinaryArray(check_id, CONTAINER_ID_LENGTH);
            tool::PrintBinaryArray((uint8_t*)readFileNameStr.c_str(), readFileNameStr.size());
            cout << readFileNameStr << " " << i << "-th" << endl;
            exit(EXIT_FAILURE);
        }

        // get the data section size (total chunk size - metadata section)
        containerIn.seekg(0, ios_base::end);
        int readSize = containerIn.tellg();
        containerIn.seekg(0, ios_base::beg);

        // read the metadata section
        int containerSize = 0;
        containerSize = readSize;
        // read compression data
        // problem here ///////////////////////////////////////////////////////////////////////////////
        // cout<<"read container size "<<containerSize<<endl;
        containerIn.read((char*)containerArray[i], containerSize);

        if (containerIn.gcount() != containerSize) {
            // tool::Logging(my_name_.c_str(), "read size %lu cannot match expected size: %d for container %s.\n", containerIn.gcount(), containerSize, readFileNameStr.c_str());
            exit(EXIT_FAILURE);
        }

        // close the container file
        containerIn.close();
        container_cache_->InsertToCache(containerNameStr, containerArray[i], containerSize);
    }
    return;
}

/**
 * @brief Get the Req Containers With Size
 *
 * @param req_container
 */
void SyncStorage::GetReqContainersWithSize(ReqContainer_t* reqContainer) {
    // ReqContainer_t* reqContainer = req_container;
    uint8_t* idBuffer = reqContainer->idBuffer;
    uint8_t** containerArray = reqContainer->containerArray;
    uint32_t idNum = reqContainer->idNum;
    uint32_t* sizeArray = reqContainer->sizeArray;

    // retrieve each container
    string containerNameStr;
#if (DEBUG_FLAG == 1)    
    cout << "total req container num " << idNum << endl;
#endif    
    for (size_t i = 0; i < idNum; i++) {
        containerNameStr.assign((char*)(idBuffer + i * CONTAINER_ID_LENGTH),
            CONTAINER_ID_LENGTH);
        // step-1: check the container cache
        bool cacheHitStatus = container_cache_->ExistsInCache(containerNameStr);
        if (cacheHitStatus) {
            // step-2: exist in the container cache, read from the cache, directly copy the data from the cache
#if (RAW_CONTAINER == 1)
            memcpy(containerArray[i], container_cache_->ReadFromCache(containerNameStr),
                MAX_CONTAINER_SIZE);
#endif

#if (CONTAINER_META_SESSION == 1)
            memcpy(containerArray[i], container_cache_->ReadFromCache(containerNameStr),
                MAX_CONTAINER_SIZE);
#endif
            continue;
        }

        // step-3: not exist in the contain cache, read from disk
        ifstream containerIn;
        string readFileNameStr = config.GetContainerRootPath() + containerNameStr + config.GetContainerSuffix();

        // check whether the container file has been persisted to disk
        bool file_exist = tool::FileExist(readFileNameStr);
        if (file_exist == false) {
            // the container is not written yet, directly return
            // tool::Logging(my_name_.c_str(), "the container file does not exist.\n");
            tool::PrintBinaryArray((uint8_t*)readFileNameStr.c_str(), readFileNameStr.size());
            sizeArray[i] = 0;
            continue;
        }

        containerIn.open(readFileNameStr, ifstream::in | ifstream::binary);
        if (!containerIn.is_open()) {
            // debug
            uint8_t check_id[CONTAINER_ID_LENGTH];
            memcpy(check_id, (uint8_t*)containerNameStr.c_str(), CONTAINER_ID_LENGTH);

            // tool::Logging(my_name_.c_str(), "cannot open the container: %s\n", readFileNameStr.c_str());
            tool::PrintBinaryArray(check_id, CONTAINER_ID_LENGTH);
            tool::PrintBinaryArray((uint8_t*)readFileNameStr.c_str(), readFileNameStr.size());
            cout << readFileNameStr << " " << i << "-th" << endl;
            exit(EXIT_FAILURE);
        }

        // get the data section size (total chunk size - metadata section)
        containerIn.seekg(0, ios_base::end);
        int readSize = containerIn.tellg();
        containerIn.seekg(0, ios_base::beg);

        // read the metadata section
        int containerSize = 0;
        containerSize = readSize;
        // read compression data
        // problem here ///////////////////////////////////////////////////////////////////////////////
        // cout<<"read container size "<<containerSize<<endl;
        containerIn.read((char*)containerArray[i], containerSize);
        // set the size array
        sizeArray[i] = containerSize;

        if (containerIn.gcount() != containerSize) {
            // tool::Logging(my_name_.c_str(), "read size %lu cannot match expected size: %d for container %s.\n", containerIn.gcount(), containerSize, readFileNameStr.c_str());
            exit(EXIT_FAILURE);
        }

        // close the container file
        containerIn.close();
        container_cache_->InsertToCache(containerNameStr, containerArray[i], containerSize);
    }
    return;
}

/**
 * @brief Get the Container object
 *
 * @param container
 * @param container_id
 */
void SyncStorage::GetContainer(Container_t* req_container)
{
#if (CONTAINER_META_SESSION)
    // read the container from disk
    string container_name_str;
    container_name_str.assign((char*)req_container->containerID, CONTAINER_ID_LENGTH);
    ifstream container_hdl;
    string container_path = config.GetContainerRootPath() + container_name_str + config.GetContainerSuffix();

    // check whether the container file has been persisted to disk
    bool file_exist = tool::FileExist(container_path);
    if (file_exist == false) {
        // the container is not written yet, directly return
        // tool::Logging(my_name_.c_str(), "the container file does not exist.\n");
        tool::PrintBinaryArray((uint8_t*)container_path.c_str(), container_path.size());
        req_container->currentSize = 0;
        req_container->currentMetaSize = 0;
        return;
    }

    container_hdl.open(container_path, ifstream::in | ifstream::binary);

    if (!container_hdl.is_open()) {
        // tool::Logging(my_name_.c_str(), "cannot open the container %s\n", container_path.c_str());
        exit(EXIT_FAILURE);
    }

    // container_hdl.seekg(0, ios_base::end);
    // int read_size = container_hdl.tellg();
    // container_hdl.seekg(0, ios_base::beg);

    // read data session
    container_hdl.seekg(0, ios_base::end);
    size_t total_read_size = container_hdl.tellg();
    container_hdl.seekg(0, ios_base::beg);
    // cout<<"total read size "<<total_read_size<<endl;

    container_hdl.read((char*)&req_container->currentMetaSize, sizeof(uint32_t));
    // cout<<"cur meta size "<<req_container->currentMetaSize<<endl;
    // read metadata session
    container_hdl.read((char*)req_container->metadata, req_container->currentMetaSize);

#if (DEBUG_FLAG == 1)
    // // debug: check feature & fp
    // uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
    // uint8_t check_fp[CHUNK_HASH_SIZE];
    // uint32_t check_offset = 0;

    // size_t metanum = req_container->currentMetaSize / (CHUNK_HASH_SIZE + sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
    // for (size_t i = 0; i < metanum; i ++) {

    //     memcpy((uint8_t*)check_features, req_container->metadata + check_offset, sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK);
    //     check_offset += sizeof(uint64_t) * SUPER_FEATURE_PER_CHUNK;
    //     memcpy(check_fp, req_container->metadata + check_offset, CHUNK_HASH_SIZE);
    //     check_offset += CHUNK_HASH_SIZE;

    //     // tool::Logging("check features","%d\t%d\t%d\n", check_features[0], check_features[1], check_features[2]);

    //     tool::PrintBinaryArray(check_fp, CHUNK_HASH_SIZE);
    // }
#endif

    size_t read_meta_size = container_hdl.tellg();
    // cout<<"read meta size "<<read_meta_size<<endl;

    size_t read_data_size = total_read_size - read_meta_size;
    // cout<<"read data size "<<read_data_size<<endl;

    container_hdl.read((char*)req_container->body, read_data_size);
    req_container->currentSize = read_data_size;

    // int container_size = 0;
    size_t container_size = read_data_size + req_container->currentMetaSize + sizeof(uint32_t);
    // cout<<"container size (calc)"<<container_size<<endl;
    // cout<<"gcount"<<container_hdl.gcount()<<endl;

    // container_hdl.read((char*)container, container_size);

    if (container_hdl.gcount() != total_read_size - read_meta_size) {
        // tool::Logging(my_name_.c_str(), "read size %lu cannot match expected size %d for container %s.\n", container_hdl.gcount(), total_read_size - read_meta_size, container_path.c_str());
        container_hdl.close();
        exit(EXIT_FAILURE);
    }

    container_hdl.close();
#endif
    return;
}

/**
 * @brief read chunk batch from tmp file
 *
 * @param chunk_data
 * @param chunk_num
 * @param batch_size
 * @return true
 * @return false
 */
bool SyncStorage::ReadChunkBatch(uint8_t* chunk_data, uint32_t chunk_num) {
    mutex_.lock();
    // pop idx from queue
    uint32_t read_idx;
    if (!idx_queue_.empty()) {
        read_idx = idx_queue_.front();
        idx_queue_.pop();
    }

    // read the chunk batch from the offset = read_idx

    ifstream read_hdl;
    read_hdl.open(tmp_file_name, ios_base::in | ios_base::binary);
    if (!read_hdl.is_open()) {
        // tool::Logging("tmp file open error, %s.\n", tmp_file_name.c_str());
    }

    // read_hdl.seekg(0, ios::beg);
    // read_hdl.seekg(0, ios::end);
    // auto file_size = read_hdl.tellg();
    // cout<<"tmp file size "<<file_size<<endl;

    read_hdl.seekg(0, ios::beg);
    read_hdl.seekg(config.GetSendChunkBatchSize() * (sizeof(uint32_t) + MAX_CHUNK_SIZE)
            * read_idx,
        ios::beg);
    cout << "read idx " << read_idx << endl;
    cout << "read from: " << config.GetSendChunkBatchSize() * (sizeof(uint32_t) + MAX_CHUNK_SIZE) * read_idx << endl;
    read_hdl.read((char*)chunk_data, config.GetSendChunkBatchSize() * (sizeof(uint32_t) + MAX_CHUNK_SIZE));

    // read finish, push the idx to avail
    avali_idx_.push(read_idx);

    read_hdl.close();

    // unlock
    mutex_.unlock();

    return true;
}

/**
 * @brief write chunk batch to tmp file
 *
 * @param chunk_data
 * @param chunk_num
 * @param batch_size
 * @return true
 * @return false
 */
bool SyncStorage::WriteChunkBatch(uint8_t* chunk_data, uint32_t chunk_num) {
    if ((cur_local_num_ + 1) <= config.GetSendChunkBatchSize()) {
        // write to local batch

    } else {
        // write the local batch to file
        mutex_.lock();

        uint32_t write_idx;

        // check the avali
        if (!avali_idx_.empty()) {
            write_idx = avali_idx_.front();
            avali_idx_.pop();
        } else {
            write_idx = cur_physical_tail_;
            cur_physical_tail_++;
        }

        cout << "write idx " << write_idx << endl;

        ofstream write_hdl;
        write_hdl.open(tmp_file_name, ios_base::out | ios_base::binary);
        if (!write_hdl.is_open()) {
            // tool::Logging("tmp file open error, %s.\n", tmp_file_name.c_str());
        }

        write_hdl.seekp(0, ios::beg);
        write_hdl.seekp(config.GetSendChunkBatchSize() * (sizeof(uint32_t) + MAX_CHUNK_SIZE)
                * write_idx,
            ios::beg);
        cout << "write from " << config.GetSendChunkBatchSize() * (sizeof(uint32_t) + MAX_CHUNK_SIZE) * write_idx << endl;

        write_hdl.write((char*)chunk_data, chunk_num * (sizeof(uint32_t) + MAX_CHUNK_SIZE));

        // update the idx queue
        idx_queue_.push(write_idx);

        write_hdl.close();

        mutex_.unlock();

        // TODO: write current chunk to local batch
    }

    return true;
}
