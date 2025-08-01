/**
 * @file plainsync_main.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief realize the plaintext data sync in FAST'12
 * @version 0.1
 * @date 2024-05-23
 * 
 * @copyright Copyright (c) 2024
 * 
 */

// basic building blocks
#include "../../include/configure.h"
#include "../../include/sync_configure.h"
#include "../../include/factoryDatabase.h"
#include "../../include/absDatabase.h"
#include "../../include/define.h"
#include "../../include/chunkStructure.h"
#include "../../include/messageQueue.h"
#include "../../include/lckFreeMQ.h"
#include "../../include/sslConnection.h"
#include "../../include/sync_storage.h"
// sync protocol phases threads
#include "../../include/phase_sender.h"
#include "../../include/phase_recv.h"
#include "../../include/stream_phase_1_thd.h"
#include "../../include/stream_phase_2_thd.h"
#include "../../include/stream_phase_3_thd.h"
#include "../../include/stream_phase_4_thd.h"
#include "../../include/stream_phase_5_thd.h"
#include "../../include/sync_data_writer.h"
#include "../../include/sync_io.h"
#include "../../include/locality_cache.h"
// #include "../src/Enclave/include/syncOcall.h"


// // for SGX related
// #include "sgx_urts.h"
// #include "sgx_capable.h"

// for the interrupt
#include <signal.h>
#include <boost/thread/thread.hpp>

using namespace std;

// // the variable to record the enclave info
// sgx_enclave_id_t eid_sgx;
// sgx_launch_token_t token_sgx = {0};
// sgx_status_t status_sgx;
// int update_sgx;

// write a SyncConfigure
Configure config("config.json");
SyncConfigure sync_config("sync_config.json");
string my_name = "PlainSyncMain";

// for input MQs
// stream-phase-1: input = chunkHash list; output = uni chunkHash list
MessageQueue<StreamPhase1MQ_t>* p1_MQ;
// stream-phase-2: input = uni chunkHash list; output = feature list
MessageQueue<StreamPhase2MQ_t>* p2_MQ;
// stream-phase-3: input = feature list; output = base hash list
MessageQueue<StreamPhase3MQ_t>* p3_MQ;
// stream-phase-4: input = base hash list; output = chunk data (delta/uni)
MessageQueue<StreamPhase4MQ_t>* p4_MQ;
// stream-phase-5: input = chunk data (delta/uni); output = storage pool
MessageQueue<StreamPhase5MQ_t>* p5_MQ;

// for outside global indexes
DatabaseFactory db_factory;
// AbsDatabase* out_seg_db;
AbsDatabase* out_chunk_db;
AbsDatabase* out_feature_db;

// // for storage
SyncStorage* sync_storage_obj = nullptr;
SyncDataWriter* sync_data_writer_obj = nullptr;
SyncIO* sync_io_obj = nullptr;

// for locality cache
LocalityCache* locality_cache_obj = nullptr;

// thread objects
PhaseSender* phase1_sender_obj = nullptr;
PhaseSender* phase2_sender_obj = nullptr;
PhaseSender* phase3_sender_obj = nullptr;
PhaseSender* phase4_sender_obj = nullptr;
PhaseSender* phase5_sender_obj = nullptr;
PhaseRecv* phase2_recv_thd = nullptr;
PhaseRecv* phase3_recv_thd = nullptr;
PhaseRecv* phase4_recv_thd = nullptr;
PhaseRecv* phase5_recv_thd = nullptr;
PhaseRecv* phase6_recv_thd = nullptr;

StreamPhase1Thd* stream_phase_1_thd = nullptr;
StreamPhase2Thd* stream_phase_2_thd = nullptr;
StreamPhase3Thd* stream_phase_3_thd = nullptr;
StreamPhase4Thd* stream_phase_4_thd = nullptr;
StreamPhase5Thd* stream_phase_5_thd = nullptr;

// recv buf
SendMsgBuffer_t recv_buf;

vector<boost::thread*> thd_list;

void Usage() {
    fprintf(stderr, "%s -t [s/w] -i [sync file path]. \n"
        "-t: operation ([s/w]:)\n"
        "\ts: sync request\n"
        "\tw: waiting\n",
        my_name.c_str()
    );

    return;
}

void CTRLC(int s) {
    tool::Logging(my_name.c_str(), "terminated with ctrl+c interruption. \n");

    phase2_recv_thd->SetDoneFlag();
    phase4_recv_thd->SetDoneFlag();
    phase6_recv_thd->SetDoneFlag();
    stream_phase_2_thd->SetDoneFlag();
    stream_phase_4_thd->SetDoneFlag();

    for (auto it : thd_list) {
        it->join();
    }

    for (auto it : thd_list) {
        delete it;
    }

    // clean up
    delete p1_MQ;
    // delete p2_MQ;
    delete p3_MQ;
    // delete p4_MQ;
    delete p5_MQ;
    // delete p6_MQ;
    // delete p7_MQ;
    // delete p8_MQ;
    // delete p8local_MQ;
    // delete sync_data_writer_MQ;
    // delete send_MQ;
    // delete send_thd_obj;
    // delete recv_thd_obj;
    // delete phase1_sender_obj;
    delete phase2_sender_obj;
    // delete phase3_sender_obj;
    delete phase4_sender_obj;
    // delete phase5_sender_obj;
    delete phase2_recv_thd;
    // delete phase3_recv_thd;
    delete phase4_recv_thd;
    // delete phase5_recv_thd;
    delete phase6_recv_thd;
    // delete stream_phase_1_thd;
    delete stream_phase_2_thd;
    // delete stream_phase_3_thd;
    delete stream_phase_4_thd;
    // delete stream_phase_5_thd;
    delete sync_data_writer_obj;
    delete sync_storage_obj;
    delete sync_io_obj;

    // Ecall_Destroy_Sync(eid_sgx);
    // Ecall_Sync_Enclave_Destroy(eid_sgx);
    // SyncOutEnclave::Destroy();

    free(recv_buf.sendBuffer);
    // OutEnclave::Destroy();
}

int main(int argc, char* argv[]) {

    // The main process
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sigIntHandler, 0);

    sigIntHandler.sa_handler = CTRLC;
    sigaction(SIGKILL, &sigIntHandler, 0);
    sigaction(SIGINT, &sigIntHandler, 0);

    srand(tool::GetStrongSeed());

    const char opt_str[] = "t:i:";
    int option;

    if (argc < (int)sizeof(opt_str)) {
        tool::Logging(my_name.c_str(), "wrong argc: %d\n", argc);
        Usage();
        exit(EXIT_FAILURE);
    }

    uint32_t opt_type;
    string sync_file_name;

    while ((option = getopt(argc, argv, opt_str)) != -1) {
        switch (option) {
            case 't': {
                if (strcmp("s", optarg) == 0) {
                    opt_type = SYNC_OPT;
                }
                else if (strcmp("w", optarg) == 0) {
                    opt_type = WAIT_OPT;
                } 
                else {
                    tool::Logging(my_name.c_str(), "wrong operation type.\n");
                    Usage();
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'i': {
                sync_file_name.assign(optarg);
                cout<<"get file name "<<sync_file_name<<endl;
                break;
            }
            case '?': {
                tool::Logging(my_name.c_str(), "error optopt: %c\n", optopt);
                tool::Logging(my_name.c_str(), "error opterr: %c\n", opterr);
                Usage();
                exit(EXIT_FAILURE);
            }
        }
    }

    // setup the log file
    string log_file_name = "Dest-Log";
    std::ofstream log_file_hdl;
    bool is_first_file = false;

    if (!tool::FileExist(log_file_name)) {
        log_file_hdl.open(log_file_name, ios_base::out);
        if (!log_file_hdl.is_open()) {
            tool::Logging(my_name.c_str(), "fail to open the dest log file.\n");
            exit(EXIT_FAILURE);
        }

        log_file_hdl << "enclave duplicate num, " << "enclave unique num, "
            << "global duplicate num, " << "global unique num, "
            << "enclave similar num, " << "batch similar num, "
            << "non-similar num, "
            << "total recv size, " << "total write size, "
            << "only comp size, " << "comp delta size, "
            << "uncomp delta size, " << "uncomp similar size, "
            << "comp similar size, "
            << "(accum.) phase2 time, " << "(accum.) phase4 time, " << "(accum.) phase6 time, "
            << "total time (dest), "
            << endl;
        is_first_file = true;
        log_file_hdl.close();
    }
    else {
        // log_file_hdl.open(log_file_name, ios_base::app | ios_base::out);
        // if (!log_file_hdl.is_open()) {
        //     tool::Logging(my_name.c_str(), "fail to open the dest log file.\n");
        //     exit(EXIT_FAILURE);
        // }
    }

    // setup the log file
    string src_log_file_name = "Src-Log";
    std::ofstream src_log_file_hdl;
    bool src_is_first_file = false;

    if (!tool::FileExist(src_log_file_name)) {
        src_log_file_hdl.open(src_log_file_name, ios_base::out);
        src_log_file_hdl << "total chunk size, " << "total chunk num, "
            << "total unique size, " << "total unique num, " 
            << "total similar size, " << "total similar num, "
            << "total base size, "
            << "total delta size, " << "total comp delta size, "
            << "(accum.) phase1 time, "
            << "(accum.) phase3 time, "
            << "(accum.) phase5 time, "
            << endl;
        src_is_first_file = true;
        // log_file_hdl.close();
    }
    else {
        src_log_file_hdl.open(src_log_file_name, ios_base::app | ios_base::out);
        if (!src_log_file_hdl.is_open()) {
            tool::Logging(my_name.c_str(), "fail to open the src log file.\n");
            exit(EXIT_FAILURE);
        }
    }

    // for enclave info
    SyncEnclaveInfo_t sync_enclave_info;

    // for network connections
    // int cloud_id;
    // string cloud_ip;
    // int send_port;
    // int recv_port;
    // SSLConnection* send_channel;
    // pair<int, SSL*> send_conn_record;
    // SSLConnection* recv_channel;
    // pair<int, SSL*> recv_conn_record;

    // 5 connection pairs for per-phase send & recv
    int cloud_id;
    string cloud_ip;

    int p1_send_port;
    int p2_recv_port;
    SSLConnection* p1_send_channel;
    pair<int, SSL*> p1_send_conn_record;
    SSLConnection* p2_recv_channel;
    pair<int, SSL*> p2_recv_conn_record;

    int p2_send_port;
    int p3_recv_port;
    SSLConnection* p2_send_channel;
    pair<int, SSL*> p2_send_conn_record;
    SSLConnection* p3_recv_channel;
    pair<int, SSL*> p3_recv_conn_record;

    int p3_send_port;
    int p4_recv_port;
    SSLConnection* p3_send_channel;
    pair<int, SSL*> p3_send_conn_record;
    SSLConnection* p4_recv_channel;
    pair<int, SSL*> p4_recv_conn_record;

    int p4_send_port;
    int p5_recv_port;
    SSLConnection* p4_send_channel;
    pair<int, SSL*> p4_send_conn_record;
    SSLConnection* p5_recv_channel;
    pair<int, SSL*> p5_recv_conn_record;

    int p5_send_port;
    int p6_recv_port;
    SSLConnection* p5_send_channel;
    pair<int, SSL*> p5_send_conn_record;
    SSLConnection* p6_recv_channel;
    pair<int, SSL*> p6_recv_conn_record;


    // SendMsgBuffer_t recv_buf;
    // recv_buf.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + SESSION_KEY_BUFFER_SIZE);
    recv_buf.sendBuffer = (uint8_t*) malloc(sizeof(NetworkHead_t) + 
        MAX_CHUNK_SIZE * sync_config.GetDataBatchSize());
    recv_buf.header = (NetworkHead_t*) recv_buf.sendBuffer;
    recv_buf.header->dataSize = 0;
    recv_buf.header->currentItemNum = 0;
    recv_buf.dataBuffer = recv_buf.sendBuffer + sizeof(NetworkHead_t);
    uint32_t recv_size = 0;

    // // init MQ
    // p1_MQ = new MessageQueue<StreamPhase1MQ_t>(CHUNK_QUEUE_SIZE);
    // p2_MQ = new MessageQueue<StreamPhase2MQ_t>(CHUNK_QUEUE_SIZE);
    // p3_MQ = new MessageQueue<StreamPhase3MQ_t>(CHUNK_QUEUE_SIZE);
    // p4_MQ = new MessageQueue<StreamPhase4MQ_t>(CHUNK_QUEUE_SIZE);

//     p1_MQ = new MessageQueue<phase1MQ_t>(CHUNK_QUEUE_SIZE);
//     p2_MQ = new MessageQueue<phase2MQ_t>(CHUNK_QUEUE_SIZE);
//     p3_MQ = new MessageQueue<phase3MQ_t>(CHUNK_QUEUE_SIZE);
//     p4_MQ = new MessageQueue<phase4MQ_t>(CHUNK_QUEUE_SIZE);
//     p5_MQ = new MessageQueue<phase5MQ_t>(LARGER_QUEUE_SIZE);
//     p6_MQ = new MessageQueue<phase6MQ_t>(CHUNK_QUEUE_SIZE);
//     p7_MQ = new MessageQueue<phase7MQ_t>(CHUNK_QUEUE_SIZE);
//     p8_MQ = new MessageQueue<phase8MQ_t>(CHUNK_QUEUE_SIZE);
//     p8local_MQ = new MessageQueue<phase8LocalMQ_t>(CHUNK_QUEUE_SIZE);
//     sync_data_writer_MQ = new MessageQueue<sendDelta_t>(CHUNK_QUEUE_SIZE);

    // send_MQ = new LockfreeMQ<SendMQEntry_t>;

    // init outside indexes
//     out_seg_db = db_factory.CreateDatabase(IN_MEMORY, sync_config.GetOutSegDBName());
    // out_chunk_db = db_factory.CreateDatabase(IN_MEMORY, sync_config.GetOutChunkDBName());
    out_chunk_db = db_factory.CreateDatabase(IN_MEMORY, config.GetFp2ChunkDBName());
    out_feature_db = db_factory.CreateDatabase(IN_MEMORY, sync_config.GetOutFeatureDBName());

    // init thread list
    // vector<boost::thread*> thd_list;
    boost::thread* tmp_thd;
    boost::thread::attributes thd_attrs;
    thd_attrs.set_stack_size(1024*1024*1024);

    // init the enclave
    // status_sgx = sgx_create_enclave(ENCLAVE_PATH, SGX_DEBUG_FLAG, &token_sgx,
    //     &update_sgx, &eid_sgx, NULL);
    // if (status_sgx != SGX_SUCCESS) {
    //     tool::Logging(my_name.c_str(), "fail to create the enclave.\n");
    //     exit(EXIT_FAILURE);
    // } else {
    //     tool::Logging(my_name.c_str(), "create the enclave successfully.\n");
    // }

    sync_storage_obj = new SyncStorage();
    sync_io_obj = new SyncIO(out_chunk_db, out_feature_db, sync_storage_obj);

    // // config the enclave
    // SyncEnclaveConfig_t enclave_config;
    // enclave_config.sendChunkBatchSize = sync_config.GetDataBatchSize();
    // enclave_config.sendRecipeBatchSize = config.GetSendRecipeBatchSize();
    // enclave_config.sendMetaBatchSize = sync_config.GetMetaBatchSize();
    // // init the sync enclave
    // Ecall_Sync_Enclave_Init(eid_sgx, &enclave_config);
    // // init the sync ecalls
    // Ecall_Init_Sync(eid_sgx);

    // // // init the out-enclave var
    // // OutEnclave::Init(dataWriterObj_, fp2ChunkDB_, storageCoreObj_,
    // //     recvDecoderObj_);
    // // SyncOutEnclave::Init(out_seg_db, out_chunk_db, out_feature_db, sync_storage_obj);
    // SyncOutEnclave::Init(out_chunk_db, out_feature_db, sync_storage_obj);

    switch (opt_type) {
        case SYNC_OPT: {
            // init MQ
            // p1_MQ = new MessageQueue<StreamPhase1MQ_t>(1);
            p2_MQ = new MessageQueue<StreamPhase2MQ_t>(CHUNK_QUEUE_SIZE);
            // p3_MQ = new MessageQueue<StreamPhase3MQ_t>(1);
            p4_MQ = new MessageQueue<StreamPhase4MQ_t>(CHUNK_QUEUE_SIZE);

            // the cloud who issues a sync request
            cloud_id = sync_config.GetCloud1ID(); // assign cloud-1 as sync issuer
            cloud_ip = sync_config.GetCloud1IP();

            // // init the network connection for send
            // send_port = sync_config.GetCloud1SendPort(); 
            // // init the network connection for recv
            // recv_port = sync_config.GetCloud1RecvPort();

            // init the network connections
            p1_send_port = sync_config.GetP1SendPort();
            p3_recv_port = sync_config.GetP3RecvPort();
            p3_send_port = sync_config.GetP3SendPort();
            p5_recv_port = sync_config.GetP5RecvPort();
            p5_send_port = sync_config.GetP5SendPort();

            p1_send_channel = new SSLConnection(sync_config.GetCloud2IP(),
                sync_config.GetP2RecvPort(), IN_CLIENTSIDE);
            p3_recv_channel = new SSLConnection(sync_config.GetCloud1IP(),
                p3_recv_port, IN_SERVERSIDE);
            p3_send_channel = new SSLConnection(sync_config.GetCloud2IP(),
                sync_config.GetP4RecvPort(), IN_CLIENTSIDE);
            p5_recv_channel = new SSLConnection(sync_config.GetCloud1IP(),
                p5_recv_port, IN_SERVERSIDE);
            p5_send_channel = new SSLConnection(sync_config.GetCloud2IP(),
                sync_config.GetP6RecvPort(), IN_CLIENTSIDE);

            // triggle the sync request
            // step-1: phase-1 connect to phase-2
            p1_send_conn_record = p1_send_channel->ConnectSSL();
            phase1_sender_obj = new PhaseSender(p1_send_channel, p1_send_conn_record, 1);
            
            // TODO: if not the first time, set up the network
            // if ()
            phase1_sender_obj->SyncLogin();

            cout<<"after sync login"<<endl;

            // phase-3 recv login from phase-2
            p3_recv_conn_record = p3_recv_channel->ListenSSL();

            cout<<"p3 recv connects with p2 send"<<endl;

            p3_send_conn_record = p3_send_channel->ConnectSSL();
            phase3_sender_obj = new PhaseSender(p3_send_channel, p3_send_conn_record, 3);
            phase3_sender_obj->SyncLogin();

            cout<<"p3 send connects with p4 recv"<<endl;

            p5_recv_conn_record = p5_recv_channel->ListenSSL();

            cout<<"p4 send connects with p5 recv"<<endl;

            p5_send_conn_record = p5_send_channel->ConnectSSL();
            phase5_sender_obj = new PhaseSender(p5_send_channel, p5_send_conn_record, 5);
            phase5_sender_obj->SyncLogin();

            cout<<"p5 send connects with p6 recv"<<endl;



            // if (!p3_recv_channel->ReceiveData(p3_recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //     tool::Logging(my_name.c_str(), "recv login header error.\n");
            //     exit(EXIT_FAILURE);
            // }
            // else if (recv_buf.header->messageType == SYNC_LOGIN) {
            //     // phase-3's send connects with phase-4's recv
            //     p3_send_conn_record = p3_send_channel->ConnectSSL();
            //     phase3_sender_obj = new PhaseSender(p3_send_channel, p3_send_conn_record, 3);
            //     phase3_sender_obj->SyncLogin();

            //     // phase-5 recv login from phase-4
            //     p5_recv_conn_record = p5_recv_channel->ListenSSL();

            //     if (!p5_recv_channel->ReceiveData(p5_recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //         tool::Logging(my_name.c_str(), "recv login header error.\n");
            //         exit(EXIT_FAILURE);
            //     }
            //     else if (recv_buf.header->messageType == SYNC_LOGIN) {
            //         // phase-5's send connects with phase-6's recv
            //         p5_send_conn_record = p5_send_channel->ConnectSSL();
            //         phase5_sender_obj = new PhaseSender(p5_send_channel, p5_send_conn_record, 5);
            //         phase5_sender_obj->SyncLogin();

            //         // recv the login response
            //         if (!p5_recv_channel->ReceiveData(p5_recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //             tool::Logging(my_name.c_str(), "recv login response error.\n");
            //             exit(EXIT_FAILURE);
            //         }
            //         else if (recv_buf.header->messageType == SYNC_LOGIN_RESPONSE) {
            //             tool::Logging(my_name.c_str(), "sync login success! All connections have been established.\n");
            //         }
            //     }
            // }




            // ////////////////////////////////////////////////////////////////////////////            
            // // step-1: cloud-1 (sync issuer) connects to cloud-2
            // send_channel = new SSLConnection(sync_config.GetCloud2IP(),
            //     sync_config.GetCloud2RecvPort(), IN_CLIENTSIDE);
            // send_conn_record = send_channel->ConnectSSL();

            

            // // step-2: cloud-1 sets up the recv channel
            // recv_channel = new SSLConnection(sync_config.GetCloud1IP(),
            //     sync_config.GetCloud1RecvPort(), IN_SERVERSIDE);

            // // notify cloud-2 to connect to the cloud-1's recv channel
            // // send_thd_obj = new SendThd(send_channel, send_conn_record);
            // send_thd_obj = new SendThd(send_channel, send_conn_record, send_MQ);

            // // send_thd_obj->SetSendLck(&send_lck);

            // send_thd_obj->SyncLogin();
            // cout<<"sent sync login"<<endl;

            // recv_conn_record = recv_channel->ListenSSL();

            // if (!recv_channel->ReceiveData(recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //     tool::Logging(my_name.c_str(), "recv login response error.\n");
            //     exit(EXIT_FAILURE);
            // }
            // if (recv_buf.header->messageType == SYNC_LOGIN_RESPONSE) {
            //     tool::Logging(my_name.c_str(), "sync login success!.\n");
            // }
            // else {
            //     tool::Logging(my_name.c_str(), "login response header type error.\n");
            //     exit(EXIT_FAILURE);
            // }

            // // recv_conn_record = recv_channel->ListenSSL();

            // recv_thd_obj = new RecvThd(recv_channel, recv_conn_record, p1_MQ, p2_MQ, p3_MQ, p4_MQ, 
            //     sync_data_writer_obj);

            // tmp_thd = new boost::thread(thd_attrs, boost::bind(&RecvThd::Run, recv_thd_obj));
            // thd_list.push_back(tmp_thd);

            // tmp_thd = new boost::thread(thd_attrs, boost::bind(&SendThd::Run, send_thd_obj));
            // thd_list.push_back(tmp_thd);

            // init the working thread here
            // stream_phase_3_thd = new StreamPhase3Thd(send_thd_obj, p2_MQ, out_chunk_db, eid_sgx);

            phase3_recv_thd = new PhaseRecv(p3_recv_channel, p3_recv_conn_record, p2_MQ, 3);

            tmp_thd = new boost::thread(thd_attrs, boost::bind(&PhaseRecv::Run, phase3_recv_thd));
            thd_list.push_back(tmp_thd);

            stream_phase_3_thd = new StreamPhase3Thd(phase3_sender_obj, p2_MQ, out_chunk_db, sync_io_obj);

            tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase3Thd::Run, stream_phase_3_thd));
            thd_list.push_back(tmp_thd);

            stream_phase_5_thd = new StreamPhase5Thd(phase5_sender_obj, p4_MQ, out_chunk_db, sync_io_obj);
            
            tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase5Thd::Run, stream_phase_5_thd));
            thd_list.push_back(tmp_thd);

            // stream_phase_1_thd = new StreamPhase1Thd(send_thd_obj, eid_sgx);
            stream_phase_1_thd = new StreamPhase1Thd(phase1_sender_obj);

            phase5_recv_thd = new PhaseRecv(p5_recv_channel, p5_recv_conn_record, p4_MQ, 5);
            phase5_recv_thd->SetPhase1Obj(stream_phase_1_thd, sync_file_name);
            phase5_recv_thd->SetPhaseObjForSrcLog(stream_phase_3_thd, stream_phase_5_thd);

            tmp_thd = new boost::thread(thd_attrs, boost::bind(&PhaseRecv::Run, phase5_recv_thd));
            thd_list.push_back(tmp_thd);

            // stream_phase_5_thd = new StreamPhase5Thd(send_thd_obj, p4_MQ, out_chunk_db, eid_sgx);

            // /////////////////////////////
            // stream_phase_5_thd = new StreamPhase5Thd(phase5_sender_obj, p4_MQ, out_chunk_db, eid_sgx);
            
            // tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase5Thd::Run, stream_phase_5_thd));
            // thd_list.push_back(tmp_thd);

            // // stream_phase_1_thd = new StreamPhase1Thd(send_thd_obj, eid_sgx);
            // stream_phase_1_thd = new StreamPhase1Thd(phase1_sender_obj, eid_sgx);
            // /////////////////////////////

            // // recv the login response
            // if (!p5_recv_channel->ReceiveData(p5_recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //     tool::Logging(my_name.c_str(), "recv login response error.\n");
            //     exit(EXIT_FAILURE);
            // }
            // else if (recv_buf.header->messageType == SYNC_LOGIN_RESPONSE) {
            //     tool::Logging(my_name.c_str(), "sync login success! All connections have been established.\n");
            // }

            // start the sync request by sending the chunk hash list
            // stream_phase_1_thd->SyncRequest();

            // only when the connections are all established.

            stream_phase_1_thd->SyncRequest(sync_file_name);

            for (auto it : thd_list) {
                it->join();
            }

            // get the enclave info here
            // Ecall_GetSyncEnclaveInfo(eid_sgx, &sync_enclave_info, SOURCE_CLOUD);


            for (auto it : thd_list) {
                delete it;
            }

            // update the source log
            // src_log_file_hdl << sync_file_name << ", "
            //     << sync_enclave_info.total_chunk_size << ", " << sync_enclave_info.total_chunk_num << ", "
            //     << sync_enclave_info.total_unique_size << ", " << sync_enclave_info.total_unique_num << ", "
            //     << sync_enclave_info.total_similar_size << ", " << sync_enclave_info.total_similar_num << ", "
            //     << sync_enclave_info.total_base_size << ", "
            //     << sync_enclave_info.total_delta_size << ", "
            //     << sync_enclave_info.total_comp_delta_size << ", "
            //     <<endl;

            src_log_file_hdl << sync_file_name << ", "
                // << sync_enclave_info.total_chunk_size << ", " 
                << stream_phase_1_thd->_total_chunk_size << ", "
                // << sync_enclave_info.total_chunk_num << ", "
                << stream_phase_1_thd->_total_chunk_fp_num << ", "
                // << sync_enclave_info.total_unique_size << ", " 
                << stream_phase_3_thd->_total_unique_size << ", "
                // << sync_enclave_info.total_unique_num << ", "
                << stream_phase_3_thd->_total_unique_num << ", "
                // << sync_enclave_info.total_similar_size << ", " 
                << stream_phase_5_thd->_total_similar_chunk_size << ", "
                // << sync_enclave_info.total_similar_num << ", "
                << stream_phase_5_thd->_total_similar_num << ", "
                // << sync_enclave_info.total_base_size << ", "
                << stream_phase_5_thd->_total_base_chunk_size << ", "
                // << sync_enclave_info.total_delta_size << ", "
                << stream_phase_5_thd->_total_delta_size << ", "
                // << sync_enclave_info.total_comp_delta_size << ", "
                << stream_phase_5_thd->_total_comp_delta_size << ", "
                << stream_phase_1_thd->stream1_process_time_ << ", "
                << stream_phase_3_thd->stream3_process_time_ << ", "
                << stream_phase_5_thd->stream5_process_time_ << ", "
                <<endl;

            src_log_file_hdl.flush();

            break;
        }
        case WAIT_OPT: {
            sync_data_writer_obj = new SyncDataWriter(sync_io_obj, out_chunk_db);
            locality_cache_obj = new LocalityCache();

            // init MQ
            p1_MQ = new MessageQueue<StreamPhase1MQ_t>(CHUNK_QUEUE_SIZE);
            // p2_MQ = new MessageQueue<StreamPhase2MQ_t>(1);
            p3_MQ = new MessageQueue<StreamPhase3MQ_t>(CHUNK_QUEUE_SIZE);
            // p4_MQ = new MessageQueue<StreamPhase4MQ_t>(1);
            p5_MQ = new MessageQueue<StreamPhase5MQ_t>(1);

            // the cloud who receives a sync request
            cloud_id = sync_config.GetCloud2ID();
            cloud_ip = sync_config.GetCloud2IP();

            // recv_port = sync_config.GetCloud2RecvPort();
            // send_port = sync_config.GetCloud2SendPort();

            // init the network connections
            p2_recv_port = sync_config.GetP2RecvPort();
            p2_send_port = sync_config.GetP2SendPort();
            p4_recv_port = sync_config.GetP4RecvPort();
            p4_send_port = sync_config.GetP4SendPort();
            p6_recv_port = sync_config.GetP6RecvPort();

            p2_recv_channel = new SSLConnection(sync_config.GetCloud2IP(),
                p2_recv_port, IN_SERVERSIDE);
            p2_send_channel = new SSLConnection(sync_config.GetCloud1IP(),
                sync_config.GetP3RecvPort(), IN_CLIENTSIDE);
            p4_recv_channel = new SSLConnection(sync_config.GetCloud2IP(),
                p4_recv_port, IN_SERVERSIDE);
            p4_send_channel = new SSLConnection(sync_config.GetCloud1IP(),
                sync_config.GetP5RecvPort(), IN_CLIENTSIDE);
            p6_recv_channel = new SSLConnection(sync_config.GetCloud2IP(),
                p6_recv_port, IN_SERVERSIDE);
            
            // setup phase connections

            // phase-2 recv login from phase-1
            p2_recv_conn_record = p2_recv_channel->ListenSSL();

            p2_send_conn_record = p2_send_channel->ConnectSSL();
            phase2_sender_obj = new PhaseSender(p2_send_channel, p2_send_conn_record, 2);
            phase2_sender_obj->SyncLogin();

            p4_recv_conn_record = p4_recv_channel->ListenSSL();

            p4_send_conn_record = p4_send_channel->ConnectSSL();
            phase4_sender_obj = new PhaseSender(p4_send_channel, p4_send_conn_record, 4);
            phase4_sender_obj->SyncLogin();

            p6_recv_conn_record = p6_recv_channel->ListenSSL();


            phase2_recv_thd = new PhaseRecv(p2_recv_channel, p2_recv_conn_record, p1_MQ, 2);
            phase2_recv_thd->SetSenderObj(phase2_sender_obj);
            cout<<"set first flg "<<is_first_file<<endl;
            phase2_recv_thd->SetFirstFlag(is_first_file);

            tmp_thd = new boost::thread(thd_attrs, boost::bind(&PhaseRecv::Run, phase2_recv_thd));
            thd_list.push_back(tmp_thd);
            
            stream_phase_2_thd = new StreamPhase2Thd(phase2_sender_obj, p1_MQ, out_chunk_db, sync_io_obj, locality_cache_obj);

            tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase2Thd::Run, stream_phase_2_thd));
            thd_list.push_back(tmp_thd);

            phase4_recv_thd = new PhaseRecv(p4_recv_channel, p4_recv_conn_record, p3_MQ, 4);
            phase4_recv_thd->SetSenderObj(phase4_sender_obj);
            cout<<"set first flg "<<is_first_file<<endl;
            phase4_recv_thd->SetFirstFlag(is_first_file);

            tmp_thd = new boost::thread(thd_attrs, boost::bind(&PhaseRecv::Run, phase4_recv_thd));
            thd_list.push_back(tmp_thd);

            stream_phase_4_thd = new StreamPhase4Thd(phase4_sender_obj, p3_MQ, sync_io_obj, locality_cache_obj);

            tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase4Thd::Run, stream_phase_4_thd));
            thd_list.push_back(tmp_thd);

            phase6_recv_thd = new PhaseRecv(p6_recv_channel, p6_recv_conn_record, p5_MQ, 6);
            // set the sync writer here
            phase6_recv_thd->SetSyncDataWriter(sync_data_writer_obj);
            phase6_recv_thd->SetPhaseObjForDestLog(stream_phase_2_thd, stream_phase_4_thd);
            // set sgx eid
            // phase6_recv_thd->SetSgxEid(eid_sgx);

            tmp_thd = new boost::thread(thd_attrs, boost::bind(&PhaseRecv::Run, phase6_recv_thd));
            thd_list.push_back(tmp_thd);

            phase4_sender_obj->LoginResponse();


            // if (!p2_recv_channel->ReceiveData(p2_recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //     tool::Logging(my_name.c_str(), "recv login header error.\n");
            //     exit(EXIT_FAILURE);
            // }
            // else if (recv_buf.header->messageType == SYNC_LOGIN) {
            //     // phase-2's send connects with phase-3's recv
            //     p2_send_conn_record = p2_send_channel->ConnectSSL();
            //     phase2_sender_obj = new PhaseSender(p2_send_channel, p2_send_conn_record, 2);
            //     phase2_sender_obj->SyncLogin();

            //     // phase-4 recv login from phase-3
            //     p4_recv_conn_record = p4_recv_channel->ListenSSL();

            //     if (!p4_recv_channel->ReceiveData(p4_recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //         tool::Logging(my_name.c_str(), "recv login header error.\n");
            //         exit(EXIT_FAILURE);
            //     }
            //     else if (recv_buf.header->messageType == SYNC_LOGIN) {
            //         // phase-4's send connects with phase-5's recv
            //         p4_send_conn_record = p4_send_channel->ConnectSSL();
            //         phase4_sender_obj = new PhaseSender(p4_send_channel, p4_send_conn_record, 4);
            //         phase4_sender_obj->SyncLogin();

            //         // phase-6 recv login from phase-5
            //         p6_recv_conn_record = p6_recv_channel->ListenSSL();

            //         if (!p6_recv_channel->ReceiveData(p6_recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //             tool::Logging(my_name.c_str(), "recv login header error.\n");
            //             exit(EXIT_FAILURE);
            //         }
            //         else if (recv_buf.header->messageType == SYNC_LOGIN) {
            //             // all connections established: send the login response

            //             phase2_recv_thd = new PhaseRecv(p2_recv_channel, p2_recv_conn_record, p1_MQ, 2);

            //             tmp_thd = new boost::thread(thd_attrs, boost::bind(&PhaseRecv::Run, phase2_recv_thd));
            //             thd_list.push_back(tmp_thd);
                        
            //             stream_phase_2_thd = new StreamPhase2Thd(phase2_sender_obj, p1_MQ, out_chunk_db, eid_sgx);

            //             tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase2Thd::Run, stream_phase_2_thd));
            //             thd_list.push_back(tmp_thd);

            //             phase4_recv_thd = new PhaseRecv(p4_recv_channel, p4_recv_conn_record, p3_MQ, 4);

            //             tmp_thd = new boost::thread(thd_attrs, boost::bind(&PhaseRecv::Run, phase4_recv_thd));
            //             thd_list.push_back(tmp_thd);

            //             stream_phase_4_thd = new StreamPhase4Thd(phase4_sender_obj, p3_MQ, eid_sgx);

            //             tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase4Thd::Run, stream_phase_4_thd));
            //             thd_list.push_back(tmp_thd);

            //             phase6_recv_thd = new PhaseRecv(p6_recv_channel, p6_recv_conn_record, p5_MQ, 6);
            //             // set the sync writer here
            //             phase6_recv_thd->SetSyncDataWriter(sync_data_writer_obj);

            //             tmp_thd = new boost::thread(thd_attrs, boost::bind(&PhaseRecv::Run, phase6_recv_thd));
            //             thd_list.push_back(tmp_thd);

            //             // // tmp_thd = new boost::thread(thd_attrs, boost::bind(&RecvThd::Run, recv_thd_obj));
            //             // // thd_list.push_back(tmp_thd);
            //             // tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase2Thd::Run, stream_phase_2_thd));
            //             // thd_list.push_back(tmp_thd);
            //             // tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase4Thd::Run, stream_phase_4_thd));
            //             // thd_list.push_back(tmp_thd);

            //             phase4_sender_obj->LoginResponse();
            //         }
            //     }

            // }

            // ////////////////////////////////////////////////////////////////////////////////////

            // // step-1: cloud-2 waiting for sync request
            // recv_channel = new SSLConnection(sync_config.GetCloud2IP(),
            //     sync_config.GetCloud2RecvPort(), IN_SERVERSIDE);

            // // recv notification (SYNC_LOGIN) from cloud-1, start connecting cloud-1's recv channel
            // recv_conn_record = recv_channel->ListenSSL();

            // if (!recv_channel->ReceiveData(recv_conn_record.second, recv_buf.sendBuffer, recv_size)) {
            //     tool::Logging(my_name.c_str(), "recv the sync login header error.\n");
            //     exit(EXIT_FAILURE);
            // }
            // if (recv_buf.header->messageType == SYNC_LOGIN) {
            //     // connect with cloud-1's recv
            //     send_channel = new SSLConnection(sync_config.GetCloud1IP(),
            //         sync_config.GetCloud1RecvPort(), IN_CLIENTSIDE);
            //     send_conn_record = send_channel->ConnectSSL();

            //     // send_thd_obj = new SendThd(send_channel, send_conn_record);
            //     send_thd_obj = new SendThd(send_channel, send_conn_record, send_MQ);

            //     // send_thd_obj->SetSendLck(&send_lck);

            //     tmp_thd = new boost::thread(thd_attrs, boost::bind(&SendThd::Run, send_thd_obj));
            //     thd_list.push_back(tmp_thd);

            //     // init the working thread here
            //     recv_thd_obj = new RecvThd(recv_channel, recv_conn_record, p1_MQ, p2_MQ, p3_MQ, p4_MQ,
            //         sync_data_writer_obj);

                // stream_phase_2_thd = new StreamPhase2Thd(send_thd_obj, p1_MQ, out_chunk_db, eid_sgx);

                // stream_phase_4_thd = new StreamPhase4Thd(send_thd_obj, p3_MQ, eid_sgx);

                // // tmp_thd = new boost::thread(thd_attrs, boost::bind(&RecvThd::Run, recv_thd_obj));
                // // thd_list.push_back(tmp_thd);
                // tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase2Thd::Run, stream_phase_2_thd));
                // thd_list.push_back(tmp_thd);
                // tmp_thd = new boost::thread(thd_attrs, boost::bind(&StreamPhase4Thd::Run, stream_phase_4_thd));
                // thd_list.push_back(tmp_thd);

                // // response when ready
                // send_thd_obj->LoginResponse();
                // cout<<"sent login response"<<endl;

            // }
            // else {
            //     tool::Logging(my_name.c_str(), "recv header type error.\n");
            //     exit(EXIT_FAILURE);
            // }

            for (auto it : thd_list) {
                it->join();
            }

            for (auto it : thd_list) {
                delete it;
            }

            break;
        }
    }

    // close log file
    src_log_file_hdl.close();
    
    // sync waiter
    free(recv_buf.sendBuffer);
    delete p1_MQ;
    delete p3_MQ;
    delete p5_MQ;
    delete sync_data_writer_obj;
    delete phase1_sender_obj;
    delete phase3_sender_obj;
    delete phase5_sender_obj;
    delete phase3_recv_thd;
    delete phase5_recv_thd;
    delete stream_phase_1_thd;
    delete stream_phase_3_thd;
    delete stream_phase_5_thd;

    return 0;
}