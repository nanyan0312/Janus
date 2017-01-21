--args: lock_name, instance_id, own_value 
--ret: {errno, errmsg, [updated instance_id]}
--errno: 0:success 1:update 2:reject -1:error

if (3 ~= table.getn(ARGV)) then
    return {-1, "invalid param nums"};
end;

------input parameters
local lock_name = ARGV[1];
local instance_id = tonumber(ARGV[2]);
local own_value = ARGV[3];

------local variables
local ret;
local outstanding_instance_id;
local arr_instance_info;
local accepted_value;

------Redis keys
local key_outstanding_instance_id = "outstanding_instance_id_{" .. lock_name .. "}";
local key_locks_instances_hash = "locks_instances_hash_{" .. lock_name .. "}_{" .. instance_id .. "}";
local field_accepted_value = "accepted_value";
local field_seq_num = "seq_num";

--check if outstanding_instance_id exists or higher or lower than input
outstanding_instance_id = redis.call("GET", key_outstanding_instance_id);
if (false == outstanding_instance_id) then
    return {2, "reject due to non-existent outstanding_instance_id"};
elseif (tonumber(outstanding_instance_id) < instance_id) then
    return {2, "reject due to lower outstanding_instance_id"};
elseif (tonumber(outstanding_instance_id) > instance_id) then
    return {1, "larger instance_id exists, update", outstanding_instance_id};
end;

--check if info exists for this lock + instance combination, if not, reject 
arr_instance_info = redis.call("HKEYS", key_locks_instances_hash);
if (nil == next(arr_instance_info)) then
    return {2, "reject due to non-existent locks_instances_hash"};
end;

--check if lock owner for this instance is self, if not, reject
accepted_value = redis.call("HGET", key_locks_instances_hash, field_accepted_value);
if (false == accepted_value) then
    return {2, "reject due to non-existent accepted_value"};
elseif (own_value ~= accepted_value) then
    return {2, "reject due to different accepted_value"};
end;

--Im indeed the lock owner for this instance, renew my lock ownership by incrementing associated seq_num
redis.call("HINCRBY", key_locks_instances_hash, field_seq_num, 1);

return {0, "success"};
