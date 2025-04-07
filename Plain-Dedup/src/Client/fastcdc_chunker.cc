/**
 * @file fastcdc_chunker.cc
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-03-21
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "../../include/fastcdc_chunker.h"

/**
 * @brief Construct a new FastCDC object
 * 
 */
FastCDChunker::FastCDChunker(uint64_t min_size, uint64_t avg_size, uint64_t max_size, 
            uint64_t read_size) {
    avg_chunk_size_ = avg_size;
    min_chunk_size_ = min_size;
    max_chunk_size_ = max_size;
    read_size_ = read_size;
    // read_size_ *= (1 << 20);
    // read_size_ *= 1024 * 1024;
    read_data_buf_ = (uint8_t*) malloc(read_size_ * sizeof(uint8_t));
    // read_data_buf_ = (uint8_t*) calloc(read_size_, sizeof(uint8_t));


    normal_chunk_size_ = this->CalNormalSize(min_chunk_size_, avg_chunk_size_, max_chunk_size_);
    uint32_t bits = (uint32_t) round(log2(static_cast<double>(avg_chunk_size_))); 
    mask_s_ = GenerateFastCDCMask(bits + 1);
    mask_l_ = GenerateFastCDCMask(bits - 1);
    tool::Logging(my_name_.c_str(), "init FastCDC.\n");
}

/**
 * @brief Destroy the FastCDC object
 * 
 */
FastCDChunker::~FastCDChunker() {
    free(read_data_buf_);
}

/**
 * @brief load the data from the file
 * 
 * @param input_file the input file handler
 * @return uint32_t the read size
 */
uint32_t FastCDChunker::LoadDataFromFile(ifstream& input_file) {
    input_file.read((char*)read_data_buf_, read_size_);
    pending_chunking_size_ = input_file.gcount();

    cout<<"pending_chunking_size_ = "<<pending_chunking_size_<<endl;

    // reset the offset
    cur_offset_ = 0;
    remain_chunking_size_ = pending_chunking_size_;

    return pending_chunking_size_;
}

uint32_t FastCDChunker::LoadDataFromFile(uint8_t* data, uint32_t size) {
    memcpy(read_data_buf_, data, read_size_);
    pending_chunking_size_ = size;

    // reset the offset
    cur_offset_ = 0;
    remain_chunking_size_ = pending_chunking_size_;

    return pending_chunking_size_;
}

uint32_t FastCDChunker::LoadDataFromBuf(uint8_t* input_buf, uint64_t input_size) {
    // if (is_first_load == true) {
    //     read_offset_ = 0;
    //     left_size_ = input_size;
    //     is_first_load = false;
    // }

    // if (left_size_ > read_size_) {
    //     memcpy(read_data_buf_, input_buf + read_offset_, read_size_);
    //     pending_chunking_size_ = read_size_;

    //     // update
    //     read_offset_ += read_size_;
    //     left_size_ = input_size - read_size_;

    //     cur_offset_ = 0;
    //     remain_chunking_size_ = left_size_;
    // }
    // else {
    //     memcpy(read_data_buf_, input_buf + read_offset_, left_size_);
    //     pending_chunking_size_ = left_size_;

    //     // update
    //     read_offset_ += left_size_;
    //     left_size_ = input_size - left_size_;

    //     cur_offset_ = 0;
    //     remain_chunking_size_ = 0;
    // }
    
    // return pending_chunking_size_;
    memcpy(read_data_buf_, input_buf, input_size);
    pending_chunking_size_ = input_size;
    remain_chunking_size_ = input_size;

    cur_offset_ = 0;

    return input_size;
}

void FastCDChunker::ResetLoadBuf() {
    is_first_load = true;
    read_offset_ = 0;
    
    return ;
}

/**
 * @brief generate a chunk
 * 
 * @param data the buffer to store the chunk data
 * @return uint32_t the chunk size
 */
uint32_t FastCDChunker::GenerateOneChunk(uint8_t* data) {
    if (pending_chunking_size_ == 0 || remain_chunking_size_ == 0) {
        return 0; // this is the end of the pending buffer
    }
    
    uint32_t chunk_size = 0;
    if (remain_chunking_size_ <= min_chunk_size_) {
        memcpy(data, read_data_buf_ + cur_offset_, remain_chunking_size_);
        chunk_size = remain_chunking_size_;
    } else {
        chunk_size = this->CutPoint(read_data_buf_ + cur_offset_, pending_chunking_size_ - cur_offset_);
        memcpy(data, read_data_buf_ + cur_offset_, chunk_size);
    }

    cur_offset_ += chunk_size;
    remain_chunking_size_ -= chunk_size;

    _total_chunk_num++;
    _total_file_size += chunk_size;
    return chunk_size;
}

/**
 * @brief compute the normal size 
 * 
 * @param min the min chunk size
 * @param avg the avg chunk size
 * @param max the max chunk size
 * @return uint32_t 
 */
uint32_t FastCDChunker::CalNormalSize(const uint32_t min, const uint32_t avg,
    const uint32_t max) {
    uint32_t off = min + tool::DivCeil(min, 2);
    if (off > avg) {
        off = avg;
    } 
    uint32_t diff = avg - off;
    if (diff > max) {
        return max;
    }
    return diff;
}

/**
 * @brief generate the mask according to the given bits
 * 
 * @param bits the number of '1' + 1
 * @return uint32_t the returned mask
 */
uint32_t FastCDChunker::GenerateFastCDCMask(uint32_t bits) {
    uint32_t tmp;
    tmp = (1 << tool::CompareLimit(bits, 1, 31)) - 1;
    return tmp;
}

/**
 * @brief To get the offset of chunks for a given buffer  
 * 
 * @param src the input buffer  
 * @param len the length of this buffer
 * @return uint32_t length of this chunk.
 */
uint32_t FastCDChunker::CutPoint(const uint8_t* src, const uint32_t len) {
    uint32_t n;
    uint32_t fp = 0;
    uint32_t i;
    i = std::min(len, static_cast<uint32_t>(min_chunk_size_)); 
    n = std::min(normal_chunk_size_, len);
    for (; i < n; i++) {
        fp = (fp >> 1) + FASTCDC_GEAR[src[i]];
        if (!(fp & mask_s_)) {
            return (i + 1);
        }
    }

    n = std::min(static_cast<uint32_t>(max_chunk_size_), len);
    for (; i < n; i++) {
        fp = (fp >> 1) + FASTCDC_GEAR[src[i]];
        if (!(fp & mask_l_)) {
            return (i + 1);
        }
    } 
    return i;
}