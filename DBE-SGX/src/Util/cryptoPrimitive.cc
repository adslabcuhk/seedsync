#include "../../include/cryptoPrimitive.h"

/**
 * @brief Construct a new Crypto Primitive object
 *
 * @param cipherType
 * @param hashType
 */
CryptoPrimitive::CryptoPrimitive(int cipherType, int hashType)
{
    cipherType_ = static_cast<ENCRYPT_SET>(cipherType);
    hashType_ = static_cast<HASH_SET>(hashType);
    iv_ = (uint8_t*)malloc(sizeof(uint8_t) * CRYPTO_BLOCK_SIZE);
    hmacKey_ = (uint8_t*)malloc(sizeof(uint8_t) * CHUNK_ENCRYPT_KEY_SIZE);
    if (!iv_) {
        fprintf(stderr, "CryptoTool: allocate the memory for iv fail.\n");
        exit(EXIT_FAILURE);
    }
    memset(iv_, 0, sizeof(uint8_t) * CRYPTO_BLOCK_SIZE);
    memset(hmacKey_, 0, sizeof(uint8_t) * CHUNK_ENCRYPT_KEY_SIZE);
    if ((hmacCtx_ = HMAC_CTX_new()) == NULL) {
        return;
    }
    if ((mdCtx_ = EVP_MD_CTX_new()) == NULL) {
        return;
    }
}

/**
 * @brief Destroy the Crypto Primitive object
 *
 */
CryptoPrimitive::~CryptoPrimitive()
{
    if (iv_) {
        free(iv_);
    }
    if (hmacKey_) {
        free(hmacKey_);
    }
    if (hmacCtx_) {
        HMAC_CTX_free(hmacCtx_);
    }
    if (mdCtx_) {
        EVP_MD_CTX_free(mdCtx_);
    }
}

/**
 * @brief Generate the hash of the input data
 *
 * @param mdCtx hasher ctx
 * @param dataBuffer input data buffer
 * @param dataSize input data size
 * @param hash output hash
 */
void CryptoPrimitive::GenerateHash(uint8_t* dataBuffer, const int dataSize, uint8_t* hash)
{
    int expectedHashSize = 0;
    switch (hashType_) {
    case SHA_1:
        if (!EVP_DigestInit_ex(mdCtx_, EVP_sha1(), NULL)) {
            fprintf(stderr, "CryptoTool: Hash init error.\n");
            exit(EXIT_FAILURE);
        }
        expectedHashSize = 20;
        break;
    case SHA_256:
        if (!EVP_DigestInit_ex(mdCtx_, EVP_sha256(), NULL)) {
            fprintf(stderr, "CryptoTool: Hash init error.\n");
            exit(EXIT_FAILURE);
        }
        expectedHashSize = 32;
        break;
    case MD5:
        if (!EVP_DigestInit_ex(mdCtx_, EVP_md5(), NULL)) {
            fprintf(stderr, "CryptoTool: Hash init error.\n");
            exit(EXIT_FAILURE);
        }
        expectedHashSize = 16;
        break;
    }
    if (!EVP_DigestUpdate(mdCtx_, dataBuffer, dataSize)) {
        fprintf(stderr, "CryptoTool: Hash error.\n");
        exit(EXIT_FAILURE);
    }
    uint32_t hashSize;
    if (!EVP_DigestFinal_ex(mdCtx_, hash, &hashSize)) {
        fprintf(stderr, "CryptoTool: Hash error.\n");
        exit(EXIT_FAILURE);
    }

    if (hashSize != expectedHashSize) {
        fprintf(stderr, "CryptoTool: Hash size error.\n");
        exit(EXIT_FAILURE);
    }

    EVP_MD_CTX_reset(mdCtx_);
    return;
}

void CryptoPrimitive::GenerateHMAC(uint8_t* dataBuffer, const int dataSize, uint8_t* hmac)
{
    unsigned int length;
    if (HMAC_Init_ex(hmacCtx_, hmacKey_, CHUNK_ENCRYPT_KEY_SIZE, EVP_sha3_256(), NULL) != 1) {
        HMAC_CTX_reset(hmacCtx_);
        return;
    }
    if (HMAC_Update(hmacCtx_, dataBuffer, dataSize) != 1) {
        HMAC_CTX_reset(hmacCtx_);
        return;
    }
    if (HMAC_Final(hmacCtx_, hmac, &length) != 1) {
        HMAC_CTX_reset(hmacCtx_);
        return;
    }
    HMAC_CTX_reset(hmacCtx_);
    return;
}

/**
 * @brief Encrypt the data with the encryption key
 *
 * @param ctx cipher ctx
 * @param dataBuffer input data buffer
 * @param dataSize input data size
 * @param key encryption key
 * @param ciphertext output cipherText
 */
void CryptoPrimitive::EncryptWithKey(EVP_CIPHER_CTX* ctx, uint8_t* dataBuffer, const int dataSize,
    uint8_t* key, uint8_t* ciphertext)
{
    int cipherLen = 0;
    int len = 0;

    switch (cipherType_) {
    case AES_128_CFB:
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_cfb(), NULL,
                key, iv_)) {
            fprintf(stderr, "CryptoTool: Init error.\n");
            exit(EXIT_FAILURE);
        }
        break;
    case AES_256_CFB:
        if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cfb(), NULL,
                key, iv_)) {
            fprintf(stderr, "CryptoTool: Init error.\n");
            exit(EXIT_FAILURE);
        }
        break;
    case AES_256_GCM:
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CRYPTO_BLOCK_SIZE, NULL);
        if (!EVP_EncryptInit_ex(ctx, NULL, NULL,
                key, iv_)) {
            fprintf(stderr, "CryptoTool: Init error.\n");
            exit(EXIT_FAILURE);
        }
        EVP_EncryptUpdate(ctx, NULL, &cipherLen, gcm_aad, sizeof(gcm_aad));
        break;
    case AES_128_GCM:
        EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CRYPTO_BLOCK_SIZE, NULL);
        if (!EVP_EncryptInit_ex(ctx, NULL, NULL,
                key, iv_)) {
            fprintf(stderr, "CryptoTool: Init error.\n");
            exit(EXIT_FAILURE);
        }
        EVP_EncryptUpdate(ctx, NULL, &cipherLen, gcm_aad, sizeof(gcm_aad));
        break;
    }

    // encrypt the plaintext
    if (!EVP_EncryptUpdate(ctx, ciphertext, &cipherLen, dataBuffer,
            dataSize)) {
        fprintf(stderr, "CryptoTool: Encryption error.\n");
        exit(EXIT_FAILURE);
    }
    EVP_EncryptFinal_ex(ctx, ciphertext + cipherLen, &len);
    cipherLen += len;

    if (cipherLen != dataSize) {
        fprintf(stderr, "CryptoTool: encryption output size not equal to origin size.\n");
        exit(EXIT_FAILURE);
    }

    EVP_CIPHER_CTX_reset(ctx);
    return;
}

/**
 * @brief Decrypt the ciphertext with the encryption key
 *
 * @param ctx cipher ctx
 * @param ciphertext ciphertext data buffer
 * @param dataSize input data size
 * @param key encryption key
 * @param dataBuffer output ciphertext
 */
void CryptoPrimitive::DecryptWithKey(EVP_CIPHER_CTX* ctx, uint8_t* ciphertext, const int dataSize,
    uint8_t* key, uint8_t* dataBuffer)
{
    int plainLen;
    int len;
    switch (cipherType_) {
    case AES_128_CFB:
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_cfb(), NULL,
                key, iv_)) {
            fprintf(stderr, "CryptoTool: Init error.\n");
            exit(EXIT_FAILURE);
        }
        break;
    case AES_256_CFB:
        if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cfb(), NULL,
                key, iv_)) {
            fprintf(stderr, "CryptoTool: Init error.\n");
            exit(EXIT_FAILURE);
        }
        break;
    case AES_128_GCM:
        EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CRYPTO_BLOCK_SIZE, NULL);
        if (!EVP_DecryptInit_ex(ctx, NULL, NULL,
                key, iv_)) {
            fprintf(stderr, "CryptoTool: Init error.\n");
            exit(EXIT_FAILURE);
        }
        EVP_DecryptUpdate(ctx, NULL, &plainLen, gcm_aad, sizeof(gcm_aad));
        break;
    case AES_256_GCM:
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CRYPTO_BLOCK_SIZE, NULL);
        if (!EVP_DecryptInit_ex(ctx, NULL, NULL,
                key, iv_)) {
            fprintf(stderr, "CryptoTool: Init error.\n");
            exit(EXIT_FAILURE);
        }
        EVP_DecryptUpdate(ctx, NULL, &plainLen, gcm_aad, sizeof(gcm_aad));
        break;
    }

    // decrypt the plaintext
    if (!EVP_DecryptUpdate(ctx, dataBuffer, &plainLen, ciphertext,
            dataSize)) {
        fprintf(stderr, "CryptoTool: Decrypt error.\n");
        exit(EXIT_FAILURE);
    }

    EVP_DecryptFinal_ex(ctx, dataBuffer + plainLen, &len);

    plainLen += len;

    if (plainLen != dataSize) {
        fprintf(stderr, "CryptoTool: Decrypt output size not equal to origin size.\n");
        exit(EXIT_FAILURE);
    }

    EVP_CIPHER_CTX_reset(ctx);
    return;
}

/**
 * @brief encrypt for secure communication
 *
 * @param ctx cipher ctx
 * @param dataBuffer input data buffer
 * @param dataSize input data size
 * @param key session key
 * @param ciphertext output ciphertext
 */
void CryptoPrimitive::SessionKeyEnc(EVP_CIPHER_CTX* ctx, uint8_t* dataBuffer, const int dataSize,
    uint8_t* sessionKey, uint8_t* ciphertext)
{
    int cipherLen = 0;
    int len = 0;

    // for SSL/TLS
    EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CRYPTO_BLOCK_SIZE, NULL);
    if (!EVP_EncryptInit_ex(ctx, NULL, NULL,
            sessionKey, iv_)) {
        fprintf(stderr, "CryptoTool: Init error.\n");
        exit(EXIT_FAILURE);
    }
    EVP_EncryptUpdate(ctx, NULL, &cipherLen, gcm_aad, sizeof(gcm_aad));

    // encrypt the plaintext
    if (!EVP_EncryptUpdate(ctx, ciphertext, &cipherLen, dataBuffer,
            dataSize)) {
        fprintf(stderr, "CryptoTool: Encryption error.\n");
        exit(EXIT_FAILURE);
    }
    EVP_EncryptFinal_ex(ctx, ciphertext + cipherLen, &len);
    cipherLen += len;

    if (cipherLen != dataSize) {
        fprintf(stderr, "CryptoTool: encryption output size not equal to original size.\n");
        exit(EXIT_FAILURE);
    }

    EVP_CIPHER_CTX_reset(ctx);
    return;
}

/**
 * @brief decrypt for secure communication
 *
 * @param ctx cipher ctx
 * @param ciphertext ciphertext data buffer
 * @param dataSize input data size
 * @param sessionKey session key
 * @param dataBuffer output plaintext
 */
void CryptoPrimitive::SessionKeyDec(EVP_CIPHER_CTX* ctx, uint8_t* ciphertext, const int dataSize,
    uint8_t* sessionKey, uint8_t* dataBuffer)
{
    int plainLen;
    int len;

    // for SSL/TLS
    EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, CRYPTO_BLOCK_SIZE, NULL);
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL,
            sessionKey, iv_)) {
        fprintf(stderr, "CryptoTool: Init error.\n");
        exit(EXIT_FAILURE);
    }
    EVP_DecryptUpdate(ctx, NULL, &plainLen, gcm_aad, sizeof(gcm_aad));

    // decrypt the ciphertext
    if (!EVP_DecryptUpdate(ctx, dataBuffer, &plainLen, ciphertext,
            dataSize)) {
        fprintf(stderr, "CryptoTool: Decrypt error.\n");
        exit(EXIT_FAILURE);
    }
    EVP_DecryptFinal_ex(ctx, dataBuffer + plainLen, &len);
    plainLen += len;

    if (plainLen != dataSize) {
        EVP_CIPHER_CTX_reset(ctx);
        exit(EXIT_FAILURE);
    }

    EVP_CIPHER_CTX_reset(ctx);
    return;
}