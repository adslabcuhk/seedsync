/**
 * @file ecallStreamChunkHash.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief
 * @version 0.1
 * @date 2024-05-23
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../../include/ecallStreamChunkHash.h"

/**
 * @brief Construct a new Ecall Stream Chunk Hash object
 *
 */
EcallStreamChunkHash::EcallStreamChunkHash()
{
    crypto_util_ = new EcallCrypto(CIPHER_TYPE, HASH_TYPE);
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    md_ctx_ = EVP_MD_CTX_new();
    SyncEnclave::Logging(my_name_.c_str(), "init the StreamChunkHash.\n");

    plain_recipe_buf_ = (uint8_t*)malloc(SyncEnclave::send_recipe_batch_size_ * sizeof(RecipeEntry_t));
    plain_fp_list_ = (uint8_t*)malloc(SyncEnclave::send_meta_batch_size_ * CHUNK_HASH_HMAC_SIZE);

    SyncEnclave::Logging(my_name_.c_str(), "recipeBatchSize = %d.\n", SyncEnclave::send_recipe_batch_size_);
}

/**
 * @brief Destroy the Ecall Stream Chunk Hash object
 *
 */
EcallStreamChunkHash::~EcallStreamChunkHash()
{
    delete (crypto_util_);
    EVP_CIPHER_CTX_free(cipher_ctx_);
    EVP_MD_CTX_free(md_ctx_);

    free(plain_recipe_buf_);
    free(plain_fp_list_);
}

/**
 * @brief process the recipe batch
 *
 * @param filename_buf
 * @param recipe_buf
 * @param recipe_num
 * @param chunk_hash_buf
 */
void EcallStreamChunkHash::ProcessBatch(uint8_t* filename_buf, uint32_t currentBatchID, uint8_t* recipe_buf,
    size_t recipe_num, uint8_t* chunk_hash_buf)
{
    string recipe_path((char*)filename_buf);
    std::string recipePath = recipe_path;
    size_t lastSlashPos = recipePath.find_last_of('/');
    if (lastSlashPos != std::string::npos) {
        recipePath = recipePath.substr(lastSlashPos + 1);
    }
    auto recipeHMACsVecIt = Enclave::fileRecipeNameToHMACIndex_.find(recipePath);
    bool recipeHMACExist = false;
    if (recipeHMACsVecIt == Enclave::fileRecipeNameToHMACIndex_.end()) {
        SyncEnclave::Logging(my_name_.c_str(), "Can not find recipe HMAC in metadata, target recipe path: %s.\n", recipePath.c_str());
    } else {
        recipeHMACExist = true;
    }

    // SyncEnclave::Logging(my_name_.c_str(), "in ecallStreamChunkHash.\n");
    // decrypt the recipe batch
    uint8_t dec_key[CHUNK_HASH_HMAC_SIZE];
    crypto_util_->GenerateHMAC((uint8_t*)&master_key_[0], master_key_.size(),
        dec_key);
    // SyncEnclave::Logging("recipe_num ", "%d\n", recipe_num);

    crypto_util_->DecryptWithKey(cipher_ctx_, recipe_buf,
        recipe_num * sizeof(RecipeEntry_t), dec_key,
        plain_recipe_buf_);

    if (recipeHMACExist) {
        // Verify recipe batch HMAC
        uint8_t currentRecipeBatchHMACBuffer[CHUNK_HASH_HMAC_SIZE];
        crypto_util_->GenerateHMAC(plain_recipe_buf_, recipe_num * sizeof(RecipeEntry_t), currentRecipeBatchHMACBuffer);
        uint8_t storedRecipeBatchHMACBuffer[CHUNK_HASH_HMAC_SIZE];
        memcpy(storedRecipeBatchHMACBuffer, &((recipeHMACsVecIt->second)[currentBatchID][0]), CHUNK_HASH_HMAC_SIZE);
        if (memcmp(currentRecipeBatchHMACBuffer, storedRecipeBatchHMACBuffer, CHUNK_HASH_HMAC_SIZE) != 0) {
            SyncEnclave::Logging(my_name_.c_str(), "Read recipe batch ID: %ld, HMAC mismatch, stored HMAC: %s, computed HMAC: %s\n", currentBatchID, Enclave::convertBufferToHexString(storedRecipeBatchHMACBuffer, CHUNK_HASH_HMAC_SIZE).c_str(), Enclave::convertBufferToHexString(currentRecipeBatchHMACBuffer, CHUNK_HASH_HMAC_SIZE).c_str());
        }
    }
    // uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
    // crypto_util_->GenerateHMAC( recipe_buf, recipe_num * sizeof(RecipeEntry_t),
    // check_hash);
    // Ocall_PrintfBinary(check_hash, CHUNK_HASH_HMAC_SIZE);

    // check key
    // Ocall_PrintfBinary(dec_key, CHUNK_HASH_HMAC_SIZE);

    RecipeEntry_t* tmp_recipe_entry;
    tmp_recipe_entry = (RecipeEntry_t*)plain_recipe_buf_;

    uint32_t offset = 0;

    // get chunk hash for a batch
    for (size_t i = 0; i < recipe_num; i++) {
        // SyncEnclave::Logging("offset ", "%d\n", tmp_recipe_entry->offset);
        // SyncEnclave::Logging("len ", "%d\n", tmp_recipe_entry->length);
        // uint8_t check_chunk_hash[CHUNK_HASH_HMAC_SIZE];
        // memcpy(check_chunk_hash, tmp_recipe_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);
        // Ocall_PrintfBinary(check_chunk_hash, CHUNK_HASH_HMAC_SIZE);

        // copy the chunk fp back to output buf
        memcpy(plain_fp_list_ + offset, tmp_recipe_entry->chunkHash,
            CHUNK_HASH_HMAC_SIZE);
        offset += CHUNK_HASH_HMAC_SIZE;

        // move on
        tmp_recipe_entry++;

        _total_chunk_fp_num++;
        _total_chunk_size += tmp_recipe_entry->length;
    }

    // encrypt the chunk fp batch with sessionkey
    crypto_util_->EncryptWithKey(cipher_ctx_, plain_fp_list_,
        recipe_num * CHUNK_HASH_HMAC_SIZE, session_key_, chunk_hash_buf);

    return;
}