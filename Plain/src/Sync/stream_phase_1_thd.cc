/**
 * @file stream_phase_1_thd.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief
 * @version 0.1
 * @date 2024-05-23
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../../include/stream_phase_1_thd.h"

struct timeval stream1_stime;
struct timeval stream1_etime;

/**
 * @brief Construct a new Stream Phase 1 Thd:: Stream Phase 1 Thd object
 *
 * @param phase_sender_obj
 * @param eid_sgx
 */
StreamPhase1Thd::StreamPhase1Thd(PhaseSender* phase_sender_obj)
{
    // send_thd_obj_ = send_thd_obj;
    phase_sender_obj_ = phase_sender_obj;
    // eid_sgx_ = eid_sgx;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * CHUNK_HASH_HMAC_SIZE * sizeof(uint8_t));
    send_batch_buf_.header = (NetworkHead_t*)send_batch_buf_.sendBuffer;
    send_batch_buf_.header->currentItemNum = 0;
    send_batch_buf_.header->dataSize = 0;
    send_batch_buf_.dataBuffer = send_batch_buf_.sendBuffer + sizeof(NetworkHead_t);

    // for recipe batch reading
    recipe_path_ = config.GetRecipeRootPath();
    read_recipe_buf_ = (uint8_t*)malloc(sizeof(RecipeEntry_t) * config.GetSendRecipeBatchSize());
    if (read_recipe_buf_ == NULL) {
        cout << "read recipe buf malloc fail" << endl;
    }

    // for debugging
    cryptoObj_ = new CryptoPrimitive(CIPHER_TYPE, HASH_TYPE, HMAC_TYPE);
    cipherCtx_ = EVP_CIPHER_CTX_new();
    mdCtx_ = EVP_MD_CTX_new();

    tool::Logging(my_name_.c_str(), "init StreamPhase1Thd.\n");
}

/**
 * @brief Destroy the Stream Phase 1 Thd object
 *
 */
StreamPhase1Thd::~StreamPhase1Thd()
{
    free(send_batch_buf_.sendBuffer);
    free(read_recipe_buf_);

    EVP_CIPHER_CTX_free(cipherCtx_);
    EVP_MD_CTX_free(mdCtx_);
    delete cryptoObj_;
}

/**
 * @brief start the sync request by sending chunk list
 *
 */
void StreamPhase1Thd::SyncRequest()
{
    // traverse the recipe path, load the recipe file in batches

    std::multimap<std::time_t, fs::path> sorted_files;

    for (const auto& entry : fs::directory_iterator(recipe_path_)) {

        if (fs::is_regular_file(entry)) {
            string filenamestr; // length = CHUNK_HASH_HMAC_SIZE * 2
            filenamestr.assign(entry.path().filename().c_str(),
                CHUNK_HASH_HMAC_SIZE * 2);

            // cout <<"traverse "<< filenamestr << endl;

            // insert multimap
            sorted_files.insert({ fs::last_write_time(entry), entry.path() });
        }
    }

    // get the sorted file list
    for (const auto& file : sorted_files) {
        string recipe_name;
        recipe_name.assign(file.second.filename().c_str(), CHUNK_HASH_HMAC_SIZE * 2);
        // process one recipe to send the batches of chunk FP list
        ProcessOneRecipe(recipe_name);
    }

    return;
}

/**
 * @brief process one file recipe
 *
 * @param recipe_name
 */
void StreamPhase1Thd::ProcessOneRecipe(string recipe_name)
{

    gettimeofday(&stream1_stime, NULL);

    // do ecall to read the recipe and send the encrypted FP list in batches

    // read the recipe file
    string recipe_path = config.GetRecipeRootPath() + recipe_name
        + config.GetRecipeSuffix();
    // cout<<"recipe path "<<recipe_path<<endl;

    ifstream recipe_hdl;
    recipe_hdl.open(recipe_path, ios_base::in | ios_base::binary);
    if (!recipe_hdl.is_open()) {
        tool::Logging(my_name_.c_str(), "open recipe file failed %s.\n", recipe_path.c_str());
        exit(EXIT_FAILURE);
    }

    // read the recipe file in batches
    FileRecipeHead_t skip_head;
    recipe_hdl.read((char*)&skip_head, sizeof(FileRecipeHead_t));

    bool file_end = false;
    uint32_t processedRecipeBatchIndex = 0;
    while (!file_end) {
        recipe_hdl.read((char*)read_recipe_buf_,
            config.GetSendRecipeBatchSize() * sizeof(RecipeEntry_t));
        size_t read_cnt = recipe_hdl.gcount();
        // cout<<"read cnt"<<read_cnt<<endl;
        file_end = recipe_hdl.eof();
        size_t recipe_entry_num = read_cnt / sizeof(RecipeEntry_t);
        // cout<<"read entry num"<<recipe_entry_num<<endl;

        if (read_cnt == 0) {
            tool::Logging(my_name_.c_str(), "read cnt = 0.\n");
            break;
        }
        // uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
        // cryptoObj_->GenerateHMAC(mdCtx_, read_recipe_buf_, read_cnt, check_hash);
        // cout<<"check hash"<<endl;
        // tool::PrintBinaryArray(check_hash, CHUNK_HASH_HMAC_SIZE);
        // cout<<"check hash"<<endl;

        // string master_key_ = "12345";
        // uint8_t dec_key[CHUNK_HASH_HMAC_SIZE];
        // uint8_t* plain_recipe_buf_;
        // plain_recipe_buf_ = (uint8_t*) malloc(config.GetSendRecipeBatchSize() * sizeof(RecipeEntry_t));
        // RecipeEntry_t* tmp_recipe_entry;
        // tmp_recipe_entry = (RecipeEntry_t*)plain_recipe_buf_;
        // for (size_t j = 0; j < recipe_entry_num; j++) {
        //     // decrypt the recipe batch
        //     cryptoObj_->GenerateHMAC(mdCtx_, (uint8_t*)&master_key_[0], master_key_.size(),
        //         dec_key);

        //     cryptoObj_->DecryptWithKey(cipherCtx_, read_recipe_buf_,
        //         recipe_entry_num * sizeof(RecipeEntry_t), dec_key,
        //         plain_recipe_buf_);

        //     uint8_t check_chunk_hash[CHUNK_HASH_HMAC_SIZE];
        //     memcpy(check_chunk_hash, tmp_recipe_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);

        //     cout<<"offset "<<tmp_recipe_entry->offset<<endl;
        //     tool::PrintBinaryArray(check_chunk_hash, CHUNK_HASH_HMAC_SIZE);
        //     cout<<"len "<<tmp_recipe_entry->length<<endl;

        //     tmp_recipe_entry ++;
        // }

        // Ecall_Stream_Phase1_ProcessBatch(eid_sgx_, (uint8_t*)recipe_path.c_str(), processedRecipeBatchIndex,
        //     read_recipe_buf_, recipe_entry_num, send_batch_buf_.dataBuffer);
        ProcessPlainBatch((uint8_t*)recipe_path.c_str(), processedRecipeBatchIndex,
            read_recipe_buf_, recipe_entry_num, send_batch_buf_.dataBuffer);    
        
        processedRecipeBatchIndex++;
        // prepare the send buf
        send_batch_buf_.header->messageType = SYNC_CHUNK_FP;
        send_batch_buf_.header->currentItemNum = recipe_entry_num;
        send_batch_buf_.header->dataSize = recipe_entry_num * CHUNK_HASH_HMAC_SIZE;
        // this->SendBatch(send_batch_buf_);
        phase_sender_obj_->SendBatch(&send_batch_buf_);

        // reset the send buf
        send_batch_buf_.header->currentItemNum = 0;
        send_batch_buf_.header->dataSize = 0;
    }

    // reach recipe end
    send_batch_buf_.header->messageType = FILE_END_CHUNK_FP;
    // this->SendBatch(send_batch_buf_);
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    recipe_hdl.close();

    gettimeofday(&stream1_etime, NULL);

    stream1_process_time_ += tool::GetTimeDiff(stream1_stime, stream1_etime);
    cout << "stream1 process time " << stream1_process_time_ << endl;

    return;
}

/**
 * @brief sync a single file
 *
 * @param file_name
 */
void StreamPhase1Thd::SyncRequest(string& file_name)
{
    int client_id = 1;
    string full_name = file_name + to_string(client_id);
    cout << full_name << endl;
    uint8_t file_name_hash[CHUNK_HASH_HMAC_SIZE] = { 0 };
    cryptoObj_->GenerateHMAC((uint8_t*)&full_name[0],
        full_name.size(), file_name_hash);

    tool::PrintBinaryArray(file_name_hash, CHUNK_HASH_HMAC_SIZE);

    char file_hash_buf[CHUNK_HASH_HMAC_SIZE * 2 + 1];
    for (uint32_t i = 0; i < CHUNK_HASH_HMAC_SIZE; i++) {
        sprintf(file_hash_buf + i * 2, "%02x", file_name_hash[i]);
    }
    string recipe_name;
    recipe_name.assign(file_hash_buf, CHUNK_HASH_HMAC_SIZE * 2);

    cout << "recipe name " << recipe_name << endl;

    // string recipe_path = config.GetRecipeRootPath() + recipe_name
    //     + config.GetRecipeSuffix();

    // string fullName = inputFile + to_string(clientID);
    // uint8_t fileNameHash[CHUNK_HASH_HMAC_SIZE] = {0};
    // cryptoObj->GenerateHMAC(mdCtx, (uint8_t*)&fullName[0],
    //     fullName.size(), fileNameHash);

    // char fileHashBuf[CHUNK_HASH_HMAC_SIZE * 2 + 1];
    // for (uint32_t i = 0; i < CHUNK_HASH_HMAC_SIZE; i++) {
    //     sprintf(fileHashBuf + i * 2, "%02x", recvBuf.dataBuffer[i]);
    // }
    // string fileName;
    // fileName.assign(fileHashBuf, CHUNK_HASH_HMAC_SIZE * 2);
    // string recipePath = config.GetRecipeRootPath() +
    //     fileName + config.GetRecipeSuffix();

    ProcessOneRecipe(recipe_name);

    return;
}

/**
 * @brief process the plain batch
 * 
 * @param filename_buf 
 * @param currentBatchID 
 * @param recipe_buf 
 * @param recipe_num 
 * @param chunk_hash_buf 
 */
void StreamPhase1Thd::ProcessPlainBatch(uint8_t* filename_buf, uint32_t currentBatchID,
    uint8_t* recipe_buf, size_t recipe_num, uint8_t* chunk_hash_buf) {
    
    string recipe_path((char*)filename_buf);
    // std::string recipePath = recipe_path;
    // size_t lastSlashPos = recipePath.find_last_of('/');
    // if (lastSlashPos != std::string::npos) {
    //     recipePath = recipePath.substr(lastSlashPos + 1);
    // }
    // auto recipeHMACsVecIt = Enclave::fileRecipeNameToHMACIndex_.find(recipePath);
    // bool recipeHMACExist = false;
    // if (recipeHMACsVecIt == Enclave::fileRecipeNameToHMACIndex_.end()) {
    //     SyncEnclave::Logging(my_name_.c_str(), "Can not find recipe HMAC in metadata, target recipe path: %s.\n", recipePath.c_str());
    // } else {
    //     recipeHMACExist = true;
    // }

    // // SyncEnclave::Logging(my_name_.c_str(), "in ecallStreamChunkHash.\n");
    // // decrypt the recipe batch
    // uint8_t dec_key[CHUNK_HASH_HMAC_SIZE];
    // crypto_util_->GenerateHMAC((uint8_t*)&master_key_[0], master_key_.size(),
    //     dec_key);
    // // SyncEnclave::Logging("recipe_num ", "%d\n", recipe_num);

    // crypto_util_->DecryptWithKey(cipher_ctx_, recipe_buf,
    //     recipe_num * sizeof(RecipeEntry_t), dec_key,
    //     plain_recipe_buf_);

    // if (recipeHMACExist) {
    //     // Verify recipe batch HMAC
    //     uint8_t currentRecipeBatchHMACBuffer[CHUNK_HASH_HMAC_SIZE];
    //     crypto_util_->GenerateHMAC(plain_recipe_buf_, recipe_num * sizeof(RecipeEntry_t), currentRecipeBatchHMACBuffer);
    //     uint8_t storedRecipeBatchHMACBuffer[CHUNK_HASH_HMAC_SIZE];
    //     memcpy(storedRecipeBatchHMACBuffer, &((recipeHMACsVecIt->second)[currentBatchID][0]), CHUNK_HASH_HMAC_SIZE);
    //     if (memcmp(currentRecipeBatchHMACBuffer, storedRecipeBatchHMACBuffer, CHUNK_HASH_HMAC_SIZE) != 0) {
    //         SyncEnclave::Logging(my_name_.c_str(), "Read recipe batch ID: %ld, HMAC mismatch, stored HMAC: %s, computed HMAC: %s\n", currentBatchID, Enclave::convertBufferToHexString(storedRecipeBatchHMACBuffer, CHUNK_HASH_HMAC_SIZE).c_str(), Enclave::convertBufferToHexString(currentRecipeBatchHMACBuffer, CHUNK_HASH_HMAC_SIZE).c_str());
    //     }
    // }
    // uint8_t check_hash[CHUNK_HASH_HMAC_SIZE];
    // crypto_util_->GenerateHMAC( recipe_buf, recipe_num * sizeof(RecipeEntry_t),
    // check_hash);
    // Ocall_PrintfBinary(check_hash, CHUNK_HASH_HMAC_SIZE);

    // check key
    // Ocall_PrintfBinary(dec_key, CHUNK_HASH_HMAC_SIZE);

    RecipeEntry_t* tmp_recipe_entry;
    tmp_recipe_entry = (RecipeEntry_t*)recipe_buf;

    uint32_t offset = 0;

    // get chunk hash for a batch
    for (size_t i = 0; i < recipe_num; i++) {
        // SyncEnclave::Logging("offset ", "%d\n", tmp_recipe_entry->offset);
        // SyncEnclave::Logging("len ", "%d\n", tmp_recipe_entry->length);
        // uint8_t check_chunk_hash[CHUNK_HASH_HMAC_SIZE];
        // memcpy(check_chunk_hash, tmp_recipe_entry->chunkHash, CHUNK_HASH_HMAC_SIZE);
        // Ocall_PrintfBinary(check_chunk_hash, CHUNK_HASH_HMAC_SIZE);

        // copy the chunk fp back to output buf
        memcpy(chunk_hash_buf + offset, tmp_recipe_entry->chunkHash,
            CHUNK_HASH_HMAC_SIZE);
        offset += CHUNK_HASH_HMAC_SIZE;

        // move on
        tmp_recipe_entry++;

        _total_chunk_fp_num ++;
        _total_chunk_size += tmp_recipe_entry->length;
    }

    // // encrypt the chunk fp batch with sessionkey
    // crypto_util_->EncryptWithKey(cipher_ctx_, plain_fp_list_,
    //     recipe_num * CHUNK_HASH_HMAC_SIZE, session_key_, chunk_hash_buf);
    // memcpy(chunk_hash_buf, plain_fp_list_, recipe_num * CHUNK_HASH_HMAC_SIZE);

    return;

}

/**
 * @brief insert the batch into send MQ
 *
 * @param send_buf
 */
// void StreamPhase1Thd::SendBatch(SendMsgBuffer_t& send_buf) {
//     uint8_t flag = META_POOL;
//     while (true) {
//         if (send_thd_obj_->InsertSendMQ(META_POOL, send_buf)) {
//             break;
//         }
//     }

//     return ;
// }