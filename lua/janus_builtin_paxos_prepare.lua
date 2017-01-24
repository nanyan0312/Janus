--args: lock_name, instance_id, proposal_id_fir, proposal_id_sec
--ret: {errno, errmsg, [updated instance_id], [proposal_id_highest_accepted_fir], [proposal_id_highest_accepted_sec], [accepted_value]}
--errno: 0:success 1:update 2:reject -1:error

if (4 ~= table.getn(ARGV)) then
    return {-1, "invalid param nums"};
end;

------input parameters
local lock_name = ARGV[1];
local instance_id = tonumber(ARGV[2]);
local proposal_id_fir = tonumber(ARGV[3]);
local proposal_id_sec = tonumber(ARGV[4]);

------local variables
local ret;
local outstanding_instance_id;
local arr_instance_info;
local proposal_id_highest_seen_fir;
local proposal_id_highest_seen_sec;
local proposal_id_highest_accepted_fir;
local proposal_id_highest_accepted_sec;
local accepted_value;

------Redis keys
local key_outstanding_instance_id = "outstanding_instance_id_{" .. lock_name .. "}";
local key_locks_instances_hash = "locks_instances_hash_{" .. lock_name .. "}_{" .. instance_id .. "}";
local field_proposal_id_highest_seen_fir = "proposal_id_highest_seen_fir";
local field_proposal_id_highest_seen_sec = "proposal_id_highest_seen_sec";
local field_proposal_id_highest_accepted_fir = "proposal_id_highest_accepted_fir";
local field_proposal_id_highest_accepted_sec = "proposal_id_highest_accepted_sec";
local field_accepted_value = "accepted_value";

--check if outstanding_instance_id exists or larger than input
outstanding_instance_id = redis.call("GET", key_outstanding_instance_id);
if (false == outstanding_instance_id) or (tonumber(outstanding_instance_id) < instance_id) then
    ret = redis.call("SET", key_outstanding_instance_id, instance_id);
elseif (tonumber(outstanding_instance_id) > instance_id) then
    return {1, "larger instance_id exists, update", outstanding_instance_id};
end;

--check if info exists for this lock + instance combination, if not, set it
arr_instance_info = redis.call("HKEYS", key_locks_instances_hash);
if (nil == next(arr_instance_info)) then
    ret = redis.call("HSET", key_locks_instances_hash, field_proposal_id_highest_seen_fir, proposal_id_fir);
    ret = redis.call("HSET", key_locks_instances_hash, field_proposal_id_highest_seen_sec, proposal_id_sec);
end;

--check if higher proposal_id has been seen before, if so, reject
proposal_id_highest_seen_fir = redis.call("HGET", key_locks_instances_hash, field_proposal_id_highest_seen_fir);
if (false == proposal_id_highest_seen_fir) then
    return {-1, "proposal_id_highest_seen_fir not exists"};
elseif (tonumber(proposal_id_highest_seen_fir) > proposal_id_fir) then
    return {2, "reject due to higher proposal_id_fir"};
elseif (tonumber(proposal_id_highest_seen_fir) == proposal_id_fir) then
    proposal_id_highest_seen_sec = redis.call("HGET", key_locks_instances_hash, field_proposal_id_highest_seen_sec);
    if (false == proposal_id_highest_seen_sec) then
        return {-1, "proposal_id_highest_seen_sec not exists"};
    elseif (tonumber(proposal_id_highest_seen_sec) > proposal_id_sec) then
        return {2, "reject due to higher proposal_id_sec"};
    end;
end;

--set current proposal_id as the highest ever seen
ret = redis.call("HSET", key_locks_instances_hash, field_proposal_id_highest_seen_fir, proposal_id_fir);
ret = redis.call("HSET", key_locks_instances_hash, field_proposal_id_highest_seen_sec, proposal_id_sec);

--check if any proposal already accepted for this instance
proposal_id_highest_accepted_fir = redis.call("HGET", key_locks_instances_hash, field_proposal_id_highest_accepted_fir);
proposal_id_highest_accepted_sec = redis.call("HGET", key_locks_instances_hash, field_proposal_id_highest_accepted_sec);
accepted_value = redis.call("HGET", key_locks_instances_hash, field_accepted_value);

if (false == proposal_id_highest_accepted_fir) and (false == proposal_id_highest_accepted_sec) and (false == accepted_value) then 
    return {0, "success without accepted proposal", "", "", ""};
elseif (false == proposal_id_highest_accepted_fir) or (false == proposal_id_highest_accepted_sec) or (false == accepted_value) then
    return {-1 ,"missing info about accepted proposal"};
end;

return {0, "success with accepted proposal", tonumber(proposal_id_highest_accepted_fir), tonumber(proposal_id_highest_accepted_sec), accepted_value};
