/**
 * @file janus_utils.cpp
 * @author jessesleep@hotmail.com
 * @date 2017/01/13 11:45:38
 * @brief 
 *  
 **/

#include "janus_utils.h"
#include "janus_globals.h"
#include <stropts.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netdevice.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

unsigned long convert_timeval_to_us(const struct timeval *tv) {
    return tv->tv_sec * 1000000 + tv->tv_usec;
}

long convert_timeval_used_to_us(const struct timeval *tv1, const struct timeval *tv2) {
    return (tv2->tv_sec - tv1->tv_sec) * 1000000 + (tv2->tv_usec - tv1->tv_usec);
}

int get_local_ip_addr(unsigned int& ip_addr) {
    int s;
    struct ifconf ifconf;
    struct ifreq ifr[50];
    int ifs;
    int i;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return JANUS_RET_ERR;
    }

    ifconf.ifc_buf = (char *) ifr;
    ifconf.ifc_len = sizeof ifr;
    if (-1 == ioctl(s, SIOCGIFCONF, &ifconf)) {
        return JANUS_RET_ERR;
    }

    ifs = ifconf.ifc_len / sizeof(ifr[0]);
    for (i = 0; i < ifs; i++) {
        char ip[INET_ADDRSTRLEN];
        struct sockaddr_in *s_in = (struct sockaddr_in *) &ifr[i].ifr_addr;
        if (NULL == inet_ntop(AF_INET, &s_in->sin_addr, ip, sizeof(ip))) {
            return JANUS_RET_ERR;
        }

        string tmp(ip);
        if (LOCAL_IP_ADDR_PREFIX == tmp.substr(0, 3)) {
            continue;
        }

        if (1 != inet_pton(AF_INET, ip, &ip_addr)) {
            return JANUS_RET_ERR;
        }
        
        break;
    }

    close(s);

    return JANUS_RET_OK;
}


/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
