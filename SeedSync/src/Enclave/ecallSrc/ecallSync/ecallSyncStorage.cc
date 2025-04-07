/**
 * @file ecallSyncStorage.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-02-23
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/ecallSyncStorage.h"

/**
 * @brief Construct a new Ecall Sync Storage object
 * 
 */
EcallSyncStorage::EcallSyncStorage() {

}

/**
 * @brief Destroy the Ecall Sync Storage object
 * 
 */
EcallSyncStorage::~EcallSyncStorage() {

}

/**
 * @brief write the chunk batch, waiting for process of the next phases
 * 
 * @param chunk_data 
 * @param chunk_num 
 */
void EcallSyncStorage::WriteTmpChunkBatch(uint8_t* chunk_data, uint32_t chunk_num) {

}

/**
 * @brief read the pre-persisted chunk batches
 * 
 * @param chunk_data 
 * @param chunk_num 
 */
void EcallSyncStorage::ReadTmpChunkBatch(uint8_t* chunk_data, uint32_t& chunk_num) {

}