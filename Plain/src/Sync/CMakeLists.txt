#src/sync
aux_source_directory(. SYNC_SRC)

set(SGXOPENSSL_INCLUDE_PATH /opt/intel/sgxssl/include)
set(SGXOPENSSL_LIBRARY_PATH /opt/intel/sgxssl/lib64)
include_directories(${SGXOPENSSL_INCLUDE_PATH})
link_directories(${SGXOPENSSL_LIBRARY_PATH})

add_library(SyncCore stream_phase_1_thd.cc stream_phase_2_thd.cc stream_phase_3_thd.cc
    stream_phase_4_thd.cc stream_phase_5_thd.cc
    recv_thd.cc send_thd.cc phase_sender.cc phase_recv.cc sync_storage.cc sync_data_writer.cc
    locality_cache.cc sync_io.cc sync_configure.cc)


add_executable(Plain plainsync_main.cc)
target_link_libraries(Plain UtilCore SyncCore leveldb ${FINAL_OBJ})