/**
 * @file cryptoPrimitive.h
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief define interfaces of crypto module (hash&encryption)
 * @version 0.1
 * @date 2019-12-19
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#ifndef BASICDEDUP_CRYPTOPRIMITIVE_h
#define BASICDEDUP_CRYPTOPRIMITIVE_h

#include "chunkStructure.h"
#include "configure.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

using namespace std;

static const unsigned char gcm_aad[] = {
    0x4d, 0x23, 0xc3, 0xce, 0xc3, 0x34, 0xb4, 0x9b, 0xdb, 0x37, 0x0c, 0x43,
    0x7f, 0xec, 0x78, 0xde
};

class CryptoPrimitive {
    private:
        // the type of cipher
        ENCRYPT_SET cipherType_;
        // the type of hash
        HASH_SET hashType_;
        // initialized vector
        uint8_t* iv_;
        uint8_t* hmacKey_;
        HMAC_CTX* hmacCtx_;
        EVP_MD_CTX* mdCtx_;

    public:
        /**
         * @brief Construct a new Crypto Primitive object
         * 
         * @param cipherType 
         * @param hashType 
         */
        CryptoPrimitive(int cipherType, int hashType);

        /**
         * @brief Destroy the Crypto Primitive object
         * 
         */
        ~CryptoPrimitive();

        /**
         * @brief Generate the hash of the input data
         * 
         * @param mdCtx hasher ctx
         * @param dataBuffer input data buffer
         * @param dataSize input data size 
         * @param hash output hash 
         */
        void GenerateHash(uint8_t* dataBuffer, const int dataSize, uint8_t* hash);

        /**
         * @brief Generate the hmac of the input data with default HMAC key
         * 
         * @param dataBuffer input data buffer
         * @param dataSize input data size 
         * @param hmac output hmac 
         */
        void GenerateHMAC(uint8_t* dataBuffer, const int dataSize, uint8_t* hmac);

        /**
         * @brief Encrypt the data with the encryption key 
         * 
         * @param ctx cipher ctx
         * @param dataBuffer input data buffer
         * @param dataSize input data size 
         * @param key encryption key 
         * @param ciphertext output cipherText 
         */
        void EncryptWithKey(EVP_CIPHER_CTX* ctx, uint8_t* dataBuffer, const int dataSize, 
            uint8_t* key, uint8_t* ciphertext);

        /**
         * @brief Decrypt the ciphertext with the encryption key
         * 
         * @param ctx cipher ctx
         * @param ciphertext ciphertext data buffer
         * @param dataSize input data size
         * @param key encryption key 
         * @param dataBuffer output ciphertext 
         */
        void DecryptWithKey(EVP_CIPHER_CTX* ctx, uint8_t* ciphertext, const int dataSize, 
            uint8_t* key, uint8_t* dataBuffer);

        /**
         * @brief encrypt for secure communication
         * 
         * @param ctx cipher ctx 
         * @param dataBuffer input data buffer
         * @param dataSize input data size
         * @param key session key
         * @param ciphertext output ciphertext
         */
        void SessionKeyEnc(EVP_CIPHER_CTX* ctx, uint8_t* dataBuffer, const int dataSize, 
            uint8_t* sessionKey, uint8_t* ciphertext);

        /**
         * @brief decrypt for secure communication
         * 
         * @param ctx cipher ctx
         * @param ciphertext ciphertext data buffer
         * @param dataSize input data size
         * @param sessionKey session key
         * @param dataBuffer output plaintext 
         */
        void SessionKeyDec(EVP_CIPHER_CTX* ctx, uint8_t* ciphertext, const int dataSize, 
            uint8_t* sessionKey, uint8_t* dataBuffer);

};

#endif //BASICDEDUP_CRYPTOPRIMITIVE_h
