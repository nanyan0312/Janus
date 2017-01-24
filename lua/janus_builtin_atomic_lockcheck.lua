local janus_builtin_avoid_repetition_argc = table.getn(ARGV); 
if (janus_builtin_avoid_repetition_argc < 3) then 
    return {-1, "janus_builtin_atomic_lockcheck_failed, too few args"}; 
end 
local janus_builtin_avoid_repetition_switch = ARGV[janus_builtin_avoid_repetition_argc - 2]; 
local janus_builtin_avoid_repetition_lock_name = ARGV[janus_builtin_avoid_repetition_argc - 1]; 
local janus_builtin_avoid_repetition_instance_id = tonumber(ARGV[janus_builtin_avoid_repetition_argc]); 
if ("janus_builtin_atomic_lockcheck_yes" == janus_builtin_avoid_repetition_switch) then
    local janus_builtin_avoid_repetition_key_outstanding_instance_id = "outstanding_instance_id_{" .. janus_builtin_avoid_repetition_lock_name .. "}"; 
    local janus_builtin_avoid_repetition_outstanding_instance_id = redis.call("GET", janus_builtin_avoid_repetition_key_outstanding_instance_id); 
    if (false == janus_builtin_avoid_repetition_outstanding_instance_id) then 
        return {-1, "janus_builtin_atomic_lockcheck_failed, outstanding_instance_id no exists"}; 
    end 
    if (tonumber(janus_builtin_avoid_repetition_outstanding_instance_id) > janus_builtin_avoid_repetition_instance_id) then 
        return {-1, "janus_builtin_atomic_lockcheck_failed, outstanding_instance_id higher"}; 
    end 
    redis.call("SET", janus_builtin_avoid_repetition_key_outstanding_instance_id, janus_builtin_avoid_repetition_instance_id); 
end
ARGV[janus_builtin_avoid_repetition_argc - 2] = nil; 
ARGV[janus_builtin_avoid_repetition_argc - 1] = nil; 
ARGV[janus_builtin_avoid_repetition_argc] = nil;
