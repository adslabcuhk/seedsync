/**
 * @file chunker.cc
 * @author Zuoru YANG (zryang@cse.cuhk.edu.hk)
 * @brief implement the interface defined in chunker.h
 * @version 0.1
 * @date 2019-12-19
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <stdio.h>
#include <sys/time.h>
#include "../../include/chunker.h"
struct timeval sTimeChunking;
struct timeval eTimeChunking;
struct timeval sTimeMQ;
struct timeval eTimeMQ;

/**
 * @brief Construct a new Chunker object
 * 
 * @param path the target file path 
 * @param storageCoreObj refer to the storageCore MQ 
 */
Chunker::Chunker(std::string path) {
    LoadChunkFile(path);
    ChunkerInit(path);
    compressGenObj_ = new CompressGen(3.0, 3.0, 1);
    tool::Logging(myName_.c_str(), "init the chunker.\n");
}

/**
 * @brief initialize the chunker inputstream
 * 
 * @param path 
 */
void Chunker::ChunkerInit(string path) {
    // get the chunking method type
    chunkerType_ = config.GetChunkingType();
    avgChunkSize_ = config.GetAvgChunkSize();
    minChunkSize_ = config.GetMinChunkSize();
    maxChunkSize_ = config.GetMaxChunkSize();
    readSize_ = config.GetReadSize();
    readSize_ *= 1024 * 1024;
    slidingWinSize_ = config.GetSlidingWinSize();
    
    switch (chunkerType_) {
        case FIXED_SIZE_CHUNKING: {
            tool::Logging(myName_.c_str(), "using fixed size chunking.\n");
            waitingForChunkingBuffer_ = (uint8_t*) calloc(readSize_, sizeof(uint8_t));
            chunkBuffer_ = (uint8_t*) calloc(maxChunkSize_ , sizeof(uint8_t));

            if ((!waitingForChunkingBuffer_) || (!chunkBuffer_)) {
                tool::Logging(myName_.c_str(), "memory malloc error.\n");
                exit(EXIT_FAILURE);
            }

            if (minChunkSize_ >= avgChunkSize_ || minChunkSize_ >= maxChunkSize_) {
                tool::Logging(myName_.c_str(), "minChunkSize_ setting error.\n");
                exit(EXIT_FAILURE);
            }

            if (maxChunkSize_ <= avgChunkSize_ || maxChunkSize_ <= minChunkSize_) {
                tool::Logging(myName_.c_str(), "maxChunkSize_ setting error.\n");
                exit(EXIT_FAILURE);
            }
            break;
        }
        case FAST_CDC: {
            tool::Logging(myName_.c_str(), "using FastCDC chunking.\n");
            waitingForChunkingBuffer_ = (uint8_t*) calloc(readSize_, sizeof(uint8_t));
            pos_ = 0;

            if (!waitingForChunkingBuffer_) {
                tool::Logging(myName_.c_str(), "memory malloc error.\n");
                exit(EXIT_FAILURE);
            }

            if (minChunkSize_ >= avgChunkSize_ || minChunkSize_ >= maxChunkSize_) {
                tool::Logging(myName_.c_str(), "minChunkSize_ setting error.\n");
                exit(EXIT_FAILURE);
            }

            if (maxChunkSize_ <= avgChunkSize_ || maxChunkSize_ <= minChunkSize_) {
                tool::Logging(myName_.c_str(), "maxChunkSize_ setting error.\n");
                exit(EXIT_FAILURE);
            }

            normalSize_ = CalNormalSize(minChunkSize_, avgChunkSize_, maxChunkSize_);
            uint32_t bits = (uint32_t) round(log2(static_cast<double>(avgChunkSize_))); 
            maskS_ = GenerateFastCDCMask(bits + 1);
            maskL_ = GenerateFastCDCMask(bits - 1);
            break;
        }
        case LOCALITY_FASTCDC: {
            waitingForChunkingBuffer_ = (uint8_t*) calloc(readSize_, sizeof(uint8_t));
            pos_ = 0;

            if (!waitingForChunkingBuffer_) {
                tool::Logging(myName_.c_str(), "memory malloc error.\n");
                exit(EXIT_FAILURE);
            }
            tool::Logging(myName_.c_str(), "using locality FastCDC chunking.\n");
            min_seg_size_ = config.GetSegMinSize();
            avg_seg_size_ = config.GetSegAvgSize();
            max_seg_size_ = config.GetSegMaxSize();

            chunk_obj_ = new FastCDChunker(minChunkSize_, avgChunkSize_, maxChunkSize_, 
                readSize_);
            
            seg_obj_ = new FastCDChunker(min_seg_size_, avg_seg_size_, max_seg_size_, 
                readSize_);
            
            break;
        }
        case FSL_TRACE: {
            tool::Logging(myName_.c_str(), "using FSL trace chunking.\n");
            chunkBuffer_ = (uint8_t*) calloc(maxChunkSize_, sizeof(uint8_t));
            break;
        }
        case UBC_TRACE: {
            tool::Logging(myName_.c_str(), "using FSL trace chunking.\n");
            chunkBuffer_ = (uint8_t*) calloc(maxChunkSize_, sizeof(uint8_t));
            break;
        }
        default: {
            tool::Logging(myName_.c_str(), "error chunker type.\n");
            exit(EXIT_FAILURE);
        }
    }
    return ;
}

/**
 * @brief Destroy the Chunker object
 * 
 */
Chunker::~Chunker() {
    delete compressGenObj_;
    switch (chunkerType_) {
        case FIXED_SIZE_CHUNKING: {
            free(chunkBuffer_);
            free(waitingForChunkingBuffer_);
            break;
        }
        case FAST_CDC: {
            free(waitingForChunkingBuffer_);
            break;
        }
        case LOCALITY_FASTCDC: {
            delete seg_obj_;
            delete chunk_obj_;
            break;
        }
        case FSL_TRACE: {
            free(chunkBuffer_);
            break;
        }
        case UBC_TRACE: {
            free(chunkBuffer_);
            break;
        }
    }
    if (chunkingFile_.is_open()) {
        chunkingFile_.close();
    }

    fprintf(stderr, "========Chunker Info========\n");
    fprintf(stderr, "total file size: %lu\n", _recipe.recipeHead.fileSize);
    fprintf(stderr, "total chunk num: %lu\n", _recipe.recipeHead.totalChunkNum);
    fprintf(stderr, "total thread running time: %lf\n", totalTime_);
#if (CHUNKING_BREAKDOWN == 1)
    fprintf(stderr, "total MQ insert time: %lf\n", insertTime_);
    fprintf(stderr, "total chunking time: %lf\n", (totalTime_ - insertTime_));
    double chunkingBreakTime = ((totalTime_ - insertTime_) * 1024.0) / 
        (_recipe.recipeHead.fileSize / 1024.0 / 1024.0);
    fprintf(stderr, "chunking time: %lf\n", chunkingBreakTime);
#endif
    fprintf(stderr, "============================\n");
}

/**
 * @brief the chunking process
 * 
 */
void Chunker::Chunking(ifstream* input_file_hdl) {
    switch (chunkerType_) {
        case FIXED_SIZE_CHUNKING: {
            fixSizeChunking();
            break;
        }
        case FAST_CDC: {
            FastCDC();
            break;
        }
        case LOCALITY_FASTCDC: { // "chunking type = 2" in config
            Locality_FASTCDC(*input_file_hdl); 
            break;
        }
        case FSL_TRACE: {
            FSLChunking();
            break;
        }
        case UBC_TRACE: {
            UBCChunking();
            break;
        }
        default: {
            tool::Logging(myName_.c_str(), "chunking type error.\n");
            exit(EXIT_FAILURE);
        }
    }
    tool::Logging(myName_.c_str(), "thread exit.\n");
    return ;
}

/**
 * @brief load the input file 
 * 
 * @param path the path of the chunking file
 */
void Chunker::LoadChunkFile(string path) {
    if (chunkingFile_.is_open()) {
        chunkingFile_.close();
    }

    chunkingFile_.open(path, ios_base::in | ios::binary);
    if (!chunkingFile_.is_open()) {
        tool::Logging(myName_.c_str(), "open file: %s error.\n", 
            path.c_str());
        exit(EXIT_FAILURE);
    }
    return ;
}

/**
 * @brief fix size chunking process
 * 
 */
void Chunker::fixSizeChunking() {
    uint64_t chunkIDCnt = 0;
    memset(chunkBuffer_, 0, sizeof(uint8_t) * avgChunkSize_);
    uint64_t fileSize = 0;
    bool end = false;

    // start chunking
    while(!end) {
        memset((char*)waitingForChunkingBuffer_, 0, sizeof(uint8_t) * readSize_);
        chunkingFile_.read((char*)waitingForChunkingBuffer_, sizeof(uint8_t) * readSize_);
        end = chunkingFile_.eof();
        size_t len = chunkingFile_.gcount();
        size_t chunkedSize = 0;
        if (len == 0) {
            break;
        }
        fileSize += len;
        
        size_t remainSize = len;
        while (chunkedSize < len) {
            Data_t tempChunk;
            memset(chunkBuffer_, 0, sizeof(uint8_t) * avgChunkSize_);
            if (remainSize > avgChunkSize_) {
                // direct copy avgChunkSize
                memcpy(chunkBuffer_, waitingForChunkingBuffer_ + chunkedSize,
                    avgChunkSize_);
                tempChunk.chunk.chunkSize = avgChunkSize_;
                memcpy(tempChunk.chunk.data, chunkBuffer_, avgChunkSize_);
                chunkedSize += avgChunkSize_;
                remainSize -= avgChunkSize_;
            } else {
                // copy the tail chunk
                memcpy(chunkBuffer_, waitingForChunkingBuffer_ + chunkedSize,
                    remainSize);
                tempChunk.chunk.chunkSize = remainSize;
                memcpy(tempChunk.chunk.data, chunkBuffer_, remainSize);
                chunkedSize += remainSize;
                remainSize -= remainSize;
            }
            tempChunk.dataType = DATA_CHUNK;
#if (CHUNKING_BREAKDOWN == 1)
            gettimeofday(&sTimeMQ, NULL);
#endif
            if (!outputMQ_->Push(tempChunk)) {
                tool::Logging(myName_.c_str(), "insert chunk to output MQ error.\n");
                exit(EXIT_FAILURE);
            }
#if (CHUNKING_BREAKDOWN == 1)
            gettimeofday(&eTimeMQ, NULL);
            insertTime_ += tool::GetTimeDiff(sTimeMQ, eTimeMQ);
#endif
            chunkIDCnt++;
        }
    }
    _recipe.recipeHead.totalChunkNum = chunkIDCnt;
    _recipe.recipeHead.fileSize = fileSize;
    _recipe.dataType = RECIPE_END;

    if (!outputMQ_->Push(_recipe)) {
        tool::Logging(myName_.c_str(), "insert recipe end to output MQ error.\n");
        exit(EXIT_FAILURE);
    }
    // set the done flag
    outputMQ_->done_ = true;

    return ;      
}

/**
 * @brief FSL trace-driven chunking
 * 
 */
void Chunker::FSLChunking() {
    uint64_t chunkIDCnt = 0;
    uint64_t fileSize = 0;
    char readBuffer[256]; // suppose read size <= 256 bytes
    string readLineStr;

    normal_distribution<double> distribution(avgCompressRatio_, stdCompressRatio_);
    double compressionRatio = 0;
    gettimeofday(&sTimeChunking, NULL);

    // start chunking
    while (true) {
        // read the fingerprint recipe
        getline(chunkingFile_, readLineStr);
        if (readLineStr.size() == 0) {
            break;
        }
        if (chunkingFile_.eof()) {
            break;
        }
        memset(readBuffer, 0, 256);
        memcpy(readBuffer, readLineStr.c_str(), readLineStr.length());

        uint8_t chunkFp[7];
        memset(chunkFp, 0, 7);
        char* item;
        item = strtok(readBuffer, ":\t\n ");
        for (size_t index = 0; item != NULL && index < 6; index++) {
            chunkFp[index] = strtol(item, NULL, 16);
            item = strtok(NULL, ":\t\n ");
        }
        chunkFp[6] = '\0';

        // get the size of this chunk
        uint32_t size = atoi(item);
        memset(chunkBuffer_, 0, sizeof(uint8_t) * maxChunkSize_);
        if (size > maxChunkSize_) {
            size = maxChunkSize_;
        }

        // compute the fp seed
        uint64_t seed = 0;
        memcpy(&seed, chunkFp, 6);
        
        // compte the compression ratio
        default_random_engine shuffler(seed);
        distribution.reset();
        compressionRatio = distribution(shuffler);
        if (compressionRatio <= 1) {
            // ensure the data is compressible
            compressionRatio += 1;
        }
        uint32_t compressionInt = static_cast<uint32_t>(round(compressionRatio / 0.1));

        // generate the chunk
        compressGenObj_->GenerateChunkFromCanditdateSet(chunkBuffer_, compressionInt, size);
        memcpy(chunkBuffer_, chunkFp, 6);

        Data_t tempChunk;
        tempChunk.chunk.chunkSize = size;
        memcpy(tempChunk.chunk.data, chunkBuffer_, size);
        tempChunk.dataType = DATA_CHUNK;

#if (CHUNKING_BREAKDOWN == 1)
        gettimeofday(&sTimeMQ, NULL);
#endif
        if (!outputMQ_->Push(tempChunk)) {
            tool::Logging(myName_.c_str(), "insert chunk to output MQ error.\n");
            exit(EXIT_FAILURE);
        }
#if (CHUNKING_BREAKDOWN == 1)
        gettimeofday(&eTimeMQ, NULL);
        insertTime_ += tool::GetTimeDiff(sTimeMQ, eTimeMQ);
#endif
        
        chunkIDCnt++;
        fileSize += size;
        memset(chunkBuffer_, 0, MAX_CHUNK_SIZE);
    }
    _recipe.recipeHead.totalChunkNum = chunkIDCnt;
    _recipe.recipeHead.fileSize = fileSize;
    _recipe.dataType = RECIPE_END;

    if (!outputMQ_->Push(_recipe)) {
        tool::Logging(myName_.c_str(), "insert recipe end to output MQ error.\n");
        exit(EXIT_FAILURE);
    }
    // set the done flag
    outputMQ_->done_ = true;

    gettimeofday(&eTimeChunking, NULL);
    totalTime_ += tool::GetTimeDiff(sTimeChunking, eTimeChunking);
    return ;
}

/**
 * @brief UBC trace-driven chunking
 * 
 */
void Chunker::UBCChunking() {
    uint64_t chunkIDCnt = 0;
    uint64_t fileSize = 0;
    char readBuffer[256]; // suppose read size <= 256 bytes
    string readLineStr;

    normal_distribution<double> distribution(avgCompressRatio_, stdCompressRatio_); 
    double compressionRatio = 0;
    gettimeofday(&sTimeChunking, NULL);

    // start chunking
    while (true) {
        // read the fingerprint recipe
        getline(chunkingFile_, readLineStr);
        if (readLineStr.size() == 0) {
            break;
        }
        if (chunkingFile_.eof()) {
            break;
        }
        memset(readBuffer, 0, 256);
        memcpy(readBuffer, readLineStr.c_str(), readLineStr.length());

        uint8_t chunkFp[6];
        memset(chunkFp, 0, 6);
        char* item;
        item = strtok(readBuffer, ":\t\n ");
        for (size_t index = 0; item != NULL && index < 5; index++) {
            chunkFp[index] = strtol(item, NULL, 16);
            item = strtok(NULL, ":\t\n ");
        }
        chunkFp[5] = '\0';

        // get the size of this chunk
        uint32_t size = atoi(item);
        memset(chunkBuffer_, 0, sizeof(uint8_t) * maxChunkSize_);
        if (size > maxChunkSize_) {
            size = maxChunkSize_;
        }

        // compute the fp seed
        uint64_t seed = 0;
        memcpy(&seed, chunkFp, 5);

        // compute the compression ratio
        default_random_engine shuffler(seed);
        distribution.reset();
        compressionRatio = distribution(shuffler);
        if (compressionRatio < 1) {
            // ensure the data is compressible
            compressionRatio += 1;
        }
        uint32_t compressionInt = static_cast<uint32_t>(round(compressionRatio / 0.1));

        // generate the chunk
        compressGenObj_->GenerateChunkFromCanditdateSet(chunkBuffer_, compressionInt, size);
        memcpy(chunkBuffer_, chunkFp, 5);

        Data_t tempChunk;
        tempChunk.chunk.chunkSize = size;
        memcpy(tempChunk.chunk.data, chunkBuffer_, size);
        tempChunk.dataType = DATA_CHUNK;

#if (CHUNKING_BREAKDOWN == 1)
        gettimeofday(&sTimeMQ, NULL);
#endif
        if (!outputMQ_->Push(tempChunk)) {
            tool::Logging(myName_.c_str(), "insert chunk to output MQ error.\n");
            exit(EXIT_FAILURE);
        }
#if (CHUNKING_BREAKDOWN == 1)
        gettimeofday(&eTimeMQ, NULL);
        insertTime_ += tool::GetTimeDiff(sTimeMQ, eTimeMQ);
#endif
        
        chunkIDCnt++;
        fileSize += size;
    }
    _recipe.recipeHead.totalChunkNum = chunkIDCnt;
    _recipe.recipeHead.fileSize = fileSize;
    _recipe.dataType = RECIPE_END;

    if (!outputMQ_->Push(_recipe)) {
        tool::Logging(myName_.c_str(), "insert recipe end to output MQ error.\n");
        exit(EXIT_FAILURE);
    }
    // set the done flag
    outputMQ_->done_ = true;

    gettimeofday(&eTimeChunking, NULL);
    totalTime_ += tool::GetTimeDiff(sTimeChunking, eTimeChunking);
    return ;
}

/**
 * @brief use FastCDC to do the chunking 
 * 
 */
void Chunker::FastCDC() {
    uint64_t fileSize = 0;
    uint64_t chunkIDCnt = 0;
    size_t totalOffset = 0;
    bool end = false;
    gettimeofday(&sTimeChunking, NULL);

    while (!end) {
        memset((char*)waitingForChunkingBuffer_, 0, sizeof(uint8_t) * readSize_);
        chunkingFile_.read((char*)waitingForChunkingBuffer_, sizeof(uint8_t) * readSize_);
        end = chunkingFile_.eof();
        size_t len = chunkingFile_.gcount();
        if (len == 0) {
            break;
        }
        size_t localOffset = 0;
        while (((len - localOffset) >= maxChunkSize_) || (end && (localOffset < len))) {
            uint32_t cp = CutPoint(waitingForChunkingBuffer_ + localOffset, len - localOffset);
            Data_t tempChunk;
            tempChunk.chunk.chunkSize = cp;
            memcpy(tempChunk.chunk.data, waitingForChunkingBuffer_ + localOffset, cp);
            tempChunk.dataType = DATA_CHUNK;
#if (CHUNKING_BREAKDOWN == 1)
            gettimeofday(&sTimeMQ, NULL);
#endif
            if (!outputMQ_->Push(tempChunk)) {
                tool::Logging(myName_.c_str(), "insert chunk to output MQ error.\n");
                exit(EXIT_FAILURE);
            }
#if (CHUNKING_BREAKDOWN == 1)
            gettimeofday(&eTimeMQ, NULL);
            insertTime_ += tool::GetTimeDiff(sTimeMQ, eTimeMQ);
#endif
            localOffset += cp;
            fileSize += cp;
            chunkIDCnt++;
        }
        pos_ += localOffset;
        totalOffset += localOffset;
    
        chunkingFile_.seekg(totalOffset, std::ios_base::beg);
    }
    _recipe.recipeHead.totalChunkNum = chunkIDCnt;
    _recipe.recipeHead.fileSize = fileSize;
    _recipe.dataType = RECIPE_END;

    if (!outputMQ_->Push(_recipe)) {
        tool::Logging(myName_.c_str(), "insert recipe end to output MQ error.\n");
        exit(EXIT_FAILURE);
    }
    // set the done flag
    outputMQ_->done_ = true;

    gettimeofday(&eTimeChunking, NULL);
    totalTime_ += tool::GetTimeDiff(sTimeChunking, eTimeChunking);
    return ;
}

/**
 * @brief use FastCDC
 */
// void Chunker::FASTCDC() {
//     uint64_t file_size = 0;
//     uint64_t chunk_id = 0;
//     size_t total_offset = 0;
//     bool is_end = false;

//     Data_t tmp_data;
//     while (!is_end) {
//         uint64_t pending_size = 0;
//         pending_size = this->LoadDataFromFile();
        
//         if (pending_size == 0) {
//             break;
//         }

//         while (true) {
//             tmp_data.chunk.chunkSize = this->GenerateOneChunk(tmp_data.chunk.data);
//             tmp_data.dataType = DATA_CHUNK;

//             if (tmp_data.chunk.chunkSize == 0) {
//                 break;
//             }

//             if (!outputMQ_->Push(tmp_data)) {
//                 tool::Logging(myName_.c_str(), "insert chunk to output MQ error.\n");
//                 exit(EXIT_FAILURE);
//             }

//             file_size += tmp_data.chunk.chunkSize;
//             chunk_id ++;
//         }
//     }

//     _recipe.recipeHead.totalChunkNum = chunk_id;
//     _recipe.recipeHead.fileSize = file_size;
//     _recipe.dataType = RECIPE_END;

//     if (!outputMQ_->Push(_recipe)) {
//         tool::Logging(myName_.c_str(), "insert recipe end to output MQ error.\n");
//         exit(EXIT_FAILURE);
//     }
//     // set the done flag
//     outputMQ_->done_ = true;

//     return ;
// }

/**
 * @brief compute the normal size 
 * 
 * @param min 
 * @param av 
 * @param max 
 * @return uint32_t 
 */
uint32_t Chunker::CalNormalSize(const uint32_t min, const uint32_t av,
    const uint32_t max) {
    uint32_t off = min + tool::DivCeil(min, 2);
    if (off > av) {
        off = av;
    } 
    uint32_t diff = av - off;
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
uint32_t Chunker::GenerateFastCDCMask(uint32_t bits) {
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
uint32_t Chunker::CutPoint(const uint8_t* src, const uint32_t len) {
    uint32_t n;
    uint32_t fp = 0;
    uint32_t i;
    i = std::min(len, static_cast<uint32_t>(minChunkSize_)); 
    n = std::min(normalSize_, len);
    for (; i < n; i++) {
        fp = (fp >> 1) + GEAR[src[i]];
        if (!(fp & maskS_)) {
            return (i + 1);
        }
    }

    n = std::min(static_cast<uint32_t>(maxChunkSize_), len);
    for (; i < n; i++) {
        fp = (fp >> 1) + GEAR[src[i]];
        if (!(fp & maskL_)) {
            return (i + 1);
        }
    } 
    return i;
}

/**
 * @brief use FastCDC, first do segmenting, followed by chunking within segments
 * 
 */
void Chunker::Locality_FASTCDC(ifstream& input_file_hdl) {
    
    // bool is_end = false;
    // gettimeofday(&sTimeChunking, NULL);

    // while (!is_end) {
    //     uint64_t pending_size = 0;
    //     // pending_size = seg_obj_->LoadDataFromFile(input_file_hdl);
    // //         input_file.read((char*)read_data_buf_, read_size_);
    // // pending_chunking_size_ = input_file.gcount();

    // // cout<<"pending_chunking_size_ = "<<pending_chunking_size_<<endl;

    // // // reset the offset
    // // cur_offset_ = 0;
    // // remain_chunking_size_ = pending_chunking_size_;

    // // return pending_chunking_size_;
        
    //     memset((char*)waitingForChunkingBuffer_, 0, sizeof(uint8_t) * readSize_);
    //     chunkingFile_.read((char*)waitingForChunkingBuffer_, sizeof(uint8_t) * readSize_);
    //     // end = chunkingFile_.eof();
    //     pending_size = chunkingFile_.gcount();
    //     // pending_size = sizeof(uint8_t) * readSize_;

    //     // cout<<"pending_size = "<<pending_size<<endl;

    //     // uint8_t* tstbuf;
    //     // tstbuf = (uint8_t*) malloc(readSize_ * sizeof(uint8_t));

    //     seg_obj_->LoadDataFromFile(waitingForChunkingBuffer_, pending_size);

    //     if (pending_size == 0) {
    //         break;
    //     }

    //     SyncSegment_t tmp_seg;
    //     while (true) {
    //         // generate one seg
    //         tmp_seg.segSize = seg_obj_->GenerateOneChunk(tmp_seg.data);

    //         if (tmp_seg.segSize == 0) {
    //             break;
    //         }

    //         // further chunking
    //         Data_t tmp_chunk;
    //         chunk_obj_->ResetLoadBuf();
    //         {
    //             chunk_obj_->LoadDataFromBuf(tmp_seg.data, tmp_seg.segSize);

    //             while (true) {
    //                 // generate one chunk
    //                 tmp_chunk.chunk.chunkSize = chunk_obj_->GenerateOneChunk(tmp_chunk.chunk.data);

    //                 if (tmp_chunk.chunk.chunkSize == 0) {
    //                     break;
    //                 }

    //                 _recipe.recipeHead.totalChunkNum ++;
    //                 tmp_seg.chunkNum ++;

    //                 tmp_chunk.dataType = DATA_CHUNK;
    //                 if (!outputMQ_->Push(tmp_chunk)) {
    //                     tool::Logging(myName_.c_str(), "insert chunk to output MQ error.\n");
    //                     exit(EXIT_FAILURE);
    //                 }
    //             }
    //         }

    //         tmp_chunk.dataType = DATA_SEGMENT_END_FLAG;
    //         tmp_chunk.segmentHead.totalChunkNum = tmp_seg.chunkNum;
    //         tmp_chunk.segmentHead.segSize = tmp_seg.segSize;
    //         if (!outputMQ_->Push(tmp_chunk)) {
    //             tool::Logging(myName_.c_str(), "insert segment end to output MQ error.\n");
    //             exit(EXIT_FAILURE);
    //         }

    //         _recipe.recipeHead.totalSegNum ++;
    //         _recipe.recipeHead.fileSize += tmp_seg.segSize;
    //     }
    // }

    // _recipe.dataType = RECIPE_END;
    // if (!outputMQ_->Push(_recipe)) {
    //     tool::Logging(myName_.c_str(), "insert recipe end to output MQ error.\n");
    //     exit(EXIT_FAILURE);
    // }
    // // set the done flag
    // outputMQ_->done_ = true;

    // gettimeofday(&eTimeChunking, NULL);
    // totalTime_ += tool::GetTimeDiff(sTimeChunking, eTimeChunking);

    // return ;
}
