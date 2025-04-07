/**
 * @file ecallStreamChunkHash.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief
 * @version 0.1
 * @date 2024-05-23
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef ECALL_STREAM_CHUNK_HASH_H
#define ECALL_STREAM_CHUNK_HASH_H

#include "commonEnclave.h"
#include "ecallEnc.h"

#include "../../../include/chunkStructure.h"
#include "../../../include/constVar.h"

class EcallCrypto;

class EcallStreamChunkHash {
private:
    string my_name_ = "EcallStreamChunkHash";

    // for crypto
    EcallCrypto* crypto_util_;
    EVP_CIPHER_CTX* cipher_ctx_;
    EVP_MD_CTX* md_ctx_;
    uint8_t iv_[CRYPTO_BLOCK_SIZE];

    string master_key_ = "12345";

    // session key
    uint8_t session_key_[CHUNK_HASH_SIZE];

    // decrypted recipe buffer
    uint8_t* plain_recipe_buf_;

    // chunk fp buffer
    uint8_t* plain_fp_list_;

public:
    // for logs
    uint64_t _total_chunk_fp_num = 0;
    uint64_t _total_chunk_size = 0;

    /**
     * @brief Construct a new Ecall Stream Chunk Hash object
     *
     */
    EcallStreamChunkHash();

    /**
     * @brief Destroy the Ecall Stream Chunk Hash object
     *
     */
    ~EcallStreamChunkHash();

    /**
     * @brief process the recipe batch
     *
     * @param filename_buf
     * @param recipe_buf
     * @param recipe_num
     * @param chunk_hash_buf
     */
    void ProcessBatch(uint8_t* filename_buf, uint32_t currentBatchID, uint8_t* recipe_buf,
        size_t recipe_num, uint8_t* chunk_hash_buf);
};

#endif