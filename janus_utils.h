/**
 * @file janus_utils.h
 * @author jessesleep@hotmail.com
 * @date 2017/01/13 11:56:37
 * @brief 
 *  
 **/




#ifndef  __JANUS_UTILS_H_
#define  __JANUS_UTILS_H_

#include <sys/time.h>
#include <string>

using namespace std;

unsigned long convert_timeval_to_us(const struct timeval *tv);
long convert_timeval_used_to_us(const struct timeval *tv1, const struct timeval *tv2);
int get_local_ip_addr(unsigned int& ip_addr);


#endif  //__JANUS_UTILS_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
