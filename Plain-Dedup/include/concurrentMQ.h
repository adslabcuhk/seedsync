/**
 * @file concurrentMQ.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-01-25
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef CONCURRENT_MQ_H
#define CONCURRENT_MQ_H

#include "configure.h"
#include "chunkStructure.h"
#include "messageQueue/concurrentqueue.h"
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

template <class T>
class ConcurrentMQ {
    private:
        moodycamel::ConcurrentQueue<T>* concurrent_mq_;
    
    public:
        boost::atomic<bool> done_;

        /**
         * @brief Construct a new Concurrent M Q object
         * 
         * @param max_queue_size 
         */
        ConcurrentMQ(uint32_t max_queue_size) {
            concurrent_mq_ = new moodycamel::ConcurrentQueue<T>(max_queue_size);
            done_ = false;
        }

        ~ConcurrentMQ() {
            bool flag = IsEmpty();
            if (flag) {
	            delete concurrent_mq_;
            } else {
                fprintf(stderr, "ConcurrentMQ: Queue is not empty.\n");
                exit(EXIT_FAILURE);
            }  
        }

        /**
         * @brief push data to the queue
         * 
         * @param data the original data
         * @return true success
         * @return false fails
         */
        bool Push(T& data) {
            while (!concurrent_mq_->try_enqueue(data)) {
                ;
            }
            return true;
        }

        /**
         * @brief pop data from the queue 
         * 
         * @param data the original data
         * @return true success
         * @return false fails
         */
        bool Pop(T& data) {
            return concurrent_mq_->try_dequeue(data);
        }

        /**
         * @brief whether the queue is empty
         * 
         * @return true 
         * @return false 
         */
        bool IsEmpty() {
            size_t count = concurrent_mq_->size_approx();
            if (count == 0) {
                return true;
            }
            else {
                return false;
            }
        }
};

#endif