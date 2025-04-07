/**
 * @file sync_io.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-10-17
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SYNC_IO_H
#define SYNC_IO_H

#include "absDatabase.h"
#include "sync_storage.h"
#include "phase_sender.h"

class SyncIO {
    private:
        string my_name_ = "SyncIO";

    public:
        AbsDatabase* out_chunk_index_ = NULL;
        AbsDatabase* debug_index_ = NULL;
        SyncStorage* sync_storage_ = NULL;

        // for lck
        pthread_rwlock_t chunk_index_lck_;
        pthread_rwlock_t debug_index_lck_;
        pthread_mutex_t sync_storage_lck_;

        /**
         * @brief init the sync io
         * 
         * @param out_chunk_index 
         * @param out_feature_index 
         */
        SyncIO(AbsDatabase* out_chunk_index, AbsDatabase* debug_index,
            SyncStorage* sync_storage);

        /**
         * @brief destroy the sync io
         * 
         */
        ~SyncIO();

        /**
         * @brief query the chunk index
         * 
         * @param out_chunk_query 
         */
        void SyncIO_QueryChunkIndex(void* out_chunk_query);

        /**
         * @brief query the chunk addr
         * 
         * @param out_chunk_query 
         */
        void SyncIO_QueryChunkAddr(void* out_chunk_query);

        void SyncIO_QueryEncodeChunkAddr(void* out_chunk_query);

        /**
         * @brief issue a single query to the chunk index
         * 
         * @param query_entry 
         */
        void SyncIO_SingleQueryChunkIndex(void* query_entry);

        /**
         * @brief fetch container
         * 
         * @param req_container 
         */
        void SyncIO_SyncGetReqContainer(void* req_container);

        /**
         * @brief fetch container with size
         * 
         * @param req_container 
         */
        void SyncIO_SyncGetReqContainerWithSize(void* req_container);

        /**
         * @brief fetch a single container
         * 
         * @param req_container 
         */
        void SyncIO_SingleGetReqContainer(void* req_container);

        /**
         * @brief write the sync container
         * 
         * @param sync_container 
         */
        void SyncIO_WriteSyncContainer(Container_t* newContainer);

        /**
         * @brief update the outside fp index
         * 
         * @param update_index 
         */
        void SyncIO_UpdateOutFPIndex(void* update_index);

        void SyncIO_InsertDebugIndex(void* debug_index);

        void SyncIO_CheckDebugIndex(void* debug_index);


};

#endif