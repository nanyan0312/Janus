/**
 * @file janus_lock.cpp
 * @author jessesleep@hotmail.com
 * @date 2017/01/13 11:25:33
 * @brief 
 *  
 **/

#include "janus_lock.h"

JanusLock::JanusLock(const string lock_name, const unsigned long lock_timeout_in_us) 
    : _lock_name(lock_name), _timeout_for_user_us(lock_timeout_in_us) {
        _timeout_renew_us = _timeout_for_user_us / 2;
        _timeout_check_us = 2 * _timeout_for_user_us;
        _timeout_backoff_us = random() % _timeout_for_user_us;
    }

JanusLock::~JanusLock() {
}

void JanusLock::setTimeout(unsigned long lock_timeout_in_us) {
    _timeout_for_user_us = lock_timeout_in_us;
    _timeout_renew_us = _timeout_for_user_us / 2;
    _timeout_check_us = 2 * _timeout_for_user_us;
    _timeout_backoff_us = random() % _timeout_for_user_us;
    return;
}

const string JanusLock::get_lock_name() {
    return _lock_name;
}

unsigned long JanusLock::get_timeout_renew_us() {
    return _timeout_renew_us;
}

unsigned long JanusLock::get_timeout_check_us() {
    return _timeout_check_us;
}

unsigned long JanusLock::get_timeout_backoff_us() {
    return _timeout_backoff_us;
}




/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
