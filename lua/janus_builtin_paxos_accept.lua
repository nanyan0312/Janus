--args: lock_name, instance_id, proposal_id_fir, proposal_id_sec, proposal_value
--ret: {errno, errmsg, [updated instance_id]}
--errno: 0:success 1:update 2:reject 3:unallowed -1:error

if (5 ~= table.getn(ARGV)) then
    return {-1, 'invalid param nums'};
end;

------input parameters
local lock_name = ARGV[1];
local instance_id = tonumber(ARGV[2]);
local proposal_id_fir = tonumber(ARGV[3]);
local proposal_id_sec = tonumber(ARGV[4]);
local proposal_value = ARGV[5];

------local variables
local ret;
local allowed_to_participate_switch;
local outstanding_instance_id;
local arr_instance_info;
local proposal_id_highest_seen_fir;
local proposal_id_highest_seen_sec;

------Redis keys
local key_allowed_to_participate_switch = 'allowed_to_participate_switch_{' .. lock_name .. '}';
local key_outstanding_instance_id = 'outstanding_instance_id_{' .. lock_name .. '}';
local key_locks_instances_hash = 'locks_instances_hash_{' .. lock_name .. '}_{' .. instance_id .. '}';
local field_proposal_id_highest_seen_fir = 'proposal_id_highest_seen_fir';
local field_proposal_id_highest_seen_sec = 'proposal_id_highest_seen_sec';
local field_proposal_id_highest_accepted_fir = 'proposal_id_highest_accepted_fir';
local field_proposal_id_highest_accepted_sec = 'proposal_id_highest_accepted_sec';
local field_accepted_value = 'accepted_value';

--check if allowed to answer prepare requests, if not, set it
local allowed_to_participate_switch = redis.call('GET', key_allowed_to_participate_switch);
if (false == allowed_to_participate_switch) then
    return {3, 'NOT allowed_to_participate_switch'};
end;

--check if outstanding_instance_id exists or larger than input
outstanding_instance_id = redis.call('GET', key_outstanding_instance_id);
if (false == outstanding_instance_id) then
    return {2, 'reject due to non-existent outstanding_instance_id'};
elseif (tonumber(outstanding_instance_id) < instance_id) then
    return {2, 'reject due to lower outstanding_instance_id'};
elseif (tonumber(outstanding_instance_id) > instance_id) then
    return {1, 'larger instance_id exists, update', outstanding_instance_id};
end;

--check if info exists for this lock + instance combination, if not, set it
arr_instance_info = redis.call('HKEYS', key_locks_instances_hash);
if (nil == next(arr_instance_info)) then
    return {2, 'reject due to non-existent locks_instances_hash'};
end;

--check if higher proposal_id has been seen before, if so, reject
proposal_id_highest_seen_fir = redis.call('HGET', key_locks_instances_hash, field_proposal_id_highest_seen_fir);
if (false == proposal_id_highest_seen_fir) then
    return {-1, 'proposal_id_highest_seen_fir not exists'};
elseif (tonumber(proposal_id_highest_seen_fir) > proposal_id_fir) then
    return {2, 'reject due to higher proposal_id_fir'};
elseif (tonumber(proposal_id_highest_seen_fir) == proposal_id_fir) then
    proposal_id_highest_seen_sec = redis.call('HGET', key_locks_instances_hash, field_proposal_id_highest_seen_sec);
    if (false == proposal_id_highest_seen_sec) then
        return {-1, 'proposal_id_highest_seen_sec not exists'};
    elseif (tonumber(proposal_id_highest_seen_sec) > proposal_id_sec) then
        return {2, 'reject due to higher proposal_id_sec'};
    end;
end;

--proposal accepted
--set current proposal_id as the highest ever seen and accepted
--set proposal value as accepted_value
ret = redis.call('HSET', key_locks_instances_hash, field_proposal_id_highest_seen_fir, proposal_id_fir);
ret = redis.call('HSET', key_locks_instances_hash, field_proposal_id_highest_seen_sec, proposal_id_sec);
ret = redis.call('HSET', key_locks_instances_hash, field_proposal_id_highest_accepted_fir, proposal_id_fir);
ret = redis.call('HSET', key_locks_instances_hash, field_proposal_id_highest_accepted_sec, proposal_id_sec);
ret = redis.call('HSET', key_locks_instances_hash, field_accepted_value, proposal_value);

return {0, 'success'};
