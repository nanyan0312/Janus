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

    /*
     * single process example
     */
    /*
    JanusLocker jlr("nylock", 1000000);
    jlr.register_node("127.0.0.1", 7771, "pay", "pay");
    jlr.register_node("127.0.0.1", 7772, "pay", "pay");
    jlr.register_node("127.0.0.1", 7773, "pay", "pay");
    jlr.register_script("./lua/demo.lua");
    jlr.acquire_lock(true);
    vector<string> argv;
    argv.push_back("first_argv");
    argv.push_back("second_argv");
    argv.push_back("last_argv");
    vector<vector<string> > replies;
    jlr.batch_exec_redis_script("./lua/demo.lua", 0, argv, replies, true);
    cout << "batch_exec_redis_script result:" << endl;
    for (unsigned int i = 0; i < replies.size(); i++) {
        cout << i << endl;
        for (unsigned int j = 0; j < replies[i].size(); j++) {
            cout << replies[i][j] << endl;
        }
    }
    jlr.release_lock();
    */
   
    /*
     * multi processes example
     */
    //fork workers to contend for lock
    int i = 0;
    while (i++ < 3) {
        pid_t pid = fork();
        if (-1 == pid) {
            return -1;
        } else if (0 < pid) {
        } else {
            cout << getpid() << " BORN." << endl;
            //init a locker by naming its target lock and setting a expiration time for the lock
            JanusLocker jlr("nylock", 1000000);

            //register the redis cluster
            jlr.register_node("127.0.0.1", 7771, "pay", "pay");
            jlr.register_node("127.0.0.1", 7772, "pay", "pay");
            jlr.register_node("127.0.0.1", 7773, "pay", "pay");

            //register scripts we are gonna use
            jlr.register_script("./lua/demo.lua");
            
            bool has_lock = false;
            while (true) {
                if (false == has_lock) {
                    has_lock = jlr.acquire_lock(true);
                    cout << getpid() << " ACQUIRED." << endl;
                } else {
                    has_lock = jlr.renew_lock();
                    if (false == has_lock) {
                        cout << getpid() << " RENEW FAILED." << endl;
                        continue;
                    }
                    cout << getpid() << " RENEWED." << endl;
                }

                //test the atomic lockcheck functionality
                vector<string> argv;
                argv.push_back("first_argv");
                argv.push_back("second_argv");
                argv.push_back("last_argv");
                vector<vector<string> > replies;
                jlr.batch_exec_redis_script("./lua/demo.lua", 0, argv, replies, true);
                cout << "batch_exec_redis_script result:" << endl;
                for (unsigned int i = 0; i < replies.size(); i++) {
                    cout << i << endl;
                    for (unsigned int j = 0; j < replies[i].size(); j++) {
                        cout << replies[i][j] << endl;
                    }
                }
                
                //simulate doing more work, 0.5 second
                usleep(500000);
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
