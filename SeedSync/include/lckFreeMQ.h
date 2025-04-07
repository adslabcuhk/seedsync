/**
 * @file lckFreeMq.h
 * @author Jia Zhao (jzhao@cse.cuhk.edu.hk)
 * @brief 
 * @version 0.1
 * @date 2024-01-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef LOCKFREE_MQ
#define LOCKFREE_MQ

#include "configure.h"
#include "chunkStructure.h"
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

template <class T>
class LockfreeMQ {
    private:
        boost::lockfree::queue<T, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<SYNC_SEND_MQ_SIZE>> lckfree_mq_;
    public:
        boost::atomic<bool> done_;

        LockfreeMQ() {
            done_ = false;
        }

        ~LockfreeMQ() {

        }

        bool Push(T& data) {
            while (!lckfree_mq_.push(data)) {
                ;
            }
            return true;
        }

        bool Pop(T& data) {
            return lckfree_mq_.pop(data);
        }

        bool IsEmpty() {
            return lckfree_mq_.empty();
        }
};

#endif