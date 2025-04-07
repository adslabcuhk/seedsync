/**
 * @file send_thd.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief
 * @version 0.1
 * @date 2024-01-09
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "../../include/send_thd.h"

/**
 * @brief Construct a new Send Thd object
 *
 * @param send_channel
 */
SendThd::SendThd(SSLConnection* send_channel, pair<int, SSL*> send_conn_record,
    uint8_t* session_key, size_t session_key_size)
{
    // setup config
    // send_meta_batch_size_ = sync_config.GetMetaBatchSize();
    // send_data_batch_size_ = sync_config.GetDataBatchSize();

    // send_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) +
    //     send_data_batch_size_ * sizeof(Msg_t));
    // send_buf_.header = (NetworkHead_t*) send_buf_.sendBuffer;
    // send_buf_.dataBuffer = send_buf_.sendBuffer + sizeof(NetworkHead_t);
    // send_buf_.header->currentItemNum = 0;
    // send_buf_.header->dataSize = 0;

    // enc_send_buf_.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) +
    //     send_data_batch_size_ * sizeof(Msg_t));
    // enc_send_buf_.header = (NetworkHead_t*) enc_send_buf_.sendBuffer;
    // enc_send_buf_.dataBuffer = enc_send_buf_.sendBuffer + sizeof(NetworkHead_t);
    // enc_send_buf_.header->currentItemNum = 0;
    // enc_send_buf_.header->dataSize = 0;

    send_channel_ = send_channel;
    send_conn_record_ = send_conn_record;

    crypto_util_ = new CryptoPrimitive(CIPHER_TYPE, HASH_TYPE, HMAC_TYPE);
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    md_ctx_ = EVP_MD_CTX_new();
    tool::Logging(my_name_.c_str(), "init the SendThd.\n");
}

/**
 * @brief Construct a new Send Thd object
 *
 * @param send_channel
 */
SendThd::SendThd(SSLConnection* send_channel, pair<int, SSL*> send_conn_record,
    LockfreeMQ<SendMQEntry_t>* send_mq)
{
    // setup config
    send_meta_batch_size_ = sync_config.GetMetaBatchSize();
    send_data_batch_size_ = sync_config.GetDataBatchSize();

    send_channel_ = send_channel;
    send_conn_record_ = send_conn_record;

    send_MQ_ = send_mq;

    // init the memory pool
    // metadata memory pool
    uint64_t meta_entry_size = send_meta_batch_size_ * CHUNK_HASH_HMAC_SIZE * 2
        + sizeof(NetworkHead_t);
    metadata_pool_ = (uint8_t**)malloc(META_POOL_SIZE * sizeof(uint8_t*));
    for (size_t i = 0; i < META_POOL_SIZE; i++) {
        metadata_pool_[i] = (uint8_t*)malloc(meta_entry_size * sizeof(uint8_t));
        meta_valid_idx_.push_back(i);
    }
    // data memory pool
    uint64_t data_entry_size = send_data_batch_size_ * MAX_CHUNK_SIZE
        + sizeof(NetworkHead_t);
    data_pool_ = (uint8_t**)malloc(DATA_POOL_SIZE * sizeof(uint8_t*));
    for (size_t i = 0; i < DATA_POOL_SIZE; i++) {
        data_pool_[i] = (uint8_t*)malloc(data_entry_size * sizeof(uint8_t));
        data_valid_idx_.push_back(i);
    }

    crypto_util_ = new CryptoPrimitive(CIPHER_TYPE, HASH_TYPE, HMAC_TYPE);
    cipher_ctx_ = EVP_CIPHER_CTX_new();
    md_ctx_ = EVP_MD_CTX_new();
    tool::Logging(my_name_.c_str(), "init the SendThd.\n");
}

/**
 * @brief Destroy the Send Thd object
 *
 */
SendThd::~SendThd()
{
    // free(send_buf_.sendBuffer);
    // free(enc_send_buf_.sendBuffer);
    EVP_CIPHER_CTX_free(cipher_ctx_);
    EVP_MD_CTX_free(md_ctx_);
    delete crypto_util_;

    for (size_t i = 0; i < META_POOL_SIZE; i++) {
        free(metadata_pool_[i]);
    }
    free(metadata_pool_);

    for (size_t i = 0; i < DATA_POOL_SIZE; i++) {
        free(data_pool_[i]);
    }
    free(data_pool_);
}

/**
 * @brief send the sync login for connection estabulishment
 *
 */
void SendThd::SyncLogin()
{
    cout << "sent sync login" << endl;
    // TODO: may integrite with sessionKey exchange later
    // currently work as a notifier for estabulishment of second connection
    SendMsgBuffer_t login_msg;
    login_msg.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t));
    login_msg.header = (NetworkHead_t*)login_msg.sendBuffer;
    login_msg.header->messageType = SYNC_LOGIN;
    login_msg.header->dataSize = 0;
    login_msg.header->currentItemNum = 0;

    // send the login header
    // send_lck_->lock();
    // boost::unique_lock<boost::shared_mutex> t(this->mtx);
    // std::lock_guard<std::mutex> locker(this->mutex_);
    if (!send_channel_->SendData(send_conn_record_.second,
            login_msg.sendBuffer, sizeof(NetworkHead_t) + login_msg.header->dataSize)) {
        tool::Logging(my_name_.c_str(), "send the sync login error.\n");
        exit(EXIT_FAILURE);
    }
    // send_lck_->unlock();

    free(login_msg.sendBuffer);
    return;
}

/**
 * @brief send the login response
 *
 */
void SendThd::LoginResponse()
{
    cout << "sent login response" << endl;
    SendMsgBuffer_t login_msg;
    login_msg.sendBuffer = (uint8_t*)malloc(sizeof(NetworkHead_t));
    login_msg.header = (NetworkHead_t*)login_msg.sendBuffer;
    login_msg.header->messageType = SYNC_LOGIN_RESPONSE;
    login_msg.header->dataSize = 0;
    login_msg.header->currentItemNum = 0;

    // // send the login header
    // send_lck_->lock();
    // // boost::unique_lock<boost::shared_mutex> t(this->mtx);
    // // std::lock_guard<std::mutex> locker(this->mutex_);
    // if (!send_channel_->SendData(send_conn_record_.second,
    //     login_msg.sendBuffer, sizeof(NetworkHead_t) + login_msg.header->dataSize)) {
    //     tool::Logging(my_name_.c_str(), "send the sync login error.\n");
    //     exit(EXIT_FAILURE);
    // }
    // send_lck_->unlock();

    uint8_t flag = META_POOL;
    while (true) {
        if (this->InsertSendMQ(META_POOL, login_msg)) {
            break;
        }
    }

    free(login_msg.sendBuffer);
    return;
}

/**
 * @brief send the batch of data
 *
 * @param send_msg_buf
 */
void SendThd::SendBatch(SendMsgBuffer_t& send_msg_buf)
{
    // send_lck_->lock();
    // boost::unique_lock<boost::shared_mutex> t(this->mtx);
    // std::lock_guard<std::mutex> locker(this->mutex_);
    if (!send_channel_->SendData(send_conn_record_.second, send_msg_buf.sendBuffer,
            sizeof(NetworkHead_t) + send_msg_buf.header->dataSize)) {
        tool::Logging(my_name_.c_str(), "send the filename batch error.\n");
        exit(EXIT_FAILURE);
    }
    // send_lck_->unlock();

    return;
}

void SendThd::SendBatchtst(SendMsgBuffer_t& send_msg_buf)
{
    // send_lck_->lock();
    // boost::unique_lock<boost::shared_mutex> t(this->mtx);
    // std::lock_guard<std::mutex> locker(this->mutex_);
    // if (!send_channel_->SendData(send_conn_record_.second, send_msg_buf.sendBuffer,
    //     sizeof(NetworkHead_t) + send_msg_buf.header->dataSize)) {
    //     tool::Logging(my_name_.c_str(), "send the filename batch error.\n");
    //     exit(EXIT_FAILURE);
    // }
    // cout<<"send batch data"<<endl;
    // send_lck_->unlock();

    return;
}

/**
 * @brief send the batch of data
 *
 * @param send_buf
 * @param send_size
 */
void SendThd::SendBatch(uint8_t* send_buf, uint32_t send_size)
{
    if (!send_channel_->SendData(send_conn_record_.second, send_buf,
            send_size)) {
        tool::Logging(my_name_.c_str(), "send the filename batch error.\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Insert into the send MQ and memory pool
 *
 * @param flag
 * @param send_buf
 * @return true
 * @return false
 */
bool SendThd::InsertSendMQ(uint8_t flag, SendMsgBuffer_t& send_buf)
{

    if (flag == META_POOL) {
        // check whether the meta pool is full
        // cout<<"in insert meta"<<endl;
        mutex_.lock();
        cout << "push aqired lock" << endl;
        // cout<<"after lock"<<endl;
        if (!meta_valid_idx_.empty()) {
            // cout<<"idx list not empty"<<endl;
            // insert memory pool & mq
            // step-1: get a valid index
            uint32_t tmp_idx = meta_valid_idx_.front();
            meta_valid_idx_.pop_front();
            // mutex_.unlock();
            // step-2: copy the meta to memory pool
            // cout<<"inserting meta pool"<<endl;
            // cout<<"insert datasize"<<send_buf.header->dataSize<<endl;
            // cout<<send_buf.header->dataSize + sizeof(NetworkHead_t)<<endl;
            memcpy(metadata_pool_[tmp_idx], send_buf.sendBuffer,
                send_buf.header->dataSize + sizeof(NetworkHead_t));
            NetworkHead_t* head = (NetworkHead_t*)metadata_pool_[tmp_idx];
            // cout<<"get size"<<head->dataSize<<endl;
            // step-3: insert the MQ
            SendMQEntry_t tmp_entry;
            tmp_entry.flag = META_POOL;
            tmp_entry.index = tmp_idx;
            send_MQ_->Push(tmp_entry);

            mutex_.unlock();

            cout << "insert meta into mq" << endl;
            return true;
        } else {
            mutex_.unlock();
            cout << "idx list is empty, waiting for inserting" << endl;
            return false;
        }
    } else if (flag == DATA_POOL) {
        // check whether the data pool is full
        mutex_.lock();
        if (!data_valid_idx_.empty()) {
            // insert memory pool & mq
            // step-1: get a valid index
            uint32_t tmp_idx = data_valid_idx_.front();
            data_valid_idx_.pop_front();
            // step-2: copy the data to memory pool
            memcpy(data_pool_[tmp_idx], send_buf.sendBuffer,
                send_buf.header->dataSize + sizeof(NetworkHead_t));
            // step-3: insert the MQ
            SendMQEntry_t tmp_entry;
            tmp_entry.flag = DATA_POOL;
            tmp_entry.index = tmp_idx;
            send_MQ_->Push(tmp_entry);

            mutex_.unlock();
            return true;
        } else {
            mutex_.unlock();
            return false;
        }
    }
}

/**
 * @brief the main process of SendThd
 *
 */
void SendThd::Run()
{
    bool job_done = false;
    SendMQEntry_t tmp_entry;

    tool::Logging(my_name_.c_str(), "the main thread is running.\n");

    while (true) {
        if (send_MQ_->done_ && send_MQ_->IsEmpty()) {
            job_done = true;
        }

        if (send_MQ_->Pop(tmp_entry)) {
            if (tmp_entry.flag == META_POOL) {
                // read the data from meta pool
                mutex_.lock();
                cout << "pop aquired lock " << tmp_entry.index << endl;

                NetworkHead_t* head = (NetworkHead_t*)metadata_pool_[tmp_entry.index];
                // cout<<"datasize "<<head->dataSize<<endl;
                // cout<<head->dataSize + sizeof(NetworkHead_t)<<endl;
                // mutex_.unlock();
                // buffer check
                // memcpy_s()
                this->SendBatch(metadata_pool_[tmp_entry.index],
                    sizeof(NetworkHead_t) + head->dataSize);

                // test ssl_write
                // uint8_t tst = SYNC_FILE_NAME;
                // if (!send_channel_->SendData(send_conn_record_.second, &tst, sizeof(uint8_t))) {
                //     tool::Logging(my_name_.c_str(), "send the filename batch error.\n");
                //     exit(EXIT_FAILURE);
                // }

                // cout<<"after send"<<endl;
                // return back the index as valid
                // mutex_.lock();
                // mutex_.lock();
                // cout<<"after lock"<<endl;
                cout << "before push back" << endl;
                meta_valid_idx_.push_back(tmp_entry.index);
                cout << "after push idx" << endl;
                mutex_.unlock();
                // cout<<"after unlock"<<endl;

                cout << "sent a batch of meta data" << endl;
            } else if (tmp_entry.flag == DATA_POOL) {
                // read the data from data pool
                mutex_.lock();
                NetworkHead_t* head = (NetworkHead_t*)data_pool_[tmp_entry.index];
                this->SendBatch(data_pool_[tmp_entry.index],
                    sizeof(NetworkHead_t) + head->dataSize);
                // return back the index as valid
                data_valid_idx_.push_back(tmp_entry.index);
                mutex_.unlock();
            }
        }

        if (job_done) {
            break;
        }
    }

    tool::Logging(my_name_.c_str(), "thread exits.\n");

    return;
}
