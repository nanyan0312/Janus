/**
 * @file janus_locker.cpp
 * @author jessesleep@hotmail.com
 * @date 2017/01/12 17:39:41
 * @brief 
 *  
 **/

#include "janus_locker.h"
#include "janus_utils.h"
#include "janus_globals.h"
#include <sys/time.h>
#include <unistd.h>
#include <iostream>

JanusLocker::JanusLocker(const string lock_name, const unsigned long lock_timeout_in_us) 
    : _lock(new JanusLock(lock_name, lock_timeout_in_us)), _rc(new JanusRedisCluster()) {
        _now_us = _make_now_us();
        _own_pid = getpid();
        get_local_ip_addr(_own_ip_addr);
        _own_name = _make_locker_name();
        _own_state = LOOKING;
        _instance_id = 0;
        _proposal_id[0] = _proposal_id[1] = 0; 
        _last_seq_num = 0;

        _last_fail_time_us = _now_us;
        _last_renew_time_us = _last_fail_time_us; 
        _last_check_time_us = _last_fail_time_us; 
    }

JanusLocker::~JanusLocker() {
    _lock = NULL;
    _rc = NULL;
}

void JanusLocker::setLockTimeout(unsigned long lock_timeout_in_us) {
    _lock->setTimeout(lock_timeout_in_us);
}

int JanusLocker::register_node(const char *host, unsigned int port, const char *user, const char *password, bool need_auth) {
    return _rc->register_node(host, port, user, password, need_auth);
}

int JanusLocker::register_script(string script_pathname) {
    return _rc->register_script(script_pathname);
}

bool JanusLocker::acquire_lock(bool blocking) {
    unsigned long lock_timeout_backoff_us = _lock->get_timeout_backoff_us(); 
    unsigned long lock_timeout_renew_us = _lock->get_timeout_renew_us(); 
    unsigned long lock_timeout_check_us = _lock->get_timeout_check_us();

    while (true) {
        _now_us = _make_now_us();

        if (LOOKING == _own_state) {/*run a single-instance paxos, find out the lock owner for my instance*/
            if (lock_timeout_backoff_us > (_now_us - _last_fail_time_us)) {
                if (true == blocking) { usleep(lock_timeout_backoff_us); } else { return false;}
            }

            /*paxos prepare phase*/
            unsigned int updated_instance_id;
            bool accepted = false;
            string accepted_value;
            _make_proposal_id();
            int ret = _do_paxos_prepare(updated_instance_id, accepted, accepted_value);
            if (JANUS_RET_ERR == ret) {/*terminate if unexpected errors happen*/
                cout << getpid() << "----" << "_do_paxos_prepare err" << endl;
                return false; 
            } else if (JANUS_RET_UPDATE == ret) {/*higher instance exists, jump to that instance next time*/
                cout << getpid() << "----" << "_do_paxos_prepare update " << updated_instance_id << endl;
                _instance_id = updated_instance_id;
            } else if (JANUS_RET_OK == ret) {/*got majority oks, proceeed to paxos accept phase*/
                cout << getpid() << "----" << "_do_paxos_prepare ok accepted_value:" << accepted_value << endl;
                string proposal_value = (true == accepted) ? accepted_value : _own_name;
                ret = _do_paxos_accept(proposal_value, updated_instance_id);
                if (JANUS_RET_ERR == ret) {/*terminate if unexpected errors happen*/
                    cout << getpid() << "----" << "_do_paxos_accept err" << endl;
                    return false; 
                } else if (JANUS_RET_UPDATE == ret) {/*higher instance exists, jump to that instance next time*/
                    cout << getpid() << "----" << "_do_paxos_accept update " << updated_instance_id << endl;
                    _instance_id = updated_instance_id;
                } else if (JANUS_RET_OK == ret) {/*got majority oks, meaning proposal accepted, lets check it*/
                    cout << getpid() << "----" << "_do_paxos_accept ok proposal_value:" << proposal_value << endl;
                    if (_own_name == proposal_value) {
                        _own_state = LEADING;
                        _last_renew_time_us = _make_now_us();
                        return true;/*got that shit, go to work*/
                    } else {/*this is the only chance that a locker becomes a follower*/
                        _own_state = FOLLOWING;
                        _last_check_time_us = _make_now_us();
                        _last_seq_num = 0;
                    }
                } else {/*accept phase failed, try with higher proposal id next time*/
                    cout << getpid() << "----" << "_do_paxos_accept failed" << endl;
                    _last_fail_time_us = _make_now_us();
                }
            } else {/*prepare phase failed, try with higher proposal id next time*/
                cout << getpid() << "----" << "_do_paxos_prepare failed" << endl;
                _last_fail_time_us = _make_now_us();
            }
        } else if (LEADING == _own_state) {
            if (lock_timeout_renew_us > (_now_us - _last_renew_time_us)) {
                return true;
            }

            /*renew lock ownership, this is the only chance a seq_num gets incremented*/
            unsigned int updated_instance_id;
            int ret = _do_renew(updated_instance_id);
            if (JANUS_RET_ERR == ret) {/*terminate if unexpected errors happen*/
                cout << getpid() << "----" << "_do_renew err" << endl;
                return false; 
            } else if (JANUS_RET_UPDATE == ret) {/*higher instance exists, become LOOKING, jump to that instance next time*/
                cout << getpid() << "----" << "_do_renew update " << updated_instance_id << endl;
                _instance_id = updated_instance_id;
            } else if (JANUS_RET_OK == ret) {/*got majority oks, renewal successful*/
                cout << getpid() << "----" << "_do_renew ok " << endl;
                _last_renew_time_us = _make_now_us();
                return true;/*still got that shit, go to work*/
            }

            cout << getpid() << "----" << "_do_renew failed " << endl;
            _own_state = LOOKING;/*failed, give up lock voluntarily*/
        } else if (FOLLOWING == _own_state) {
            if (lock_timeout_check_us > (_now_us - _last_check_time_us)) {
                if (true == blocking) { usleep(lock_timeout_check_us); } else { return false;}
            }

            /*check if lock owner still alive*/
            unsigned int updated_instance_id;
            unsigned int updated_last_seq_num;
            int ret = _do_check(updated_instance_id, updated_last_seq_num);
            if (JANUS_RET_ERR == ret) {/*terminate if unexpected errors happen*/
                cout << getpid() << "----" << "_do_check err" << endl;
                return false; 
            } else if (JANUS_RET_UPDATE == ret) {/*higher instance exists, become LOOKING, jump to that instance next time*/
                cout << getpid() << "----" << "_do_check update " << updated_instance_id << endl;
                _own_state = LOOKING;
                _instance_id = updated_instance_id;
            } else if (JANUS_RET_OK == ret) {/*got majority oks, lock owner still alive*/
                cout << getpid() << "----" << "_do_check ok " << endl;
                _last_check_time_us = _make_now_us();
                _last_seq_num = updated_last_seq_num;
            } else {/*lock owner might be dead, how about me?*/
                cout << getpid() << "----" << "_do_check failed " << endl;
                _own_state = LOOKING;
                _instance_id += 1;
                continue;/*start next round immediately*/
            }
        }

        if (true == blocking) { usleep(100); } else { return false;}
    }
}

bool JanusLocker::renew_lock() {
    if (LEADING != _own_state) {
        return false;
    }
    
    unsigned long lock_timeout_renew_us = _lock->get_timeout_renew_us(); 
    if (lock_timeout_renew_us > (_now_us - _last_renew_time_us)) {
        return true;
    }

    /*renew lock ownership, this is the only chance a seq_num gets incremented*/
    unsigned int updated_instance_id;
    int ret = _do_renew(updated_instance_id);
    if (JANUS_RET_ERR == ret) {/*terminate if unexpected errors happen*/
        cout << getpid() << "----" << "renew_lock _do_renew err" << endl;
        return false; 
    } else if (JANUS_RET_UPDATE == ret) {/*higher instance exists, deemed as renewal failure*/
        cout << getpid() << "----" << "renew_lock _do_renew update " << updated_instance_id << endl;
        _instance_id = updated_instance_id;
    } else if (JANUS_RET_OK == ret) {/*got majority oks, renewal successful*/
        cout << getpid() << "----" << "renew_lock _do_renew ok " << endl;
        _last_renew_time_us = _make_now_us();
        return true;/*still got that shit, go to work*/
    }

    cout << getpid() << "----" << "_do_renew failed " << endl;
    _own_state = LOOKING;/*failed, give up lock voluntarily*/
    return false;
}

bool JanusLocker::release_lock() {
    /*
     * release_lock always succeed
     * if lock owner calls to release, give up lock by becoming candidate
     * other roles calls to release, return immediately
     */
    if (LEADING == _own_state) {
        _own_state = LOOKING;
    }

    return true;
}

int JanusLocker::batch_exec_redis_script(string script_pathname, int num_keys, vector<string>& argv, vector<vector<string> >& replies, bool with_lock) {
    string atomic_lockcheck_switch = (true == with_lock) ? JANUS_BUILTIN_ATOMIC_LOCKCHECK_YES : JANUS_BUILTIN_ATOMIC_LOCKCHECK_NO;
    argv.push_back(atomic_lockcheck_switch);
    argv.push_back(_lock->get_lock_name());
    stringstream convert;
    convert << _instance_id;
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    int ret = _rc->batch_exec_script(script_pathname, 0, argv, replies);
    if (JANUS_RET_ERR == ret) {
        return JANUS_RET_ERR;
    }

    /*if no lockcheck, return*/
    if (false == with_lock) {
        return JANUS_RET_OK;
    }

    /*if has lockcheck, make sure all lockchecks passed on all redis nodes*/
    bool error = false;
    unsigned int vec_vec_size = replies.size();
    vector<string> reply;
    unsigned int vec_size;
    for (unsigned int i = 0; i < vec_vec_size; i++) {
        reply = replies[i];
        vec_size = reply.size();
        if (2 == vec_size && JANUS_LUA_ERRNO_ERROR == reply[0]
                && JANUS_BUILTIN_ATOMIC_LOCKCHECK_FAILED == reply[1].substr(0, strlen(JANUS_BUILTIN_ATOMIC_LOCKCHECK_FAILED))) {
            error = true;
            break;
        }
    }

    if (true == error) {
        return JANUS_RET_ERR;
    }

    return JANUS_RET_OK; 
}

int JanusLocker::_do_paxos_prepare(unsigned int& updated_instance_id, bool& accepted, string& accepted_value) {
    vector<string> argv;
    stringstream convert;

    argv.push_back(_lock->get_lock_name());

    convert << _instance_id;
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    convert << _proposal_id[0];
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    convert << _proposal_id[1];
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    vector<vector<string> > replies;
    int ret = _rc->batch_exec_script("janus_builtin_paxos_prepare.lua", 0, argv, replies);
    if (JANUS_RET_ERR == ret) {
        return JANUS_RET_ERR;
    }

    /*
     * parse paxos prepare result:
     * 1)if any error, return JANUS_RET_ERR
     * 2)if any higher instance exists, return JANUS_RET_UPDATE, along with the updated instance id
     * 3)if got less than majority oks, return JANUS_RET_REJECT
     * 4)if got majority oks, record accepted proposal if any exists
     *   if multiple accepted proposals, choose one with largest proposal id
     */
    unsigned int ok = 0;
    bool update = false;
    bool error = false;
    accepted = false;
    unsigned long accepted_proposal_id[2] = {0};
    unsigned long largest_proposal_id[2] = {0};
    unsigned int vec_vec_size = replies.size();
    vector<string> reply;
    unsigned int vec_size;
    for (unsigned int i = 0; i < vec_vec_size; i++) {
        reply = replies[i];
        vec_size = reply.size();
        if (1 > vec_size) {
            continue;
        }

        if (JANUS_LUA_ERRNO_ERROR == reply[0]) {
            error = true;
            break;
        } else if (JANUS_LUA_ERRNO_UPDATE == reply[0]) {
            if (3 == vec_size && false == reply[2].empty()) {
                stringstream updated_ins_id(reply[2]);
                updated_ins_id >> updated_instance_id; 
                update = true;
                break;
            } else {
                error = true;
                break;
            }
        } else if (JANUS_LUA_ERRNO_OK == reply[0]) {
            ok++;
            if (5 == vec_size && false == reply[2].empty()) {
                accepted = true;
                stringstream prop_id_fir(reply[2]);
                stringstream prop_id_sec(reply[3]);
                prop_id_fir >> accepted_proposal_id[0]; 
                prop_id_sec >> accepted_proposal_id[1]; 

                if (accepted_proposal_id[0] > largest_proposal_id[0]
                        || (accepted_proposal_id[0] == largest_proposal_id[0] && accepted_proposal_id[1] > largest_proposal_id[1])) {
                    largest_proposal_id[0] = accepted_proposal_id[0];
                    largest_proposal_id[1] = accepted_proposal_id[1];
                    accepted_value = reply[4];
                }
            }
        }
    }

    if (true == error) {
        return JANUS_RET_ERR;
    } else if (true == update) {
        return JANUS_RET_UPDATE;
    } else if (ok < _rc->get_cluster_size() / 2) {
        return JANUS_RET_REJECT; 
    }

    return JANUS_RET_OK;
}

int JanusLocker::_do_paxos_accept(string& proposal_value, unsigned int& updated_instance_id) {
    vector<string> argv;
    stringstream convert;

    argv.push_back(_lock->get_lock_name());

    convert << _instance_id;
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    convert << _proposal_id[0];
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    convert << _proposal_id[1];
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    argv.push_back(proposal_value);

    vector<vector<string> > replies;
    int ret = _rc->batch_exec_script("janus_builtin_paxos_accept.lua", 0, argv, replies);
    if (JANUS_RET_ERR == ret) {
        return JANUS_RET_ERR;
    }

    /*
     * parse paxos accept result:
     * 1)if any error, return JANUS_RET_ERR
     * 2)if any higher instance exists, return JANUS_RET_UPDATE, along with the updated instance id
     * 3)if got less than majority oks, return JANUS_RET_REJECT
     * 4)if got majority oks, proposal accepted, return JANUS_RET_OK
     */
    unsigned int ok = 0;
    bool update = false;
    bool error = false;
    unsigned int vec_vec_size = replies.size();
    vector<string> reply;
    unsigned int vec_size;
    for (unsigned int i = 0; i < vec_vec_size; i++) {
        reply = replies[i];
        vec_size = reply.size();
        if (1 > vec_size) {
            continue;
        }

        if (JANUS_LUA_ERRNO_ERROR == reply[0]) {
            error = true;
            break;
        } else if (JANUS_LUA_ERRNO_UPDATE == reply[0]) {
            if (3 == vec_size && false == reply[2].empty()) {
                stringstream updated_ins_id(reply[2]);
                updated_ins_id >> updated_instance_id; 
                update = true;
                break;
            } else {
                error = true;
                break;
            }
        } else if (JANUS_LUA_ERRNO_OK == reply[0]) {
            ok++;
        }
    }

    if (true == error) {
        return JANUS_RET_ERR;
    } else if (true == update) {
        return JANUS_RET_UPDATE;
    } else if (ok < _rc->get_cluster_size() / 2) {
        return JANUS_RET_REJECT; 
    }

    return JANUS_RET_OK;
}

int JanusLocker::_do_renew(unsigned int& updated_instance_id) {
    vector<string> argv;
    stringstream convert;

    argv.push_back(_lock->get_lock_name());

    convert << _instance_id;
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    argv.push_back(_own_name);

    vector<vector<string> > replies;
    int ret = _rc->batch_exec_script("janus_builtin_renew.lua", 0, argv, replies);
    if (JANUS_RET_ERR == ret) {
        return JANUS_RET_ERR;
    }

    /*
     * parse renew result:
     * 1)if any error, return JANUS_RET_ERR
     * 2)if any higher instance exists, return JANUS_RET_UPDATE, along with the updated instance id
     * 3)if got less than majority oks, return JANUS_RET_REJECT
     * 4)if got majority oks, renewal succeeds, return JANUS_RET_OK
     */
    unsigned int ok = 0;
    bool update = false;
    bool error = false;
    unsigned int vec_vec_size = replies.size();
    vector<string> reply;
    unsigned int vec_size;
    for (unsigned int i = 0; i < vec_vec_size; i++) {
        reply = replies[i];
        vec_size = reply.size();
        if (1 > vec_size) {
            continue;
        }

        if (JANUS_LUA_ERRNO_ERROR == reply[0]) {
            error = true;
            break;
        } else if (JANUS_LUA_ERRNO_UPDATE == reply[0]) {
            if (3 == vec_size && false == reply[2].empty()) {
                stringstream updated_ins_id(reply[2]);
                updated_ins_id >> updated_instance_id; 
                update = true;
                break;
            } else {
                error = true;
                break;
            }
        } else if (JANUS_LUA_ERRNO_OK == reply[0]) {
            ok++;
        }
    }

    if (true == error) {
        return JANUS_RET_ERR;
    } else if (true == update) {
        return JANUS_RET_UPDATE;
    } else if (ok < _rc->get_cluster_size() / 2) {
        return JANUS_RET_REJECT; 
    }

    return JANUS_RET_OK;
}

int JanusLocker::_do_check(unsigned int& updated_instance_id, unsigned int& updated_last_seq_num) {
    vector<string> argv;
    stringstream convert;

    argv.push_back(_lock->get_lock_name());

    convert << _instance_id;
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    convert << _last_seq_num;
    argv.push_back(convert.str());
    convert.str(string());
    convert.clear();

    vector<vector<string> > replies;
    int ret = _rc->batch_exec_script("janus_builtin_check.lua", 0, argv, replies);
    if (JANUS_RET_ERR == ret) {
        return JANUS_RET_ERR;
    }

    /*
     * parse check result:
     * 1)if any error, return JANUS_RET_ERR
     * 2)if any higher instance exists, return JANUS_RET_UPDATE, along with the updated instance id
     * 3)if got less than majority oks, return JANUS_RET_REJECT
     * 4)if got majority oks, record the smallest updated seq_num, return JANUS_RET_OK
     */
    unsigned int ok = 0;
    bool update = false;
    bool error = false;
    unsigned int tmp_seq_num, min_seq_num = UINT_MAX;
    unsigned int vec_vec_size = replies.size();
    vector<string> reply;
    unsigned int vec_size;
    for (unsigned int i = 0; i < vec_vec_size; i++) {
        reply = replies[i];
        vec_size = reply.size();
        if (1 > vec_size) {
            continue;
        }

        if (JANUS_LUA_ERRNO_ERROR == reply[0]) {
            error = true;
            break;
        } else if (JANUS_LUA_ERRNO_UPDATE == reply[0]) {
            if (3 == vec_size && false == reply[2].empty()) {
                stringstream updated_ins_id(reply[2]);
                updated_ins_id >> updated_instance_id; 
                update = true;
                break;
            } else {
                error = true;
                break;
            }
        } else if (JANUS_LUA_ERRNO_OK == reply[0] && 3 == vec_size && false == reply[2].empty()) {
            ok++;
            stringstream seq_num(reply[2]);
            seq_num >> tmp_seq_num;
            if (tmp_seq_num < min_seq_num) {
                min_seq_num = tmp_seq_num;
            } 
        }
    }

    if (true == error) {
        return JANUS_RET_ERR;
    } else if (true == update) {
        return JANUS_RET_UPDATE;
    } else if (ok < _rc->get_cluster_size() / 2) {
        return JANUS_RET_REJECT; 
    }

    updated_last_seq_num = min_seq_num;
    return JANUS_RET_OK;
}

string JanusLocker::_make_locker_name() {
    stringstream ss;
    ss << _now_us << "_" << _own_ip_addr << "_" << _own_pid;
    return ss.str();
} 

void JanusLocker::_make_proposal_id() {
    _proposal_id[0] = _now_us;
    _proposal_id[1] = _own_ip_addr;
    _proposal_id[1] = _proposal_id[1] << (sizeof(unsigned int) * 8);
    _proposal_id[1] |= _own_pid;
    return;
} 

unsigned long JanusLocker::_make_now_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return convert_timeval_to_us(&tv);
}



/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
