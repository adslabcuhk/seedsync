/**
 * @file enclaveBase.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the interface of the base of enclave
 * @version 0.1
 * @date 2020-12-28
 *
 * @copyright Copyright (c) 2020
 *
 */

#include "../../include/enclaveBase.h"

/**
 * @brief Construct a new Enclave Base object
 *
 */
EnclaveBase::EnclaveBase()
{
    // store the share parameter
    maxSegmentChunkNum_ = MAX_SEGMENT_SIZE / MIN_CHUNK_SIZE;

    // the new object
    storageCoreObj_ = new EcallStorageCore();
    Enclave::Logging(myName_.c_str(), "init the EnclaveBase (before crypto).\n");
    cryptoObj_ = new EcallCrypto(CIPHER_TYPE, HASH_TYPE);
    ///

    Enclave::Logging(myName_.c_str(), "init the EnclaveBase (after crypto).\n");
    // for feature gen
    finesse_util_ = new EcallFinesseUtil(SUPER_FEATURE_PER_CHUNK,
        FEATURE_PER_CHUNK, FEATURE_PER_SUPER_FEATURE);
    rabin_util_ = new RabinFPUtil(SLIDING_WIN_SIZE);
    SyncEnclave::send_chunk_batch_size_;
    rabin_util_->NewCtx(rabin_ctx_);

    Enclave::Logging(myName_.c_str(), "init the EnclaveBase.\n");
}

/**
 * @brief Destroy the Enclave Base object
 *
 */
EnclaveBase::~EnclaveBase()
{
    delete storageCoreObj_;
    delete cryptoObj_;

    rabin_util_->FreeCtx(rabin_ctx_);
    delete rabin_util_;
    delete finesse_util_;
}

/**
 * @brief identify whether it is the end of a segment
 *
 * @param chunkHashVal the input chunk hash
 * @param chunkSize the input chunk size
 * @param segment the reference to current segment
 * @return true is the end
 * @return false is not the end
 */
bool EnclaveBase::IsEndOfSegment(uint32_t chunkHashVal, uint32_t chunkSize,
    Segment_t* segment)
{
    // step-1: judge the number of chunks in this segment
    if (segment->chunkNum + 1 > maxSegmentChunkNum_) {
        // exceed the max allow number of chunks
        // start to process
        return true;
    }

    // step-2: judge the segment size
    if (segment->segmentSize + chunkSize < MIN_SEGMENT_SIZE) {
        // continue
        return false;
    } else if (segment->segmentSize + chunkSize > MAX_SEGMENT_SIZE) {
        // capping the size
        return true;
    } else {
        if (chunkHashVal % DIVISOR == PATTERN) {
            // capping the size
            return true;
        } else {
            // continue
            return false;
        }
    }
}

/**
 * @brief convert hash to a value
 *
 * @param inputHash the input chunk hash
 * @return uint32_t the returned value
 */
uint32_t EnclaveBase::ConvertHashToValue(const uint8_t* inputHash)
{
    uint32_t hashVal = 0;
    for (size_t i = 0; i < CHUNK_HASH_HMAC_SIZE; i++) {
        hashVal += inputHash[i];
    }
    return hashVal;
}

/**
 * @brief update the file recipe
 *
 * @param chunkAddrStr the chunk address string
 * @param inRecipe the in-enclave recipe buffer
 * @param upOutSGX the upload out-enclave var
 */
void EnclaveBase::UpdateFileRecipe(string& chunkAddrStr, Recipe_t* inRecipe,
    UpOutSGX_t* upOutSGX)
{
    memcpy(inRecipe->entryList + inRecipe->recipeNum * sizeof(RecipeEntry_t),
        chunkAddrStr.c_str(), sizeof(RecipeEntry_t));
    inRecipe->recipeNum++;
    uint32_t sendBatchSize = Enclave::sendRecipeBatchSize_;
    if (inRecipe->recipeNum == sendBatchSize) {
        // in-enclave info
        EnclaveClient* sgxClient = (EnclaveClient*)upOutSGX->sgxClient;
        EVP_CIPHER_CTX* cipherCtx = sgxClient->_cipherCtx;
        uint8_t* masterKey = sgxClient->_masterKey;

        // out-enclave info
        Recipe_t* outRecipe = (Recipe_t*)upOutSGX->outRecipe;

#if RECIPE_HMAC == 1
        // update hmac
        uint8_t tailHMACBuffer[CHUNK_HASH_HMAC_SIZE];
        cryptoObj_->GenerateHMAC(inRecipe->entryList, inRecipe->recipeNum * sizeof(RecipeEntry_t), tailHMACBuffer);
        string hmacStr((char*)tailHMACBuffer, CHUNK_HASH_HMAC_SIZE);
        std::string recipePath = sgxClient->currentRecipePath_;
        size_t lastSlashPos = recipePath.find_last_of('/');
        if (lastSlashPos != std::string::npos) {
            recipePath = recipePath.substr(lastSlashPos + 1);
        }
        auto mapIt = Enclave::fileRecipeNameToHMACIndex_.find(recipePath);
        if (mapIt == Enclave::fileRecipeNameToHMACIndex_.end()) {
            vector<string> hmacListVec;
            hmacListVec.push_back(hmacStr);
            Enclave::fileRecipeNameToHMACIndex_.emplace(recipePath, hmacListVec);
            Enclave::Logging("Add Recipe Index", "Normal batch insert recipe path: %s, hmac: %s.\n", recipePath.c_str(), Enclave::convertBufferToHexString(tailHMACBuffer, CHUNK_HASH_HMAC_SIZE).c_str());
        } else {
            mapIt->second.push_back(hmacStr);
            Enclave::Logging("Add Recipe Index", "Normal batch update recipe path: %s, hmac: %s.\n", recipePath.c_str(), Enclave::convertBufferToHexString(tailHMACBuffer, CHUNK_HASH_HMAC_SIZE).c_str());
        }
#endif

        // start to encrypt the file recipe with the enclave key
#if (ENC_MODE == 1)        
        cryptoObj_->EncryptWithKey(cipherCtx, inRecipe->entryList,
            inRecipe->recipeNum * sizeof(RecipeEntry_t), masterKey,
            outRecipe->entryList);
#endif

#if (Plain_MODE == 1)
        memcpy(outRecipe->entryList, inRecipe->entryList,
            inRecipe->recipeNum * sizeof(RecipeEntry_t));
#endif    

        outRecipe->recipeNum = inRecipe->recipeNum;
        Ocall_UpdateFileRecipe(upOutSGX->outClient);
        inRecipe->recipeNum = 0;
    }

    return;
}

/**
 * @brief process an unique chunk
 *
 * @param chunkAddr the chunk address
 * @param chunkBuffer the chunk buffer
 * @param chunkSize the chunk size
 * @param upOutSGX the upload out-enclave var
 */
void EnclaveBase::ProcessUniqueChunk(RecipeEntry_t* chunkAddr, uint8_t* chunkBuffer,
    uint32_t chunkSize, UpOutSGX_t* upOutSGX)
{
    EnclaveClient* sgxClient = (EnclaveClient*)upOutSGX->sgxClient;
    uint8_t* currentIV = sgxClient->PickNewIV();
    EVP_CIPHER_CTX* cipher = sgxClient->_cipherCtx;
    uint8_t tmpCompressedChunk[MAX_CHUNK_SIZE];
    int tmpCompressedChunkSize = 0;

    // // generate features for unique chunk
    // uint64_t features[SUPER_FEATURE_PER_CHUNK];
    // finesse_util_->ExtractFeature(rabin_ctx_, chunkBuffer, chunkSize,
    //     features);

    // // generate fp
    // EVP_MD_CTX* mdCtx = sgxClient->_mdCtx;
    // uint8_t chunkFP[CHUNK_HASH_HMAC_SIZE];
    // // cryptoObj_->GenerateHMAC( chunkBuffer, chunkSize, chunkAddr->chunkHash);
    // cryptoObj_->GenerateHMAC( chunkBuffer, chunkSize, chunkFP);

    // keep the original chunk size before local compression
    // chunkAddr->originLen = chunkSize;
    // Enclave::Logging("chunkAdder.originLen ", "%d\n", chunkAddr->originLen);

    uint8_t tmpCipherChunk[MAX_CHUNK_SIZE];
#if (SGX_BREAKDOWN == 1)
    Ocall_GetCurrentTime(&_startTime);
#endif
    tmpCompressedChunkSize = LZ4_compress_fast((char*)(chunkBuffer), (char*)tmpCompressedChunk,
        chunkSize, chunkSize, 3);
#if (SGX_BREAKDOWN == 1)
    Ocall_GetCurrentTime(&_endTime);
    _compressTime += (_endTime - _startTime);
    _compressCount++;
#endif

#if (SGX_BREAKDOWN == 1)
    Ocall_GetCurrentTime(&_startTime);
#endif
    if (tmpCompressedChunkSize > 0) {
        // it can be compressed
        _compressedDataSize += tmpCompressedChunkSize;

        // Enclave::Logging("before enc ", "%d, %d\n", chunkAddr->length, chunkAddr->originLen);
        // do encryption
        cryptoObj_->EncryptWithKeyIV(cipher, tmpCompressedChunk, tmpCompressedChunkSize,
            Enclave::enclaveKey_, tmpCipherChunk, currentIV);
    } else {
        // it cannot be compressed
        _compressedDataSize += chunkSize;
        tmpCompressedChunkSize = chunkSize;

        // do encryption
        cryptoObj_->EncryptWithKeyIV(cipher, chunkBuffer, chunkSize, Enclave::enclaveKey_,
            tmpCipherChunk, currentIV);
    }
#if (SGX_BREAKDOWN == 1)
    Ocall_GetCurrentTime(&_endTime);
    _encryptTime += (_endTime - _startTime);
    _encryptCount++;
#endif

    // finish the encryption, assign this a container
    storageCoreObj_->SaveChunk((char*)tmpCipherChunk, tmpCompressedChunkSize,
        chunkAddr, upOutSGX);
    return;
}

/**
 * @brief process an unique chunk (with FP & features)
 *
 * @param chunkAddr the chunk address (with FP)
 * @param features the chunk features
 * @param chunkBuffer the chunk buffer
 * @param chunkSize the chunk size
 * @param upOutSGX the upload out-enclave var
 */
void EnclaveBase::ProcessUniqueChunk(RecipeEntry_t* chunkAddr, uint64_t* features,
    uint8_t* chunkBuffer, uint32_t chunkSize, UpOutSGX_t* upOutSGX)
{
    EnclaveClient* sgxClient = (EnclaveClient*)upOutSGX->sgxClient;
    uint8_t* currentIV = sgxClient->PickNewIV();
    EVP_CIPHER_CTX* cipher = sgxClient->_cipherCtx;
    uint8_t tmpCompressedChunk[MAX_CHUNK_SIZE];
    int tmpCompressedChunkSize = 0;

    // debug
    // Enclave::Logging("ProcessUni chunkAddr ", "\n");
    // uint8_t tmp_checkhash[CHUNK_HASH_HMAC_SIZE];
    // memcpy(tmp_checkhash, chunkAddr->chunkHash, CHUNK_HASH_HMAC_SIZE);
    // Ocall_PrintfBinary(tmp_checkhash, CHUNK_HASH_HMAC_SIZE);

    // keep the original chunk size before local compression
    // chunkAddr->originLen = chunkSize;
    // Enclave::Logging("chunkAdder.originLen ", "%d\n", chunkAddr->originLen);

    uint8_t tmpCipherChunk[MAX_CHUNK_SIZE];
#if (SGX_BREAKDOWN == 1)
    Ocall_GetCurrentTime(&_startTime);
#endif
    tmpCompressedChunkSize = LZ4_compress_fast((char*)(chunkBuffer), (char*)tmpCompressedChunk,
        chunkSize, chunkSize, 3);
#if (SGX_BREAKDOWN == 1)
    Ocall_GetCurrentTime(&_endTime);
    _compressTime += (_endTime - _startTime);
    _compressCount++;
#endif

#if (SGX_BREAKDOWN == 1)
    Ocall_GetCurrentTime(&_startTime);
#endif
    if (tmpCompressedChunkSize > 0) {
        // it can be compressed
        _compressedDataSize += tmpCompressedChunkSize;

        // Enclave::Logging("before enc ", "%d, %d\n", chunkAddr->length, chunkAddr->originLen);
        // do encryption
#if (ENC_MODE == 1)        
        cryptoObj_->EncryptWithKeyIV(cipher, tmpCompressedChunk, tmpCompressedChunkSize,
            Enclave::enclaveKey_, tmpCipherChunk, currentIV);
#endif

#if (Plain_MODE == 1)
        memcpy(tmpCipherChunk, tmpCompressedChunk, tmpCompressedChunkSize);
#endif

    } else {
        // it cannot be compressed

        // // debug
        // uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
        // memcpy(check_hash, chunkAddr->chunkHash, CHUNK_HASH_HMAC_SIZE);
        // Enclave::Logging("decompression error ", "%d\n", chunkSize);
        // Ocall_PrintfBinary(check_hash, CHUNK_HASH_HMAC_SIZE);

        _compressedDataSize += chunkSize;
        tmpCompressedChunkSize = chunkSize;

        // do encryption
#if (ENC_MODE == 1)        
        cryptoObj_->EncryptWithKeyIV(cipher, chunkBuffer, chunkSize, Enclave::enclaveKey_,
            tmpCipherChunk, currentIV);
#endif

#if (Plain_MODE == 1)
        memcpy(tmpCipherChunk, chunkBuffer, chunkSize);
#endif

    }
#if (SGX_BREAKDOWN == 1)
    Ocall_GetCurrentTime(&_endTime);
    _encryptTime += (_endTime - _startTime);
    _encryptCount++;
#endif

    // // debug
    // for (size_t i = 0; i < SUPER_FEATURE_PER_CHUNK; i++)
    //     Enclave::Logging("ProcessUni features ","%d\t", features[i]);
    // Enclave::Logging("\n", "");

    // Enclave::Logging("before save chunk", "\n");
    // finish the encryption, assign this a container
    storageCoreObj_->SaveChunk((char*)tmpCipherChunk, tmpCompressedChunkSize,
        chunkAddr, features, upOutSGX);
    // Enclave::Logging("after save chunk", "\n");
    return;
}

/**
 * @brief update the index store
 *
 * @param key the key of the k-v pair
 * @param buffer the data buffer
 * @param bufferSize the size of the buffer
 * @return true success
 * @return false fail
 */
bool EnclaveBase::UpdateIndexStore(const string& key, const char* buffer,
    size_t bufferSize)
{
    bool status;
    Ocall_UpdateIndexStoreBuffer(&status, key.c_str(), key.size(),
        (const uint8_t*)buffer, bufferSize);
    return status;
}

/**
 * @brief read the information from the index store
 *
 * @param key key
 * @param value value
 * @param upOutSGX the upload out-enclave var
 * @return true
 * @return false
 */
bool EnclaveBase::ReadIndexStore(const string& key, string& value,
    UpOutSGX_t* upOutSGX)
{
    bool status;
    size_t expectedBufferSize = 0;
    uint8_t* bufferPtr;
    Ocall_ReadIndexStore(&status, key.c_str(), key.size(), &bufferPtr,
        &expectedBufferSize, upOutSGX->outClient);

    // copy the buffer to the string
    value.assign((const char*)bufferPtr, expectedBufferSize);
    return status;
}

/**
 * @brief Get the Time Differ object
 *
 * @param sTime the start time
 * @param eTime the end time
 * @return double the diff of time
 */
double EnclaveBase::GetTimeDiffer(uint64_t sTime, uint64_t eTime)
{
    double second = (eTime - sTime) / SEC_TO_USEC;
    return second;
}

/**
 * @brief reset the value of current segment
 *
 * @param sgxClient the to the current client
 */
void EnclaveBase::ResetCurrentSegment(EnclaveClient* sgxClient)
{
    sgxClient->_segment.chunkNum = 0;
    sgxClient->_segment.minHashVal = UINT32_MAX;
    sgxClient->_segment.segmentSize = 0;
    return;
}
