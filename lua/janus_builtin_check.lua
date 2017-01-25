--args: lock_name, instance_id, last_seq_num 
--ret: {errno, errmsg, [updated instance_id], [updated last_seq_num]}
--errno: 0:success 1:update 2:reject 3:unallowed -1:error

if (3 ~= table.getn(ARGV)) then
    return {-1, 'invalid param nums'};
end;

------input parameters
local lock_name = ARGV[1];
local instance_id = tonumber(ARGV[2]);
local last_seq_num = tonumber(ARGV[3]);

------local variables
local ret;
local allowed_to_participate_switch;
local outstanding_instance_id;
local arr_instance_info;
local seq_num;

------Redis keys
local key_allowed_to_participate_switch = 'allowed_to_participate_switch_{' .. lock_name .. '}';
local key_outstanding_instance_id = 'outstanding_instance_id_{' .. lock_name .. '}';
local key_locks_instances_hash = 'locks_instances_hash_{' .. lock_name .. '}_{' .. instance_id .. '}';
local field_seq_num = 'seq_num';

--check if allowed to answer prepare requests
local allowed_to_participate_switch = redis.call('GET', key_allowed_to_participate_switch);
if (false == allowed_to_participate_switch) then
    return {3, 'NOT allowed_to_participate_switch'};
end;

--check if outstanding_instance_id exists or higher or lower than input
outstanding_instance_id = redis.call('GET', key_outstanding_instance_id);
if (false == outstanding_instance_id) then
    return {2, 'reject due to non-existent outstanding_instance_id'};
elseif (tonumber(outstanding_instance_id) < instance_id) then
    return {2, 'reject due to lower outstanding_instance_id'};
elseif (tonumber(outstanding_instance_id) > instance_id) then
    return {1, 'larger instance_id exists, update', outstanding_instance_id};
end;

--check if info exists for this lock + instance combination, if not, reject 
arr_instance_info = redis.call('HKEYS', key_locks_instances_hash);
if (nil == next(arr_instance_info)) then
    return {2, 'reject due to non-existent locks_instances_hash'};
end;

--check if seq_num growed, if not, reject
seq_num = redis.call('HGET', key_locks_instances_hash, field_seq_num);
if (false == seq_num) then
    return {2, 'reject due to non-existent seq_num'};
elseif (tonumber(seq_num) == last_seq_num) or (tonumber(seq_num) < last_seq_num) then
    return {2, 'reject due to equal or smaller seq_num'};
end;

return {0, 'success', seq_num};
