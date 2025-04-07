/**
 * @file phase_sender.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-07-30
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/phase_sender.h"

/**
 * @brief Construct a new Phase Sender object
 * 
 * @param send_channel 
 * @param send_conn_record 
 * @param phase_id 
 */
PhaseSender::PhaseSender(SSLConnection* send_channel, pair<int, SSL*> send_conn_record, uint8_t phase_id) {
    send_channel_ = send_channel;
    send_conn_record_ = send_conn_record;
    phase_id_ = phase_id;

    tool::Logging(my_name_.c_str(), "init the PhaseSender for Phase%d.\n", phase_id_);
}

/**
 * @brief Destroy the Phase Sender object
 * 
 */
PhaseSender::~PhaseSender() {
    ;
}

/**
 * @brief send the sync login for connection estabulishment
 * 
 */
void PhaseSender::SyncLogin() {
    tool::Logging(my_name_.c_str(), "send the sync login for Phase%d.\n", phase_id_);
    // TODO: may integrite with sessionKey exchange later
    // currently work as a notifier for estabulishment of second connection 
    SendMsgBuffer_t login_msg;
    login_msg.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t));
    login_msg.header = (NetworkHead_t*) login_msg.sendBuffer;
    login_msg.header->messageType = SYNC_LOGIN;
    login_msg.header->dataSize = 0;
    login_msg.header->currentItemNum = 0;

    // send the login header
    if (!send_channel_->SendData(send_conn_record_.second,
        login_msg.sendBuffer, sizeof(NetworkHead_t) + login_msg.header->dataSize)) {
        tool::Logging(my_name_.c_str(), "send the sync login error.\n");
        exit(EXIT_FAILURE);
    }

    free(login_msg.sendBuffer);
    return ;    
}

/**
 * @brief send the login response
 * 
 */
void PhaseSender::LoginResponse() {
    tool::Logging(my_name_.c_str(), "send the login response for Phase%d.\n", phase_id_);
    SendMsgBuffer_t login_msg;
    login_msg.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t));
    login_msg.header = (NetworkHead_t*) login_msg.sendBuffer;
    login_msg.header->messageType = SYNC_LOGIN_RESPONSE;
    login_msg.header->dataSize = 0;
    login_msg.header->currentItemNum = 0;

    // send the login header
    if (!send_channel_->SendData(send_conn_record_.second,
        login_msg.sendBuffer, sizeof(NetworkHead_t) + login_msg.header->dataSize)) {
        tool::Logging(my_name_.c_str(), "send the sync login error.\n");
        exit(EXIT_FAILURE);
    }

    free(login_msg.sendBuffer);
    return ;
}

/**
 * @brief send the batch of data
 * 
 * @param send_msg_buf 
 */
void PhaseSender::SendBatch(SendMsgBuffer_t* send_msg_buf) {
    
    // if(phase_id_ == 5)
        // cout<<"before sent "<<send_msg_buf->header->dataSize<<endl;

    if (!send_channel_->SendData(send_conn_record_.second, send_msg_buf->sendBuffer,
        sizeof(NetworkHead_t) + send_msg_buf->header->dataSize)) {
        tool::Logging(my_name_.c_str(), "send batch error.\n");
        exit(EXIT_FAILURE);
    }

    // if(phase_id_ == 5)
        // cout<<"after sent"<<endl;

    return ;
}

/**
 * @brief connect
 * 
 */
void PhaseSender::LoginConnect() {
    send_conn_record_ = send_channel_->ConnectSSL();

    return ;
}