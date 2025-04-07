/**
 * @file constVar.h
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief define the const variables 
 * @version 0.1
 * @date 2020-12-10
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#ifndef CONST_VAR_H
#define CONST_VAR_H

#include <stdint.h>

// for enclave lib path
static const char ENCLAVE_PATH[] = "../lib/storeEnclave.signed.so";

// For out-enclave breakdown
#define CHUNKING_BREAKDOWN 0
#define DATAWRITER_BREAKDOWN 0
#define RESTORE_WRITER_BREAKDOWN 0
#define RECIPE_HMAC 0
#define RECIPE_HMAC_PERSIST 0
#define DEBUG_FLAG 0
#define EXTRA_HASH_FUNCTION 1

// For SGX_Breakdown
#define SGX_BREAKDOWN 0
#define IMPACT_OF_TOP_K 0
#define MULTI_CLIENT 0

#define INDEX_ENC 1

// the length of the hash
enum ENCRYPT_SET { AES_256_GCM = 0,
    AES_128_GCM = 1,
    AES_256_CFB = 2,
    AES_128_CFB = 3 };
enum HASH_SET { SHA_256 = 0,
    MD5 = 1,
    SHA_1 = 2 };
static const uint32_t CRYPTO_BLOCK_SIZE = 16;
// static const uint32_t CHUNK_HASH_SIZE = 32;
#define CHUNK_HASH_SIZE (uint32_t)32
static const uint32_t CHUNK_ENCRYPT_KEY_SIZE = 32;
static const uint32_t HASH_TYPE = SHA_256;
static const uint32_t CIPHER_TYPE = AES_256_GCM;

// the size of chunk
static const uint32_t MIN_CHUNK_SIZE = 4096;
static const uint32_t AVG_CHUNK_SIZE = 8192;
// static const uint32_t MAX_CHUNK_SIZE = 16384;
#define MAX_CHUNK_SIZE (uint32_t)16384

// the size of segment
#define MAX_SEG_SIZE (uint32_t)2097152

// the size of sync segment
static const uint32_t SYNC_MAX_SEG_SIZE = 1024 * 1024;
static const uint32_t SYNC_SEG_CHUNK_NUM = 256;

// the size of segment
static const uint32_t AVG_SEGMENT_SIZE = 1024 * 1024 * 10; // 10MB default
static const uint32_t MIN_SEGMENT_SIZE = AVG_SEGMENT_SIZE / 2; // 5MB default
static const uint32_t MAX_SEGMENT_SIZE = AVG_SEGMENT_SIZE * 2; // 20MB default
static const uint32_t DIVISOR = ((AVG_SEGMENT_SIZE - MIN_SEGMENT_SIZE) / AVG_CHUNK_SIZE);
static const uint32_t PATTERN = 1;

// the type of chunker
enum CHUNKER_TYPE { FIXED_SIZE_CHUNKING = 0,
    FAST_CDC,
    LOCALITY_FASTCDC,
    FSL_TRACE,
    UBC_TRACE };

// the setting of the container
// static const uint32_t MAX_CONTAINER_SIZE = 1 << 22; // container size: 4MB
// static const uint32_t CONTAINER_ID_LENGTH = 8;
// static const uint32_t SEGMENT_ID_LENGTH = 16;
#define MAX_CONTAINER_SIZE (uint32_t)1 << 22 // container size: 4MB
#define MAX_META_SIZE (uint32_t)(1024 * (CHUNK_HASH_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + sizeof(uint32_t) * 2))
#define MAX_CONTAINER_SIZE_WITH_META (uint32_t)((MAX_CONTAINER_SIZE) + (MAX_META_SIZE))
#define CONTAINER_ID_LENGTH (uint32_t)8
#define SEGMENT_ID_LENGTH (uint32_t)16

// for raw container storage with data only
#define RAW_CONTAINER 0
// for container storage with metadata session
#define CONTAINER_META_SESSION 1

// define the data type of the MQ
enum DATA_TYPE_SET { DATA_CHUNK = 0,
    RECIPE_END,
    DATA_SEGMENT_END_FLAG };

// configure for sparse index
static const uint32_t SPARSE_SAMPLE_RATE = 6;
static const uint32_t SPARSE_CHAMPION_NUM = 10;
static const uint32_t SPARSE_MANIFIEST_CAP_NUM = 10;

enum INDEX_TYPE_SET { OUT_ENCLAVE = 0,
    IN_ENCLAVE,
    EXTREME_BIN,
    SPARSE_INDEX,
    FREQ_INDEX };

enum SSL_CONNECTION_TYPE { IN_SERVERSIDE = 0,
    IN_CLIENTSIDE };

// for SSL connection
static const char SERVER_CERT[] = "../key/server/server.crt";
static const char SERVER_KEY[] = "../key/server/server.key";
static const char CLIENT_CERT[] = "../key/client/client.crt";
static const char CLIENT_KEY[] = "../key/client/client.key";
static const char CA_CERT[] = "../key/ca/ca.crt";

// for network message protocol code
enum PROTCOL_CODE_SET { CLIENT_UPLOAD_CHUNK = 0,
    CLIENT_UPLOAD_SEGMENT_END,
    CLIENT_UPLOAD_RECIPE_END,
    CLIENT_LOGIN_UPLOAD,
    CLIENT_LOGIN_DOWNLOAD,
    CLIENT_RESTORE_READY,
    SERVER_RESTORE_CHUNK,
    SERVER_RESTORE_FINAL,
    SERVER_LOGIN_RESPONSE,
    SERVER_FILE_NON_EXIST,
    SGX_RA_MSG01,
    SGX_RA_MSG2,
    SGX_RA_MSG3,
    SGX_RA_MSG4,
    SGX_RA_NEED,
    SGX_RA_NOT_NEED,
    SGX_RA_NOT_SUPPORT,
    SESSION_KEY_INIT,
    SESSION_KEY_REPLY };

static const uint32_t CHUNK_QUEUE_SIZE = 8192;
static const uint32_t LARGER_QUEUE_SIZE = 16384;
static const uint32_t CONTAINER_QUEUE_SIZE = 32;
static const uint32_t CONTAINER_CAPPING_VALUE = 16;

static const uint32_t SGX_PERSISTENCE_BUFFER_SIZE = 2 * 1024 * 1024;

enum TWO_PATH_STATUS { UNIQUE = 0,
    TMP_UNIQUE = 1,
    DUPLICATE = 2,
    TMP_DUPLICATE = 3 };

enum ENCLAVE_TRUST_STATUS { ENCLAVE_TRUSTED = 0,
    ENCLAVE_UNTRUSTED = 1 };

static const uint32_t MAX_SGX_MESSAGE_SIZE = 4 * 1024;

#define ENABLE_SGX_RA 0
#define TEST_IN_CSE 0

static const uint32_t THREAD_STACK_SIZE = 8 * 1024 * 1024;
static const uint32_t SESSION_KEY_BUFFER_SIZE = 65;

enum OPT_TYPE { UPLOAD_OPT = 0,
    DOWNLOAD_OPT,
    RA_OPT };

enum LOCK_TYPE { SESSION_LCK_WRITE = 0,
    SESSION_LCK_READ,
    TOP_K_LCK_WRITE,
    TOP_K_LCK_READ };

// for sync protocol
enum SYNC_OPT_TYPE { SYNC_OPT = 0,
    WAIT_OPT };

enum SYNC_PROTOCOL_SET { SYNC_LOGIN = 0,
    SYNC_LOGIN_RESPONSE,
    SYNC_FILE_NAME,
    SYNC_FILE_NAME_END_FLAG,
    SYNC_UNI_FILE_NAME,
    SYNC_UNI_FILE_NAME_END_FLAG,
    SYNC_SEG_FP,
    SYNC_SEG_FP_END_FLAG,
    SYNC_SEG_FP_END_FLAG_COND,
    FILE_END_SEG_FP,
    SYNC_UNI_SEG_FP,
    SYNC_UNI_SEG_FP_END_FLAG,
    FILE_END_UNI_SEG_FP,
    SYNC_CHUNK_FP,
    SYNC_CHUNK_FP_END_FLAG,
    FILE_END_CHUNK_FP,
    SYNC_UNI_CHUNK_FP,
    SYNC_UNI_CHUNK_FP_END_FLAG,
    FILE_END_UNI_CHUNK_FP,
    SYNC_FEATURE,
    SYNC_FEATURE_END,
    FILE_END_FEATURE,
    SYNC_LOCAL_DELTA,
    FILE_END_LOCAL_DELTA,
    SYNC_BASE_HASH,
    SYNC_BASE_HASH_END_FLAG,
    FILE_END_BASE_HASH,
    SYNC_DATA,
    FILE_END_SYNC_DATA,
    SYNC_DATA_END_FLAG,
    SYNC_FILE_START,
    SYNC_FILE_END };

static const uint32_t MAX_MSG_TYPE_IN_BATCH = 2;

static const uint64_t MAX_SEG_INDEX_ENTRY_NUM = 1024 * 32;
static const uint64_t MAX_CHUNK_INDEX_ENTRY_NUM = MAX_SEG_INDEX_ENTRY_NUM / SYNC_SEG_CHUNK_NUM;

enum HIT_TYPE { NOT_HIT = 0,
    HIT };

enum MEMORY_POOL_TYPE { META_POOL = 0,
    DATA_POOL };

static const uint32_t META_POOL_SIZE = 64; // the number of meta batches in memory pool
static const uint32_t DATA_POOL_SIZE = 16; // the number of data batches in memory pool
static const uint32_t SYNC_SEND_MQ_SIZE = META_POOL_SIZE + DATA_POOL_SIZE;

// sketch & super-feature & feature settings
// static const uint32_t SUPER_FEATURE_PER_CHUNK = 3; // 3 super-feature per chunk
// static const uint32_t FEATURE_PER_SUPER_FEATURE = 4; // 4 features per super-feature
// static const uint32_t FEATURE_PER_CHUNK = SUPER_FEATURE_PER_CHUNK *
// FEATURE_PER_SUPER_FEATURE; // 12 total features per chunk
#define SUPER_FEATURE_PER_CHUNK (uint32_t)3
#define FEATURE_PER_SUPER_FEATURE (uint32_t)4
#define FEATURE_PER_CHUNK (uint32_t) SUPER_FEATURE_PER_CHUNK* FEATURE_PER_SUPER_FEATURE
static const uint64_t SLIDING_WIN_SIZE = 48;

enum SIM_STATUS { ENCLAVE_SIMILAR_CHUNK = 0,
    SIMILAR_CHUNK,
    BATCH_SIMILAR_CHUNK,
    BASE_CHUNK,
    ENCLAVE_NON_SIMILAR_CHUNK,
    NON_SIMILAR_CHUNK,
    DELTA_COMP_CHUNK,
    DELTA_ONLY_CHUNK,
    COMP_ONLY_CHUNK };

static const uint32_t REALLOCATE_UNIT = 64;

// static const uint8_t IV_SIZE = 16;
#define IV_SIZE (uint8_t)16

enum IS_SYNC_FILE_END { FILE_END = 0,
    NOT_FILE_END };

static const uint8_t INDEX_TMP_NULL_VALUE = 1;

#endif