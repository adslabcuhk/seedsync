#include <bits/stdc++.h>
#include <chrono>
#include <iostream>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <vector>

using namespace std;
using namespace chrono;

// 生成随机数据
void generateRandomData(vector<unsigned char>& data, size_t size)
{
    data.resize(size);
    for (auto& byte : data) {
        byte = static_cast<unsigned char>(rand() % 256);
    }
}

// 计算 SHA-256
void computeSHA256(const vector<unsigned char>& data)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
}

// 计算 HMAC-SHA-256
void computeHMACSHA256(const vector<unsigned char>& data, const unsigned char* key, size_t key_len)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    HMAC(EVP_sha256(), key, key_len, data.data(), data.size(), hash, nullptr);
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <data size in bytes> <rounds>\n";
        return 1;
    }

    // 解析参数
    size_t dataSize = stoi(argv[1]);
    int rounds = stoi(argv[2]);
    uint8_t* key = new uint8_t[SHA256_DIGEST_LENGTH];
    size_t key_len = SHA256_DIGEST_LENGTH;
    memset(key, 0, key_len);

    // 生成测试数据
    vector<unsigned char> data;
    generateRandomData(data, dataSize);

    // 测试 SHA-256
    double sha256TotalTime = 0;
    for (int i = 0; i < rounds; ++i) {
        auto start = high_resolution_clock::now();
        computeSHA256(data);
        auto end = high_resolution_clock::now();
        sha256TotalTime += duration<double, micro>(end - start).count();
    }
    double sha256AvgTime = sha256TotalTime / rounds;

    // 测试 HMAC-SHA-256
    double hmacSha256TotalTime = 0;
    for (int i = 0; i < rounds; ++i) {
        auto start = high_resolution_clock::now();
        computeHMACSHA256(data, key, key_len);
        auto end = high_resolution_clock::now();
        hmacSha256TotalTime += duration<double, micro>(end - start).count();
    }
    double hmacSha256AvgTime = hmacSha256TotalTime / rounds;

    // 输出结果
    cout << "SHA-256 Average Time: " << sha256AvgTime << " microseconds\n";
    cout << "HMAC-SHA-256 Average Time: " << hmacSha256AvgTime << " microseconds\n";
    delete key;
    return 0;
}
