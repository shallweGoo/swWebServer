#ifndef _LOCKER_H
#define _LOCKER_H

#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <exception>

using namespace std;

class sem
{
public:
    sem(int num = 0)
    {
        if(sem_init(&m_sem,0,num) != 0)
        {
            throw std::exception();
        }
    };

    ~sem()
    {
        if(sem_destroy(&m_sem) != 0)
        {
            throw std::exception(); 
        }
    };

    /*允许赋值符号*/

    // sem(sem& ) = delete; 
    // sem& operator=(const sem&)=delete;

    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }

    bool post()
    {
        return sem_post(&m_sem) == 0;
    }


private:
    sem_t m_sem;
};


class locker {
public:
    locker() {
        if (pthread_mutex_init(&m_mutex,NULL) != 0) {
            throw std::exception();
        }
    }
    ~locker() {
        if (pthread_mutex_destroy(&m_mutex) != 0) {
            throw std::exception();
        }
    }

    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    
    // bool trylock()
    // {
    //     return pthread_mutex_trylock(&m_mutex) == 0;
    // }

    // bool timelock(const timespec* time)
    // {
    //     return pthread_mutex_timedlock(&m_mutex, time);
    // }

    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t* get() {
        return &m_mutex;
    }

private:

    pthread_mutex_t m_mutex;

};




class cond
{
public:
    cond()
    {
        if(pthread_cond_init(&m_cond,NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~cond()
    {
        if(pthread_cond_destroy(&m_cond) != 0)
        {
            throw std::exception();
        }
    }

    bool wait(pthread_mutex_t* mutex)
    {
        return pthread_cond_wait(&m_cond,mutex) == 0;
    }


    bool signal(pthread_mutex_t* mutex)
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;

};




#endif