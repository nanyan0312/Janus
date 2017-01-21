/**
 * @file janus_locker.h
 * @author jessesleep@hotmail.com
 * @date 2017/01/03 12:07:02
 * @brief 
 *  
 **/

#ifndef  __JANUS_LOCKER_H_
#define  __JANUS_LOCKER_H_

#include "janus_lock.h"
#include "janus_redis_cluster.h"
#include <string>

using namespace std;

typedef enum {
    LOOKING,
    LEADING,
    FOLLOWING
} LOCKER_STATE;

class JanusLocker {
    public:
        JanusLocker(JanusLock *lock, JanusRedisCluster *rc);
        ~JanusLocker();
        bool acquire_lock(bool blocking);
        bool renew_lock();
        bool release_lock();

    private:
        int _do_paxos_prepare(unsigned int& updated_instance_id, bool& accepted, string& accepted_value);
        int _do_paxos_accept(string& proposal_value, unsigned int& updated_instance_id);
        int _do_renew(unsigned int& updated_instance_id);
        int _do_check(unsigned int& updated_instance_id, unsigned int& updated_last_seq_num);
        string _make_locker_name(); 
        void _make_proposal_id(); 
        unsigned long _make_now_us(); 
        
        JanusLock *_lock;
        JanusRedisCluster *_rc;

        int _own_pid;
        unsigned int _own_ip_addr;
        string _own_name;
        LOCKER_STATE _own_state;
        unsigned int _instance_id;
        unsigned long _proposal_id[2];
        unsigned int _last_seq_num;

        /*all time in microsecs, measured since Epoch*/
        unsigned long  _now_us;
        unsigned long  _last_fail_time_us;
        unsigned long  _last_renew_time_us;
        unsigned long  _last_check_time_us;
};





#endif  //__JANUS_LOCKER_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
