/**
 * @file janus_redis_cluster.h
 * @author jessesleep@hotmail.com
 * @date 2017/01/03 16:33:58
 * @brief 
 *  
 **/




#ifndef  __JANUS_REDIS_CLUSTER_H_
#define  __JANUS_REDIS_CLUSTER_H_

#include "stdlib.h"
#include "libae/ae.h"
#include "async.h"
#include "hiredis.h"
#include "adapters/ae.h"
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <fstream>

using namespace std;

typedef enum {
    CLUSTER_STATE_NONE,
    CLUSTER_STATE_CONNECT,
    CLUSTER_STATE_DISCONNECT,
    CLUSTER_STATE_READY,
    CLUSTER_STATE_WORKING,
} JanusRedisClusterState;


class JanusRedisNode {
    public:
        JanusRedisNode(const char *host, unsigned int port, const char *user, const char *password, bool need_auth, unsigned int index, void *parent);
        ~JanusRedisNode();

        int connect(aeEventLoop *loop);
        int disconnect();
        int auth();
        int load_script(string script);
        int execute(string command);
        void *get_parent();
        redisContext *get_rc();

    private:
        string _host;
        unsigned int _port;
        string _user;
        string _password;
        
        bool _need_auth;
        unsigned int _index;
        void *_parent;
        
        redisContext *_rc;
};



class JanusRedisCluster {
    public:
        JanusRedisCluster();
        ~JanusRedisCluster();
        
        int register_node(const char *host, unsigned int port, const char *user, const char *password, bool need_auth);
        int register_script(string script_pathname);
        int reconnect_cluster();
        int disconnect_cluster();
        int batch_exec_cmd(string command, vector<vector<string> >& replies);
        int batch_exec_script(string script_pathname, int num_keys, vector<string>& argv, vector<vector<string> >& replies);
        unsigned int get_cluster_size();
        unsigned int get_cluster_size_recveived();
        unsigned int get_incr_cluster_size_recveived();
        JanusRedisClusterState get_cluster_state();
        aeEventLoop *get_loop();
        vector<redisReply*> *get_vec_replies_ptr();

    private:
        int _auth_cluster();
        int _load_script_cluster();
        int _batch_exec(string command, vector<redisReply*> *replies);
        
    private:
        vector<JanusRedisNode*> _cluster;
        unsigned int _cluster_seq;
        unsigned int _cluster_size;
        unsigned int _cluster_size_recveived;
        
        JanusRedisClusterState _cluster_state;
        aeEventLoop *_loop;
        vector<redisReply*> *_vec_replies_ptr;
        map<string, string> _cluster_script_sha_map;
};

#endif  //__JANUS_REDIS_CLUSTER_H_

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
