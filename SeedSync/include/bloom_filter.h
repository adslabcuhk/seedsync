/**
 * @file bloom_filter.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief define the interfaces of bloom filter
 * @version 0.1
 * @date 2024-05-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include "configure.h"

using namespace std;

class BloomFilter {
    private:
        bool* bit_array;
        uint32_t size_;
    public:
        /**
         * @brief Construct a new Bloom Filter object
         * 
         * @param size 
         */
        BloomFilter(uint32_t size);

        /**
         * @brief Destroy the Bloom Filter object
         * 
         */
        ~BloomFilter();

        /**
         * @brief lookup the key in the bloom filter
         * 
         * @param key 
         * @return true 
         * @return false 
         */
        bool lookup(uint8_t* key);

        /**
         * @brief insert the key into the bloom filter
         * 
         * @param key 
         */
        void insert(uint8_t* key);
};

#endif