/**
 * @file janus_global.h
 * @author jessesleep@hotmail.com
 * @date 2017/01/16 19:23:12
 * @brief 
 *  
 **/




#ifndef  __JANUS_GLOBAL_H_
#define  __JANUS_GLOBAL_H_

#define LOCAL_IP_ADDR_PREFIX "127"
#define JANUS_REDIS_REPLY_OK "OK"

#define JANUS_RET_OK 0
#define JANUS_RET_ERR -1
#define JANUS_RET_UPDATE 1
#define JANUS_RET_REJECT 2

#define JANUS_LUA_ERRNO_ERROR "-1"
#define JANUS_LUA_ERRNO_OK "0"
#define JANUS_LUA_ERRNO_UPDATE "1"
#define JANUS_LUA_ERRNO_REJECT "2"

#endif  //__JANUS_GLOBAL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */