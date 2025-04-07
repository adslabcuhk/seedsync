/**
 * @file sync_storage.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-02-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SYNC_STORAGE_H
#define SYNC_STORAGE_H

#include "configure.h"
#include "sync_configure.h"
#include "chunkStructure.h"
#include "readCache.h"
// #include "absDatabase.h"

class SyncStorage {
    private:
        string my_name_ = "SyncStorage";

        // // for ocall read
        // ReqContainer_t req_containers_;
        // ReadCache* container_cache_;

        uint32_t cur_local_num_;
        uint8_t* local_batch_buf_;
        
        uint32_t cur_physical_tail_;
        
        std::queue<uint32_t> idx_queue_;
        std::queue<uint32_t> avali_idx_;

        std::mutex mutex_;

        string tmp_file_name = "tmp-chunk-pool";

    public:
        ReadCache* container_cache_;
        
        uint64_t read_from_cache_num_ = 0;
        uint64_t read_from_disk_num_ = 0;
        
        /**
         * @brief Construct a new Sync Storage object
         * 
         */
        SyncStorage();

        /**
         * @brief Destroy the Sync Storage object
         * 
         */
        ~SyncStorage();

        /**
         * @brief Get the Req Containers object
         * 
         */
        void GetReqContainers(ReqContainer_t* req_container);

        /**
         * @brief Get the Req Containers With Size
         * 
         * @param req_container 
         */
        void GetReqContainersWithSize(ReqContainer_t* req_container);

        /**
         * @brief Get the Container object
         * 
         * @param container 
         * @param container_id 
         */
        void GetContainer(Container_t* req_container);

        // /**
        //  * @brief find base chunk from outside feature index
        //  * 
        //  * @param base_query 
        //  */
        // void FindBaseChunk(OutFeatureQueryEntry_t* base_query);

        /**
         * @brief read chunk batch from tmp file
         * 
         * @param chunk_data 
         * @param chunk_num 
         * @param batch_size 
         * @return true 
         * @return false 
         */
        bool ReadChunkBatch(uint8_t* chunk_data, uint32_t chunk_num);
        
        /**
         * @brief write chunk batch to tmp file
         * 
         * @param chunk_data 
         * @param chunk_num 
         * @param batch_size 
         * @return true 
         * @return false 
         */
        bool WriteChunkBatch(uint8_t* chunk_data, uint32_t chunk_num);
};

#endif