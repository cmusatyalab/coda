--
-- Van Jacobson SIGCOMM'88 latency estimator
--
-- designed for TCP with window scaling, so not really appropriate for RPC2
-- because we don't guarantee that sizeof(packet N+1) <= sizeof(packet N).
--

print "loading Van Jacobson estimator"

local RTT_SCALE = 8
local RTTVAR_SCALE = 4

function rtt_init(host)
    host.RTT = 0
    host.RTTVAR = 0
end

function rtt_update(host, elapsed, bytes_sent, bytes_recv)
    local delta

    --[[
    local rto = estimate(host, bytes_sent, bytes_recv)
    print("uRTT", host.name, bytes_sent, bytes_recv, elapsed, rto)
    --]]

    -- Update RTT estimate
    delta = elapsed - host.RTT / RTT_SCALE
    host.RTT = host.RTT + delta

    -- Update RTT variance estimate
    if delta < 0 then delta = -delta end
    delta = delta - host.RTTVAR / RTTVAR_SCALE
    host.RTTVAR = host.RTTVAR + delta
end

function rtt_getrto(host, bytes_sent, bytes_recv)
    return host.RTT / RTT_SCALE + host.RTTVAR
end

--[[ this estimator doesn't track bandwidth
-- if we don't define a function (or it returns a bad value) rpc2 will fall
-- back on it's own estimator to obtain that value
function rtt_getbandwidth(host) return avg_send, avg_recv end
--]]

