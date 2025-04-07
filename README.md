# SeedSync: SGX-Enabled Encrypted Cross-Cloud Data Synchronization

## Introduction

SeedSync is a system designed to achieve secure and efficient cross-cloud data synchronization. It explores leveraging shielded execution for data synchronization, which achieves data security by offloading security guarantees to shielded execution, while addressing the delimma between encryption and network traffic reduction by allowing data to be processed unencrypted within a secure shielded region.

This repo contains the implemetation of SeedSync prototype, baseline approaches, and evaluation scripts used in our IEEE ICDCS 2025 paper.

## Publication

Jia Zhao, Yanjing Ren, Jingwei Li, and Patrick P. C. Lee. SGX-Enabled Encrypted Cross-Cloud Data Synchronization. Accepted for presentation at IEEE ICDCS 2025.

## Dependencies

### Basic packages

g++, cmake, openssl, libboost-all-dev, and jemalloc

The packages above can be directly installed via `apt-get`:

`sudo apt-get install cmake libssl-dev libboost-all-dev libjemalloc-dev build-essential libtool automake autoconf llvm make gcc g++ ansible libleveldb-dev librocksdb-dev libcurl4-openssl-dev libsnappy-dev libxxhash-dev libzstd-dev liblz4-dev libacl1-dev`

Note that we require the version of OpenSSL should be at least **1.1.0**. If the default version of OpenSSL from `apt-get` is older than1.1.0, please install OpenSSL manually from this [link](https://www.openssl.org/source/).


### SGX libs on Ubuntu 20.04

Install v2-2 version of SGX libs

```shell
sudo apt-get install -y --allow-downgrades \
    libsgx-ae-epid=2.15.100.3-focal1 \
    libsgx-ae-id-enclave=1.16.100.2-focal1 \
    libsgx-ae-le=2.15.100.3-focal1 \
    libsgx-ae-pce=2.15.100.3-focal1 \
    libsgx-ae-qe3=1.12.100.3-focal1 \
    libsgx-aesm-ecdsa-plugin=2.15.100.3-focal1 \
    libsgx-aesm-epid-plugin=2.15.100.3-focal1 \
    libsgx-aesm-launch-plugin=2.15.100.3-focal1 \
    libsgx-aesm-pce-plugin=2.15.100.3-focal1 \
    libsgx-aesm-quote-ex-plugin=2.15.100.3-focal1 \
    libsgx-enclave-common=2.15.100.3-focal1 \
    libsgx-epid=2.15.100.3-focal1 \
    libsgx-launch=2.15.100.3-focal1 \
    libsgx-pce-logic=1.12.100.3-focal1 \
    libsgx-qe3-logic=1.12.100.3-focal1 \
    libsgx-quote-ex=2.15.100.3-focal1 \
    libsgx-urts=2.15.100.3-focal1 \
    sgx-aesm-service=2.15.100.3-focal1
```

```shell 
echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | sudo tee /etc/apt/sources.list.d/intel-sgx.list

wget -qO - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | sudo apt-key add

wget https://download.01.org/intel-sgx/sgx-linux/2.15/as.ld.objdump.r4.tar.gz
tar -xzvf as.ld.objdump.r4.tar.gz
sudo cp ./external/toolset/ubuntu20.04/* /usr/local/bin

sudo cp /opt/intel/sgxsdk/lib64/libsgx_capable.a /opt/intel/sgxsdk/lib64/libsgx_capable.so /usr/lib/x86_64-linux-gnu/
```

## Build & Usage

This repo contains five sub-directory: SeedSync, DBE-SGX, Plain, Plain-Dedup, and Scripts. 

- SeedSync is the implementation of our cross-cloud data synchronization protocol.
- DBE-SGX is the implementation of an encrypted deduplicated approach (DEBE). SeedSync integrates DEBE to build upon its deduplication feature.
- Plain is the implementation of a plaintext data synchronization approach which applies fine-grain network reduction with deduplication, delta compression and local compression.
- Plain-Dedup is the implementation of a plaintext deduplicated approach. Plain is built upon the deduplication feature of Plain-Dedup.
- Scripts contains evaluation scripts used in the IEEE ICDCS'25 paper.

### SeedSync

Please refer to the README files in `./SeedSync` and`./DBE-SGX` for the building instruction and usage.

### Baseline approaches

#### Plain

For the baseline approach Plain, please refer to the README files in  `./Plain` and`./Plain-Dedup` for the building instruction and usage.

For Rsync and Rsyncrypto, we have the following instructions for quick setup. For more details, please refer to their homepages ([Rsync](https://rsync.samba.org/) and [Rsyncrypto](https://rsyncrypto.lingnu.com/index.php?title=Main_Page))

#### Rsyncrypto

```shell
sudo apt install libargtable2-dev
./bootstrap
./configure
make 
```

#### Rsync

```shell
pip3 install commonmark
./configure
make
sudo make install
```
