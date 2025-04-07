/**
 * @file configure.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief Implement the interfaces defined in configure.h
 * @version 0.1
 * @date 2023-11-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "../../include/sync_configure.h"

/**
 * @brief Construct a new Configure object
 * 
 * @param path the input configure file path
 */
SyncConfigure::SyncConfigure(string path) {
    this->ReadConfig(path);
}

/**
 * @brief Destory the Configure object
 * 
 */
SyncConfigure::~SyncConfigure() {}

/**
 * @brief parse the json file
 * 
 * @param path the path to the json file
 */
void SyncConfigure::ReadConfig(string path) {
    using namespace boost;
    using namespace boost::property_tree;

    ptree root;
    read_json<ptree>(path, root);

    // segment settings
    max_seg_size_ = root.get<uint64_t>("Segment.max_seg_size");

    // chunk settings
    max_chunk_size_ = root.get<uint64_t>("Chunk.max_chunk_size");
    min_chunk_size_ = root.get<uint64_t>("Chunk.min_chunk_size");
    avg_chunk_size_ = root.get<uint64_t>("Chunk.avg_chunk_size");

    // outside indexes
    out_seg_db_name_ = root.get<string>("OutsideDB.out_seg_db_name");
    out_chunk_db_name_ = root.get<string>("OutsideDB.out_chunk_db_name");
    out_feature_db_name_ = root.get<string>("OutsideDB.out_feature_db_name");

    // cloud-1 settings
    id_1_ = root.get<int>("Cloud_1.id");
    ip_1_ = root.get<string>("Cloud_1.ip");
    // send_port_1_ = root.get<int>("Cloud_1.send_port");
    // recv_port_1_ = root.get<int>("Cloud_1.recv_port");
    p1_send_port_ = root.get<int>("Cloud_1.p1_send_port");
    p3_recv_port_ = root.get<int>("Cloud_1.p3_recv_port");
    p3_send_port_ = root.get<int>("Cloud_1.p3_send_port");
    p5_recv_port_ = root.get<int>("Cloud_1.p5_recv_port");
    p5_send_port_ = root.get<int>("Cloud_1.p5_send_port");
    file_root_path_1_ = root.get<string>("Cloud_1.file_root_path");

    // cloud-2 settings
    id_2_ = root.get<int>("Cloud_2.id");
    ip_2_ = root.get<string>("Cloud_2.ip");
    // send_port_2_ = root.get<int>("Cloud_2.send_port");
    // recv_port_2_ = root.get<int>("Cloud_2.recv_port");
    p2_recv_port_ = root.get<int>("Cloud_2.p2_recv_port");
    p2_send_port_ = root.get<int>("Cloud_2.p2_send_port");
    p4_recv_port_ = root.get<int>("Cloud_2.p4_recv_port");
    p4_send_port_ = root.get<int>("Cloud_2.p4_send_port");
    p6_recv_port_ = root.get<int>("Cloud_2.p6_recv_port");
    file_root_path_2_ = root.get<string>("Cloud_2.file_root_path");
    
    // sender settings
    send_data_batch_size_ = root.get<uint64_t>("Sender.send_data_batch_size");
    send_meta_batch_size_ = root.get<uint64_t>("Sender.send_meta_batch_size");

    return ;
}