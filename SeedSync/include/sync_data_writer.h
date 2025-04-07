/**
 * @file sync_data_writer.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-03-13
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef SYNC_DATA_WRITER_H
#define SYNC_DATA_WRITER_H

#include "configure.h"
#include "sync_configure.h"
#include "chunkStructure.h"
#include "send_thd.h"
#include "absDatabase.h"
#include "../build/src/Enclave/storeEnclave_u.h"

class SyncDataWriter {
    private:
        string my_name_ = "SyncDataWriter";
        
        // for Ecall
        sgx_enclave_id_t sgx_eid_;

        ReqContainer_t req_containers_;

        // for out query (base chunk)
        AbsDatabase* out_fp_db_;
        OutChunkQuery_t out_chunk_query_;

        // for index update
        OutChunkQuery_t update_index_;

        // for debug
        OutChunkQuery_t debug_index_;

        // for container buf
        Container_t container_buf_;
        // Container_t* container_buf_;
        // PtrContainer_t container_buf_;
    
    public:
        uint64_t _total_write_data_size = 0;
        uint64_t _total_write_chunk_num = 0;

        double _phase6_process_time = 0;

        /**
         * @brief Construct a new Sync Data Writer object
         * 
         * @param sgx_eid 
         * @param out_fp_db 
         */
        SyncDataWriter(sgx_enclave_id_t sgx_eid, AbsDatabase* out_fp_db);
        
        /**
         * @brief Destroy the Sync Data Writer object
         * 
         */
        ~SyncDataWriter();

        /**
         * @brief process a batch of recv data
         * 
         * @param recv_buf 
         * @param recv_size 
         */
        void ProcessOneBatch(uint8_t* recv_buf, uint32_t recv_size);

        /**
         * @brief process the tail batch
         * 
         */
        void ProcessTailBatch();
};


#endif