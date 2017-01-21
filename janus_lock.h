/***************************************************************************
 * 
 * Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
 * 
 **************************************************************************/
 
 
 
/**
 * @file janus_lock.h
 * @author jessesleep@hotmail.com
 * @date 2017/01/03 15:11:54
 * @brief 
 *  
 **/

#ifndef  __JANUS_LOCK_H_
#define  __JANUS_LOCK_H_

#include <string>

using namespace std;

class JanusLock {
    public:
        JanusLock(const string lock_name, const unsigned long lock_timeout_in_us);
        ~JanusLock();
        
        void setTimeout(unsigned long lock_timeout_in_us);
        const string get_lock_name();
        unsigned long get_timeout_renew_us();
        unsigned long get_timeout_check_us();
        unsigned long get_timeout_backoff_us();

    private:
        const string _lock_name;
        unsigned long _timeout_for_user_us;/*users should think lock will expire after this amount of time since acquisition*/
        unsigned long _timeout_renew_us;/*inside janus, leading locker is allowed to renew lock after this time since last renewal*/
        unsigned long _timeout_check_us;/*inside janus, following locker is allowed to check if lock expired after this time since last check*/
        unsigned long _timeout_backoff_us;/*inside janus, looking locker is allowed to retry acquiring lock after this time since last failed try*/
};

#endif  //__JANUS_LOCK_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
