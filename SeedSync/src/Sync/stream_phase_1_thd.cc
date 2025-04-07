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

#if (PHASE_BREAKDOWN == 1)
struct timeval phase1_stime;
struct timeval phase1_etime;
#endif

/**
 * @brief Construct a new Stream Phase 1 Thd:: Stream Phase 1 Thd object
 *
 * @param phase_sender_obj
 * @param eid_sgx
 */
StreamPhase1Thd::StreamPhase1Thd(PhaseSender* phase_sender_obj,
    sgx_enclave_id_t eid_sgx) {
    phase_sender_obj_ = phase_sender_obj;
    eid_sgx_ = eid_sgx;

    // for send batch
    send_batch_buf_.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t) + sync_config.GetMetaBatchSize() * CHUNK_HASH_SIZE * sizeof(uint8_t));
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
    cryptoObj_ = new CryptoPrimitive(CIPHER_TYPE, HASH_TYPE);
    cipherCtx_ = EVP_CIPHER_CTX_new();

    // tool::Logging(my_name_.c_str(), "init StreamPhase1Thd.\n");
}

/**
 * @brief Destroy the Stream Phase 1 Thd object
 *
 */
StreamPhase1Thd::~StreamPhase1Thd() {
    free(send_batch_buf_.sendBuffer);
    free(read_recipe_buf_);

    EVP_CIPHER_CTX_free(cipherCtx_);
    delete cryptoObj_;
}

/**
 * @brief start the sync request by sending chunk list
 *
 */
void StreamPhase1Thd::SyncRequest() {
    // traverse the recipe path, load the recipe file in batches
    std::multimap<std::time_t, fs::path> sorted_files;

    for (const auto& entry : fs::directory_iterator(recipe_path_)) {

        if (fs::is_regular_file(entry)) {
            string filenamestr; // length = CHUNK_HASH_SIZE * 2
            filenamestr.assign(entry.path().filename().c_str(),
                CHUNK_HASH_SIZE * 2);

            // cout <<"traverse "<< filenamestr << endl;

            // insert multimap
            sorted_files.insert({ fs::last_write_time(entry), entry.path() });
        }
    }

    // get the sorted file list
    for (const auto& file : sorted_files) {
        string recipe_name;
        recipe_name.assign(file.second.filename().c_str(), CHUNK_HASH_SIZE * 2);
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
void StreamPhase1Thd::ProcessOneRecipe(string recipe_name) {

#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase1_stime, NULL);
#endif      

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

        file_end = recipe_hdl.eof();
        size_t recipe_entry_num = read_cnt / sizeof(RecipeEntry_t);

        if (read_cnt == 0) {
            tool::Logging(my_name_.c_str(), "read cnt = 0.\n");
            break;
        }

        Ecall_Stream_Phase1_ProcessBatch(eid_sgx_, (uint8_t*)recipe_path.c_str(), processedRecipeBatchIndex,
            read_recipe_buf_, recipe_entry_num, send_batch_buf_.dataBuffer);
        processedRecipeBatchIndex++;
        // prepare the send buf
        send_batch_buf_.header->messageType = SYNC_CHUNK_FP;
        send_batch_buf_.header->currentItemNum = recipe_entry_num;
        send_batch_buf_.header->dataSize = recipe_entry_num * CHUNK_HASH_SIZE;
        phase_sender_obj_->SendBatch(&send_batch_buf_);

        // reset the send buf
        send_batch_buf_.header->currentItemNum = 0;
        send_batch_buf_.header->dataSize = 0;
    }

    // reach recipe end
    send_batch_buf_.header->messageType = FILE_END_CHUNK_FP;
    phase_sender_obj_->SendBatch(&send_batch_buf_);

    recipe_hdl.close();

#if (PHASE_BREAKDOWN == 1)
    gettimeofday(&phase1_etime, NULL);
    _phase1_process_time += tool::GetTimeDiff(phase1_stime, phase1_etime);

    tool::Logging(my_name_.c_str(), "Process time for phase 1: %f.\n", _phase1_process_time);
#endif

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
    uint8_t file_name_hash[CHUNK_HASH_SIZE] = { 0 };
    cryptoObj_->GenerateHMAC((uint8_t*)&full_name[0],
        full_name.size(), file_name_hash);

    tool::PrintBinaryArray(file_name_hash, CHUNK_HASH_SIZE);

    char file_hash_buf[CHUNK_HASH_SIZE * 2 + 1];
    for (uint32_t i = 0; i < CHUNK_HASH_SIZE; i++) {
        sprintf(file_hash_buf + i * 2, "%02x", file_name_hash[i]);
    }
    string recipe_name;
    recipe_name.assign(file_hash_buf, CHUNK_HASH_SIZE * 2);

    cout << "Sync start for recipe name: " << recipe_name << endl;

    // string recipe_path = config.GetRecipeRootPath() + recipe_name
    //     + config.GetRecipeSuffix();

    // string fullName = inputFile + to_string(clientID);
    // uint8_t fileNameHash[CHUNK_HASH_SIZE] = {0};
    // cryptoObj->GenerateHMAC(mdCtx, (uint8_t*)&fullName[0],
    //     fullName.size(), fileNameHash);

    // char fileHashBuf[CHUNK_HASH_SIZE * 2 + 1];
    // for (uint32_t i = 0; i < CHUNK_HASH_SIZE; i++) {
    //     sprintf(fileHashBuf + i * 2, "%02x", recvBuf.dataBuffer[i]);
    // }
    // string fileName;
    // fileName.assign(fileHashBuf, CHUNK_HASH_SIZE * 2);
    // string recipePath = config.GetRecipeRootPath() +
    //     fileName + config.GetRecipeSuffix();

    ProcessOneRecipe(recipe_name);

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