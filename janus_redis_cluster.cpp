/**
 * @file janus_redis_cluster.cpp
 * @author jessesleep@hotmail.com
 * @date 2017/01/06 14:37:36
 * @brief 
 *  
 **/

#include "janus_redis_cluster.h"
#include "janus_globals.h"

static void copy_redis_reply(redisReply *dest, redisReply *src) {
    if (NULL == dest || NULL == src) {
        return;
    }

    dest->type = src->type;
    dest->integer = src->integer;
    dest->len = src->len;
    dest->str = (char*) malloc(dest->len);
    memcpy(dest->str, src->str, src->len);
    
    dest->elements = src->elements;
    dest->element = (struct redisReply**) calloc(1, sizeof(struct redisReply*) * src->elements);
    for (size_t i = 0; i < src->elements; i++) {
        dest->element[i] = (struct redisReply*) calloc(1, sizeof(struct redisReply));
        copy_redis_reply(dest->element[i], src->element[i]);
    }

    return;
}

static void g_node_connect_callback(const redisAsyncContext *c, int status) {
    JanusRedisNode *node = (JanusRedisNode*) c->data;
    JanusRedisCluster *cluster = (JanusRedisCluster*) node->get_parent();

    /*retry when failed*/
    if (REDIS_OK != status) {
        node->connect(cluster->get_loop());
        return;
    }

    /*stop the cluster's connecting when all nodes received*/
    if (CLUSTER_STATE_CONNECT == cluster->get_cluster_state()
            && cluster->get_incr_cluster_size_recveived() == cluster->get_cluster_size()) {
        aeStop(cluster->get_loop());
    }

    return;
}

static void g_node_disconnect_callback(const redisAsyncContext *c, int status) {
    JanusRedisNode *node = (JanusRedisNode*) c->data;
    JanusRedisCluster *cluster = (JanusRedisCluster*) node->get_parent();

    /*no auto reconnect when disconnecting or working*/
    JanusRedisClusterState state = cluster->get_cluster_state();
    if (REDIS_OK != status 
            && CLUSTER_STATE_DISCONNECT != state
            && CLUSTER_STATE_WORKING != state) {
        node->connect(cluster->get_loop());
        return;
    }

    /*
     * when disconnecting or working
     * count this disconnect as received
     * stop the main loop if all received
     * */
    if ((CLUSTER_STATE_DISCONNECT == state
                || CLUSTER_STATE_WORKING == state)
            && cluster->get_incr_cluster_size_recveived() == cluster->get_cluster_size()) {
        aeStop(cluster->get_loop());
    }

    return;
}

static void g_node_auth_callback(redisAsyncContext *c, void *reply, void *privdata) {
    JanusRedisNode *node = (JanusRedisNode*) c->data;
    JanusRedisCluster *cluster = (JanusRedisCluster*) node->get_parent();
    
    redisReply *r = (redisReply*) reply;
    /*copy and save all non-null replies*/
    if (NULL != r) {
        redisReply *tmp = (redisReply*) calloc(1, sizeof(redisReply));
        copy_redis_reply(tmp, r);
        cluster->get_vec_replies_ptr()->push_back(tmp);
    }
    
    /*stop the cluster's working when all nodes received*/
    JanusRedisClusterState state = cluster->get_cluster_state();
    if (CLUSTER_STATE_WORKING == state
            && cluster->get_incr_cluster_size_recveived() == cluster->get_cluster_size()) {
        aeStop(cluster->get_loop());
    }

    return;
}

static void g_node_load_script_callback(redisAsyncContext *c, void *reply, void *privdata) {
    JanusRedisNode *node = (JanusRedisNode*) c->data;
    JanusRedisCluster *cluster = (JanusRedisCluster*) node->get_parent();
    
    redisReply *r = (redisReply*) reply;
    /*copy and save all non-null replies*/
    if (NULL != r) {
        redisReply *tmp = (redisReply*) calloc(1, sizeof(redisReply));
        copy_redis_reply(tmp, r);
        cluster->get_vec_replies_ptr()->push_back(tmp);
    }
    
    /*stop the cluster's working when all nodes received*/
    JanusRedisClusterState state = cluster->get_cluster_state();
    if (CLUSTER_STATE_WORKING == state
            && cluster->get_incr_cluster_size_recveived() == cluster->get_cluster_size()) {
        aeStop(cluster->get_loop());
    }

    return;
}

static void g_node_execute_callback(redisAsyncContext *c, void *reply, void *privdata) {
    JanusRedisNode *node = (JanusRedisNode*) c->data;
    JanusRedisCluster *cluster = (JanusRedisCluster*) node->get_parent();
    
    redisReply *r = (redisReply*) reply;
    /*copy and save all non-null replies*/
    if (NULL != r) {
        redisReply *tmp = (redisReply*) calloc(1, sizeof(redisReply));
        copy_redis_reply(tmp, r);
        cluster->get_vec_replies_ptr()->push_back(tmp);
    }
    
    /*stop the cluster's working when all nodes received*/
    JanusRedisClusterState state = cluster->get_cluster_state();
    if (CLUSTER_STATE_WORKING == state
            && cluster->get_incr_cluster_size_recveived() == cluster->get_cluster_size()) {
        aeStop(cluster->get_loop());
    }

    return;
}


JanusRedisNode::JanusRedisNode(const char *host, unsigned int port, const char *user, 
        const char *password, bool need_auth, unsigned int index, void *parent) { 
        _host = host;
        _port = port;
        _user = user;
        _password = password;
        _need_auth = need_auth;
        _index = index;
        _parent = parent;
        _rc = NULL;
}

JanusRedisNode::~JanusRedisNode() {
}

int JanusRedisNode::connect(aeEventLoop *loop) {
    if (NULL != _rc) {
        return JANUS_RET_ERR;
    }

    redisAsyncContext *rc = redisAsyncConnect(_host.c_str(), _port);
    if (rc->err) {
        return JANUS_RET_ERR;
    }

    _rc = (redisContext*) rc;
    rc->data = (void*) this;
    redisAeAttach(loop, rc);
    redisAsyncSetConnectCallback(rc, g_node_connect_callback);
    redisAsyncSetDisconnectCallback(rc, g_node_disconnect_callback);

    return JANUS_RET_OK;
}

int JanusRedisNode::disconnect() {
    if (NULL == _rc) {
        return JANUS_RET_ERR;
    }

    redisAsyncDisconnect((redisAsyncContext*) _rc);
    _rc = NULL;

    return JANUS_RET_OK;
}

int JanusRedisNode::auth() {
    if (NULL == _rc) {
        return JANUS_RET_ERR;
    }

    if (false == _need_auth) {
        return JANUS_RET_ERR;
    }

    string command = "AUTH " + _password;

    if (REDIS_OK != redisAsyncCommand((redisAsyncContext*) _rc, 
                g_node_auth_callback, NULL, command.c_str())) {
        return JANUS_RET_ERR;
    }

    return JANUS_RET_OK;
}

int JanusRedisNode::load_script(string script) {
    if (0 == script.length() || NULL == _rc) {
        return JANUS_RET_ERR;
    }

    if (REDIS_OK != redisAsyncCommand((redisAsyncContext*) _rc, 
                g_node_load_script_callback, NULL, "SCRIPT LOAD %s", script.c_str())) {
        return JANUS_RET_ERR;
    }

    return JANUS_RET_OK;
}

int JanusRedisNode::execute(string command) {
    if (0 == command.length() || NULL == _rc) {
        return JANUS_RET_ERR;
    }

    if (REDIS_OK != redisAsyncCommand((redisAsyncContext*) _rc, 
                g_node_execute_callback, NULL, command.c_str())) {
        return JANUS_RET_ERR;
    }

    return JANUS_RET_OK;
}

void *JanusRedisNode::get_parent() {
    return _parent;
}

redisContext *JanusRedisNode::get_rc() {
    return _rc;
}



JanusRedisCluster::JanusRedisCluster() {
    _cluster.clear();
    _cluster_seq = 0;
    _cluster_size = 0;
    _cluster_size_recveived = 0;
    _loop = aeCreateEventLoop(64);
    _cluster_state = CLUSTER_STATE_NONE;
    _vec_replies_ptr = NULL;
    _cluster_script_sha_map.clear();
}

JanusRedisCluster::~JanusRedisCluster() {
    /*disconnect cluster if not already done*/
    if (CLUSTER_STATE_NONE != _cluster_state) {
        _cluster_state = CLUSTER_STATE_NONE;
        disconnect_cluster();
    }

    for (unsigned int i = 0; i < _cluster.size(); i++) {
        delete _cluster[i];
    }
}

int JanusRedisCluster::register_node(const char *host, unsigned int port, const char *user, const char *password, bool need_auth) {
    if (NULL == host || NULL == user || NULL == password) {
        return JANUS_RET_ERR;
    }

    JanusRedisNode *node = new JanusRedisNode(host, port, user, password, need_auth, _cluster_seq++, (void*) this);
    _cluster_size++; 
    _cluster.push_back(node);

    return JANUS_RET_OK;
}

int JanusRedisCluster::register_script(string script_pathname) {
    unsigned int s_size = script_pathname.size();
    if (0 == s_size) {
        return JANUS_RET_ERR;
    }

    map<string, string>::iterator it = _cluster_script_sha_map.find(script_pathname);
    if (_cluster_script_sha_map.end() == it) {
        _cluster_script_sha_map[script_pathname] = "";
    }
    
    return JANUS_RET_OK;
}

int JanusRedisCluster::reconnect_cluster() {
    /*disconnect cluster*/
    int ret = disconnect_cluster();
    if (JANUS_RET_ERR == ret) {
        return JANUS_RET_ERR;
    }

    /*statistics have to be accurate*/
    unsigned int s = _cluster.size();
    if (s != _cluster_size) {
        return JANUS_RET_ERR;
    }

    /*check cluster's state machine*/
    if (CLUSTER_STATE_NONE != _cluster_state) {
        return JANUS_RET_OK;
    }

    /*reconnect all registered nodes*/
    _cluster_state = CLUSTER_STATE_CONNECT;
    _cluster_size_recveived = 0;
    for (unsigned int i = 0; i < _cluster_size; i++) {
        if (JANUS_RET_ERR == _cluster[i]->connect(_loop)) {
            _cluster_size_recveived++;
        }
    }

    while (_cluster_size_recveived != _cluster_size) {
        aeMain(_loop);
    }

    _cluster_state = CLUSTER_STATE_READY;

    if (- 1== _auth_cluster()) {
        return JANUS_RET_ERR;
    }

    return _load_script_cluster();
}

int JanusRedisCluster::disconnect_cluster() {
    /*statistics have to be accurate*/
    unsigned int s = _cluster.size();
    if (s != _cluster_size) {
        return JANUS_RET_ERR;
    }

    /*check cluster's state machine*/
    if (CLUSTER_STATE_READY != _cluster_state) {
        return JANUS_RET_OK;
    }

    /*disconnect all currently connected nodes*/
    _cluster_state = CLUSTER_STATE_DISCONNECT;
    _cluster_size_recveived = 0;
    for (unsigned int i = 0; i < _cluster_size; i++) {
        if (JANUS_RET_ERR == _cluster[i]->disconnect()) {
            _cluster_size_recveived++;
        }
    }

    while (_cluster_size_recveived != _cluster_size) {
        aeMain(_loop);
    }

    _cluster_state = CLUSTER_STATE_NONE;
    return JANUS_RET_OK;
}

int JanusRedisCluster::batch_exec_cmd(string command, vector<vector<string> >& replies) {
    vector<redisReply*> r_replies;
    int ret = _batch_exec(command, &r_replies);
    if (JANUS_RET_ERR == ret) {
        return JANUS_RET_ERR;
    }

    unsigned int vec_size = r_replies.size();
    redisReply *r = NULL;
    vector<string> reply;
    for (unsigned int i = 0; i < vec_size; i++) {
        r = r_replies[i];
        reply.clear();
        if (NULL == r) {
            continue;
        } else if (REDIS_REPLY_ERROR == r->type && 0 < r->len) {
            string tmp(r->str, r->len);
            reply.push_back(tmp);
            replies.push_back(reply);
        } else if (REDIS_REPLY_ARRAY == r->type) {
            for (size_t j = 0; j < r->elements; j++) {
                redisReply *cr = r->element[j];
                if (REDIS_REPLY_STRING == cr->type) {
                    string tmp(cr->str, cr->len);
                    reply.push_back(tmp);
                } else if (REDIS_REPLY_INTEGER == cr->type) {
                    stringstream convert;
                    convert << cr->integer;
                    reply.push_back(convert.str());
                } else {
                    return JANUS_RET_ERR;
                }
            }

            replies.push_back(reply);
        } else {
            continue;
        }
    }

    return JANUS_RET_OK;
}

int JanusRedisCluster::batch_exec_script(string script_pathname, int num_keys, vector<string>& argv, vector<vector<string> >& replies) {
    unsigned int s_size = script_pathname.size();
    if (0 == s_size || 0 > num_keys) {
        return JANUS_RET_ERR;
    }
    
    /*check cluster's state machine*/
    if (CLUSTER_STATE_NONE == _cluster_state) {
        if (JANUS_RET_ERR == reconnect_cluster()) {
            return JANUS_RET_ERR;
        }
    }
    
    if (CLUSTER_STATE_READY != _cluster_state) {
        return JANUS_RET_ERR;
    }

    map<string, string>::iterator it = _cluster_script_sha_map.find(script_pathname);
    if (_cluster_script_sha_map.end() == it) {
        return JANUS_RET_ERR;
    }
    
    stringstream cmd;
    cmd << "EVALSHA " << it->second << " " << num_keys;
    unsigned int argc = argv.size();
    for (unsigned int i = 0; i < argc; i++) {
        cmd << " " << argv[i];
    }
    
    return batch_exec_cmd(cmd.str(), replies);
}

unsigned int JanusRedisCluster::get_cluster_size() {
    return _cluster_size;
}

unsigned int JanusRedisCluster::get_cluster_size_recveived() {
    return _cluster_size_recveived;
}

unsigned int JanusRedisCluster::get_incr_cluster_size_recveived() {
    return ++_cluster_size_recveived;
}

JanusRedisClusterState JanusRedisCluster::get_cluster_state() {
    return _cluster_state;
}

aeEventLoop *JanusRedisCluster::get_loop() {
    return _loop;
}

vector<redisReply*> *JanusRedisCluster::get_vec_replies_ptr() {
    return _vec_replies_ptr;
}

int JanusRedisCluster::_auth_cluster() {
    /*statistics have to be accurate*/
    unsigned int s = _cluster.size();
    if (s != _cluster_size ) {
        return JANUS_RET_ERR;
    }
    
    if (CLUSTER_STATE_READY != _cluster_state) {
        return JANUS_RET_ERR;
    }

    /*authenticate all currently connected nodes*/
    _cluster_state = CLUSTER_STATE_WORKING;
    _cluster_size_recveived = 0;
    vector<redisReply*> r_replies;
    _vec_replies_ptr = &r_replies;
    for (unsigned int i = 0; i < _cluster_size; i++) {
        if (JANUS_RET_ERR == _cluster[i]->auth()) {
            _cluster_size_recveived++;
        }
    }

    while (_cluster_size_recveived != _cluster_size) {
        aeMain(_loop);
    }

    _cluster_state = CLUSTER_STATE_READY;
   
    /*check if all auth OK*/ 
    unsigned int vec_size = r_replies.size();
    redisReply *r = NULL;
    unsigned int ok = 0;
    for (unsigned int i = 0; i < vec_size; i++) {
        r = r_replies[i];
        if (NULL != r && REDIS_REPLY_STATUS == r->type && 0 < r->len) {
            if (0 == strncmp(r->str, JANUS_REDIS_REPLY_OK, r->len)) {
                ok++;
            }
        }
    }

    if (_cluster_size != ok) {
        return JANUS_RET_ERR;
    }

    return JANUS_RET_OK;
}

int JanusRedisCluster::_load_script_cluster() {
    /*statistics have to be accurate*/
    unsigned int s = _cluster.size();
    if (s != _cluster_size ) {
        return JANUS_RET_ERR;
    }
    
    if (CLUSTER_STATE_READY != _cluster_state) {
        return JANUS_RET_ERR;
    }

    /*load all scripts to all currently connected nodes*/
    _cluster_state = CLUSTER_STATE_WORKING;
    vector<redisReply*> r_replies;
    map<string, string>::iterator it = _cluster_script_sha_map.begin();
    for (; it != _cluster_script_sha_map.end(); it++) {
        ifstream script_file(it->first.c_str());
        stringstream cmd;
        cmd << script_file.rdbuf();
        r_replies.clear();
        _vec_replies_ptr = &r_replies;
        _cluster_size_recveived = 0;

        for (unsigned int i = 0; i < _cluster_size; i++) {
            if (JANUS_RET_ERR == _cluster[i]->load_script(cmd.str())) {
                _cluster_size_recveived++;
            }
        }

        while (_cluster_size_recveived != _cluster_size) {
            aeMain(_loop);
        }

        /*check if all load OK and cache scripts sha*/ 
        unsigned int vec_size = r_replies.size();
        redisReply *r = NULL;
        unsigned int ok = 0;
        string sha;
        for (unsigned int i = 0; i < vec_size; i++) {
            r = r_replies[i];
            if (NULL != r && REDIS_REPLY_STRING == r->type && 0 < r->len) {
                string tmp(r->str, r->len);
                if (true == sha.empty()) {
                    sha = tmp;
                } else if (tmp != sha) {
                    continue;
                }
                ok++;
            }
        }

        if (_cluster_size != ok) {
            return JANUS_RET_ERR;
        }

        it->second = sha;
    }

    _cluster_state = CLUSTER_STATE_READY;

    return JANUS_RET_OK;
}

int JanusRedisCluster::_batch_exec(string command, vector<redisReply*> *replies) {
    if (0 == command.length()) {
        return JANUS_RET_ERR;
    }

    /*statistics have to be accurate*/
    unsigned int s = _cluster.size();
    if (s != _cluster_size ) {
        return JANUS_RET_ERR;
    }

    /*check cluster's state machine*/
    if (CLUSTER_STATE_NONE == _cluster_state) {
        if (JANUS_RET_ERR == reconnect_cluster()) {
            return JANUS_RET_ERR;
        }
    }
    
    if (CLUSTER_STATE_READY != _cluster_state) {
        return JANUS_RET_ERR;
    }

    _cluster_state = CLUSTER_STATE_WORKING;
    _cluster_size_recveived = 0;
    _vec_replies_ptr = replies;
    for (unsigned int i = 0; i < _cluster_size; i++) {
        if (JANUS_RET_ERR == _cluster[i]->execute(command)) {
            _cluster_size_recveived++;
        }
    }

    while (_cluster_size_recveived != _cluster_size) {
        aeMain(_loop);
    }

    _cluster_state = CLUSTER_STATE_READY;
    
    return JANUS_RET_OK;
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
