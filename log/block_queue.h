#ifndef _BLOCK_QUEUE_H
#define _BLOCK_QUEUE_H

#include <iostream>
#include <pthread.h>
#include "../locker/locker.h"
#include <queue>

template <typename T>
class block_queue {
public:
    block_queue() = default;
    block_queue(size_t max_size) {
        m_max_size = max_size;
    }

    void resize(size_t max_size) {
        m_max_size = max_size;
    }

    ~block_queue() = default;

    bool empty() {
        m_mutex.lock();
        if (m_queue.empty()) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    bool full() {
        m_mutex.lock();
        if(m_queue.size() >= m_max_size) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }


    size_t size() {
        m_mutex.lock();
        size_t size = m_queue.size();
        m_mutex.unlock();
        return size;
    }

    bool push(T& item) {
        m_mutex.lock();
        if (m_queue.size() >= m_max_size) {
            m_mutex.unlock();
            m_cond.broadcast(); //满的话就广播通知别人取走
            return false;
        }

        m_queue.push(item);
        m_mutex.unlock();
        m_cond.broadcast(); //唤醒阻塞线程
        return true;
    }

    bool pop(T& get_item) {
        m_mutex.lock();
        while(m_queue.empty()) {
            if (m_cond.wait(m_mutex.get()) == false) {
                m_mutex.unlock();
                return false;
            }
        }
        get_item = m_queue.front();
        m_queue.pop();
        m_mutex.unlock();
        return true;
    }


    T& front() {
        //双检锁
        if (!m_queue.empty()) {
            m_mutex.lock();
            if (!m_queue.empty()) {
                T& ret = m_queue.front();
                m_mutex.unlock();
                return ret;
            }
            m_mutex.unlock();
            return NULL;
        }
    }


    bool clear(bool isreset) {
        m_mutex.lock();
        m_queue.clear();
        if (isreset) {
            m_max_size = 0;
        }
        m_mutex.unlock();
        return true;
    }




private:
    queue<T> m_queue;
    locker m_mutex;
    cond m_cond;
    size_t m_max_size;
};


#endif