/**
 * @file configure.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief Define the interfaces of configure
 * @version 0.1
 * @date 2023-11-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SYNC_CONFIGURE_H
#define SYNC_CONFIGURE_H

#include <bits/stdc++.h>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include "constVar.h"
#include "define.h"

using namespace std;

class SyncConfigure {
    private:
        string my_name_ = "SyncConfigure";

        // segment settings
        uint64_t max_seg_size_;

        // chunk settings
        uint64_t max_chunk_size_;
        uint64_t min_chunk_size_;
        uint64_t avg_chunk_size_;

        // outside indexes
        string out_seg_db_name_;
        string out_chunk_db_name_;
        string out_feature_db_name_;

        // cloud-1 settings
        int id_1_;
        string ip_1_;
        int send_port_1_;
        int recv_port_1_;
        string file_root_path_1_;

        // cloud-2 settings
        int id_2_;
        string ip_2_;
        int send_port_2_;
        int recv_port_2_;
        string file_root_path_2_;

        // sender settings
        uint64_t send_data_batch_size_;
        uint64_t send_meta_batch_size_;

        /**
         * @brief parse the json file
         * 
         * @param path the path to the json file
         */
        void ReadConfig(string path);

    public:

        /**
         * @brief Construct a new Configure object
         * 
         * @param path the path to the josn file
         */
        SyncConfigure(string path);
        
        /**
         * @brief Destroy the Configure object
         * 
         */
        ~SyncConfigure();

        uint64_t GetMaxSegSize() {
            return max_seg_size_;
        }
        uint64_t GetMaxChunkSize() {
            return max_chunk_size_;
        }
        uint64_t GetMinChunkSize() {
            return min_chunk_size_;
        }
        uint64_t GetAvgChunkSize() {
            return avg_chunk_size_;
        }
        string GetOutSegDBName() {
            return out_seg_db_name_;
        }
        string GetOutChunkDBName() {
            return out_chunk_db_name_;
        }
        string GetOutFeatureDBName() {
            return out_feature_db_name_;
        }
        int GetCloud1ID() {
            return id_1_;
        }
        string GetCloud1IP() {
            return ip_1_;
        }
        int GetCloud1SendPort() {
            return send_port_1_;
        }
        int GetCloud1RecvPort() {
            return recv_port_1_;
        }
        string GetCloud1Path() {
            return file_root_path_1_;
        }
        int GetCloud2ID() {
            return id_2_;
        }
        string GetCloud2IP() {
            return ip_2_;
        }
        int GetCloud2SendPort() {
            return send_port_2_;
        }
        int GetCloud2RecvPort() {
            return recv_port_2_;
        }
        string GetCloud2Path() {
            return file_root_path_2_;
        }
        uint64_t GetDataBatchSize() {
            return send_data_batch_size_;
        }
        uint64_t GetMetaBatchSize() {
            return send_meta_batch_size_;
        }
};

#endif