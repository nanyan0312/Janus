/**
 * @file demo.cpp
 * @author jessesleep@hotmail.com
 * @date 2017/01/12 14:38:21
 * @brief 
 *  
 **/

#include <janus_lock.h>
#include <janus_redis_cluster.h>
#include <janus_locker.h>
#include <fstream>
#include <iostream>

using namespace std;

int main(int argc, char**) {

    //init the lock object, second argument means we want lock lease to be 1 second
    JanusLock jl("nylock", 1000000);

    //init the redis cluster object
    JanusRedisCluster jrc;
    jrc.register_node("127.0.0.1", 7771, "pay", "pay");
    jrc.register_node("127.0.0.1", 7772, "pay", "pay");
    jrc.register_node("127.0.0.1", 7773, "pay", "pay");
    jrc.register_script("./lua/janus_paxos_prepare.lua");
    jrc.register_script("./lua/janus_paxos_accept.lua");
    jrc.register_script("./lua/janus_renew.lua");
    jrc.register_script("./lua/janus_check.lua");

    //fork worker to contend for lock
    int i = 0;
    while (i++ < 3) {
        pid_t pid = fork();
        if (-1 == pid) {
            return -1;
        } else if (0 < pid) {
        } else {
            cout << getpid() << " BORN." << endl;
            JanusLocker jlr(&jl, &jrc);
            bool has_lock = false;
            while (true) {
                if (false == has_lock) {
                    has_lock = jlr.acquire_lock(true);
                } else {
                    has_lock = jlr.renew_lock();
                }

                if (false == has_lock) {
                    continue;
                }

                //simulate doing work, 0.1 second
                usleep(10000);
            }
        }
    }

    //master lives forever
    while (true) {
        sleep(100);
    }

    return 0;
}




/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
