/**
 * @file bloom-filter.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief Implement the interfaces defined in bloom_filter.h
 * @version 0.1
 * @date 2024-05-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "../../include/bloom_filter.h"

/**
 * @brief Construct a new Bloom Filter object
 * 
 * @param size 
 */
BloomFilter::BloomFilter(uint32_t size) {
    size_ = size;
    bit_array = new bool[size_];
    for (uint32_t i = 0; i < size_; i++) {
        bit_array[i] = false;
    }
}

/**
 * @brief Destroy the Bloom Filter object
 * 
 */
BloomFilter::~BloomFilter() {
    delete[] bit_array;
}

/**
 * @brief lookup the key in the bloom filter
 * 
 * @param key 
 * @return true 
 * @return false 
 */
bool BloomFilter::lookup(uint8_t* key) {

}

/**
 * @brief insert the key into the bloom filter
 * 
 * @param key 
 */
void BloomFilter::insert(uint8_t* key) {
    
}