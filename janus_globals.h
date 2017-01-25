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
#define JANUS_INSTANCE_ID_MEGAJUMP 1000000

#define JANUS_RET_OK 0
#define JANUS_RET_ERR -1
#define JANUS_RET_UPDATE 1
#define JANUS_RET_REJECT 2
#define JANUS_RET_UNALLOWED 3
#define JANUS_RET_NOLOCK 4

#define JANUS_LUA_ERRNO_ERROR "-1"
#define JANUS_LUA_ERRNO_OK "0"
#define JANUS_LUA_ERRNO_UPDATE "1"
#define JANUS_LUA_ERRNO_REJECT "2"
#define JANUS_LUA_ERRNO_UNALLOWED "3"

#define JANUS_BUILTIN_ATOMIC_LOCKCHECK_YES "janus_builtin_atomic_lockcheck_yes"
#define JANUS_BUILTIN_ATOMIC_LOCKCHECK_NO "janus_builtin_atomic_lockcheck_no"
#define JANUS_BUILTIN_ATOMIC_LOCKCHECK_FAILED "janus_builtin_atomic_lockcheck_failed"

#endif  //__JANUS_GLOBAL_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
