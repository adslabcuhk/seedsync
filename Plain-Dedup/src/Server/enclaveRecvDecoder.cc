/**
 * @file enclaveRecvDecoder.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the interface of enclave-based recv decoder
 * @version 0.1
 * @date 2021-02-28
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "../../include/enclaveRecvDecoder.h"

/**
 * @brief Construct a new Enclave Recv Decoder object
 * 
 * @param fileName2metaDB the file recipe index
 * @param sslConnection the ssl connection pointer
 * @param eidSGX the id to the enclave
 */
EnclaveRecvDecoder::EnclaveRecvDecoder(SSLConnection* sslConnection,
    sgx_enclave_id_t eidSGX) : AbsRecvDecoder(sslConnection) {
    eidSGX_ = eidSGX;
    Ecall_Init_Restore(eidSGX_);

    cryptoObj_ = new CryptoPrimitive(CIPHER_TYPE, HASH_TYPE, HMAC_TYPE);
    cipherCtx_ = EVP_CIPHER_CTX_new();
    mdCtx_ = EVP_MD_CTX_new();

    tool::Logging(myName_.c_str(), "init the EnclaveRecvDecoder.\n");
}

/**
 * @brief Destroy the Enclave Recv Decoder object
 * 
 */
EnclaveRecvDecoder::~EnclaveRecvDecoder() {
    fprintf(stderr, "========EnclaveRecvDecoder Info========\n");
    fprintf(stderr, "read container from file num: %lu\n", readFromContainerFileNum_);

    EVP_CIPHER_CTX_free(cipherCtx_);
    EVP_MD_CTX_free(mdCtx_);
    delete cryptoObj_;

    fprintf(stderr, "=======================================\n");
}

/**
 * @brief the main process
 * 
 * @param outClient the out-enclave client ptr
 */
void EnclaveRecvDecoder::Run(ClientVar* outClient) {
    tool::Logging(myName_.c_str(), "the main thread is running.\n");
    SSL* clientSSL = outClient->_clientSSL;
    ResOutSGX_t* resOutSGX = &outClient->_resOutSGX;

    uint8_t* readRecipeBuf = outClient->_readRecipeBuf;
    SendMsgBuffer_t* sendChunkBuf = &outClient->_sendChunkBuf;
    uint32_t recvSize = 0;

    if (!dataSecureChannel_->ReceiveData(clientSSL, sendChunkBuf->sendBuffer,
        recvSize)) {
        tool::Logging(myName_.c_str(), "recv the client ready error.\n");
        exit(EXIT_FAILURE);
    } else {
        if (sendChunkBuf->header->messageType != CLIENT_RESTORE_READY) {
            tool::Logging(myName_.c_str(), "wrong type of client ready reply.\n");
            exit(EXIT_FAILURE);
        }
    }

    struct timeval sProcTime;
    struct timeval eProcTime;
    double totalProcTime = 0;
    tool::Logging(myName_.c_str(), "start to read the file recipe.\n");
    gettimeofday(&sProcTime, NULL);

    // outClient->_recipeReadHandler.clear();
    // outClient->_recipeReadHandler.seekg(0, ios_base::beg);
    cout<<"current position in file recipe "<<outClient->_recipeReadHandler.tellg();

    bool end = false;
    while (!end) {
        // read a batch of the recipe entries from the recipe file
        outClient->_recipeReadHandler.read((char*)readRecipeBuf, 
            sizeof(RecipeEntry_t) * sendRecipeBatchSize_);
        size_t readCnt = outClient->_recipeReadHandler.gcount();
        // cout<<"read cnt "<<readCnt<<endl;
        end = outClient->_recipeReadHandler.eof();
        size_t recipeEntryNum = readCnt / sizeof(RecipeEntry_t);
        // cout<<"recipe entry num "<<recipeEntryNum<<endl;
        if (readCnt == 0) {
            break;
        }

        uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
        cryptoObj_->GenerateHMAC(readRecipeBuf,
            sizeof(RecipeEntry_t) * sendRecipeBatchSize_, check_hash);
        // cout<<"checkhash "<<endl;
        // tool::PrintBinaryArray(check_hash, CHUNK_HASH_HMAC_SIZE);

        totalRestoreRecipeNum_ += recipeEntryNum;
        Ecall_ProcRecipeBatch(eidSGX_, readRecipeBuf, recipeEntryNum, 
            resOutSGX);
    }

    Ecall_ProcRecipeTailBatch(eidSGX_, resOutSGX);

    // wait the close connection request from the client
    string clientIP;
    if (!dataSecureChannel_->ReceiveData(clientSSL, sendChunkBuf->sendBuffer, 
        recvSize)) {
        tool::Logging(myName_.c_str(), "client closed socket connect, thread exit now.\n");
        dataSecureChannel_->GetClientIp(clientIP, clientSSL);
        dataSecureChannel_->ClearAcceptedClientSd(clientSSL);
    }
    gettimeofday(&eProcTime, NULL);

    tool::Logging(myName_.c_str(), "all job done for %s, thread exits now.\n", clientIP.c_str());
    totalProcTime += tool::GetTimeDiff(sProcTime, eProcTime);

    return ;
}

/**
 * @brief Get the Required Containers object 
 * 
 * @param outClient the out-enclave client ptr
 */
void EnclaveRecvDecoder::GetReqContainers(ClientVar* outClient) {
    ReqContainer_t* reqContainer = &outClient->_reqContainer;
    uint8_t* idBuffer = reqContainer->idBuffer; 
    uint8_t** containerArray = reqContainer->containerArray;
    ReadCache* containerCache = outClient->_containerCache;
    uint32_t idNum = reqContainer->idNum; 

    // retrieve each container
    string containerNameStr;
    for (size_t i = 0; i < idNum; i++) {
        containerNameStr.assign((char*) (idBuffer + i * CONTAINER_ID_LENGTH), 
            CONTAINER_ID_LENGTH);
        // step-1: check the container cache
        bool cacheHitStatus = containerCache->ExistsInCache(containerNameStr);
        if (cacheHitStatus) {
            // step-2: exist in the container cache, read from the cache, directly copy the data from the cache
#if (RAW_CONTAINER == 1)
            memcpy(containerArray[i], containerCache->ReadFromCache(containerNameStr), 
                MAX_CONTAINER_SIZE);
#endif

#if (CONTAINER_META_SESSION == 1)
            memcpy(containerArray[i], containerCache->ReadFromCache(containerNameStr), 
                MAX_CONTAINER_SIZE);    
#endif
            continue ;
        } 

        // step-3: not exist in the contain cache, read from disk
        ifstream containerIn;
        string readFileNameStr = containerNamePrefix_ + containerNameStr + containerNameTail_;
        containerIn.open(readFileNameStr, ifstream::in | ifstream::binary);

        if (!containerIn.is_open()) {
            tool::Logging(myName_.c_str(), "cannot open the container: %s\n", readFileNameStr.c_str());
            exit(EXIT_FAILURE);
        }

        // get the data section size (total chunk size - metadata section)
        containerIn.seekg(0, ios_base::end);
        int readSize = containerIn.tellg();
        containerIn.seekg(0, ios_base::beg);

        // read the metadata section
        int containerSize = 0;
        containerSize = readSize;
        // read compression data
        // problem here ///////////////////////////////////////////////////////////////////////////////
        // cout<<"read container size "<<containerSize<<endl;
        containerIn.read((char*)containerArray[i], containerSize);

        // // debug
        // uint32_t check_metasize;
        // memcpy((char*)&check_metasize, (char*)containerArray[i], sizeof(uint32_t));
        // size_t check_num = check_metasize / (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
        // uint32_t read_meta_offset = SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t) + sizeof(uint32_t);
        // uint64_t check_features[SUPER_FEATURE_PER_CHUNK];
        // uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
        // for (size_t k = 0; k < check_num; k++) {
        //     memcpy(check_hash, containerArray[i] + read_meta_offset, CHUNK_HASH_HMAC_SIZE);

        //     size_t read_feature_offset = read_meta_offset - SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);
        //     memcpy((char*)check_features, (char*)containerArray[i] + read_feature_offset, SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));

        //     read_meta_offset += CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t);

        //     // // uint64_t check_feature[SUPER_FEATURE_PER_CHUNK];
        //     // // memcpy((char*)check_feature, (char*)containerArray[i] + sizeof(uint32_t) + k * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)), SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t));
        //     // cout<<"feature: ";
        //     // for (size_t j = 0; j < SUPER_FEATURE_PER_CHUNK; j++) {
        //     //     tool::Logging(myName_.c_str(), "%d\t", check_features[j]);
        //     // }
        //     // cout<<endl;

        //     // // uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
        //     // // memcpy(check_hash, containerArray[i] + sizeof(uint32_t) + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)
        //     // //     + k * (CHUNK_HASH_HMAC_SIZE + SUPER_FEATURE_PER_CHUNK * sizeof(uint64_t)), CHUNK_HASH_HMAC_SIZE);
        //     // tool::PrintBinaryArray(check_hash, CHUNK_HASH_HMAC_SIZE);
        // }

        if (containerIn.gcount() != containerSize) {
            tool::Logging(myName_.c_str(), "read size %lu cannot match expected size: %d for container %s.\n",
                containerIn.gcount(), containerSize, readFileNameStr.c_str());
            exit(EXIT_FAILURE);
        } 

        // close the container file
        containerIn.close();
        readFromContainerFileNum_++;
        containerCache->InsertToCache(containerNameStr, containerArray[i], containerSize);
    }
    return ;
}

/**
 * @brief Get the Meta Containers object
 * 
 * @param outClient 
 */
void EnclaveRecvDecoder::GetMetaContainers(ClientVar* outClient) {
    ReqContainer_t* reqContainer = &outClient->_reqContainer;
    uint8_t* idBuffer = reqContainer->idBuffer; 
    uint8_t** containerArray = reqContainer->containerArray;
    // ReadCache* containerCache = outClient->_containerCache;
    uint32_t idNum = reqContainer->idNum; 

    // retrieve each container
    string containerNameStr;
    for (size_t i = 0; i < idNum; i++) {
        containerNameStr.assign((char*) (idBuffer + i * CONTAINER_ID_LENGTH), 
            CONTAINER_ID_LENGTH);
        // // step-1: check the container cache
        // bool cacheHitStatus = containerCache->ExistsInCache(containerNameStr);
        // if (cacheHitStatus) {
        //     // step-2: exist in the container cache, read from the cache, directly copy the data from the cache
        //     memcpy(containerArray[i], containerCache->ReadFromCache(containerNameStr), 
        //         MAX_CONTAINER_SIZE);
        //     continue ;
        // } 

        // step-3: not exist in the contain cache, read from disk
        ifstream containerIn;
        string readFileNameStr = containerNamePrefix_ + containerNameStr + containerNameTail_;
        containerIn.open(readFileNameStr, ifstream::in | ifstream::binary);

        if (!containerIn.is_open()) {
            tool::Logging(myName_.c_str(), "cannot open the container: %s\n", readFileNameStr.c_str());
            exit(EXIT_FAILURE);
        }

        // get the data section size (total chunk size - metadata section)
        containerIn.seekg(0, ios_base::end);
        int readSize = containerIn.tellg();
        containerIn.seekg(0, ios_base::beg);

        // read the metadata section
        uint32_t metaSize = 0;

        int containerSize = 0;
        containerSize = readSize;
        // read compression data
        containerIn.read((char*)containerArray[i], containerSize);

        if (containerIn.gcount() != containerSize) {
            tool::Logging(myName_.c_str(), "read size %lu cannot match expected size: %d for container %s.\n",
                containerIn.gcount(), containerSize, readFileNameStr.c_str());
            exit(EXIT_FAILURE);
        } 

        // close the container file
        containerIn.close();
        readFromContainerFileNum_++;
        // containerCache->InsertToCache(containerNameStr, containerArray[i], containerSize);
    }
    return ;    
}


/**
 * @brief send the restore chunk to the client
 * 
 * @param sendChunkBuf the send chunk buffer
 * @param clientSSL the ssl connection
 */
void EnclaveRecvDecoder::SendBatchChunks(SendMsgBuffer_t* sendChunkBuf, 
    SSL* clientSSL) {
    if (!dataSecureChannel_->SendData(clientSSL, sendChunkBuf->sendBuffer, 
        sizeof(NetworkHead_t) + sendChunkBuf->header->dataSize)) {
        tool::Logging(myName_.c_str(), "send the batch of restored chunks error.\n");
        exit(EXIT_FAILURE);
    }
    return ;
}