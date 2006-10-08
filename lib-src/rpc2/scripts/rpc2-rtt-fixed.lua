--
-- Fixed bandwidth 'estimator' where we know our network environment
--

print "loading fixed estimator"

--
-- Latencies are rough estimates based on the average reported by 'ping -c10'
-- bandwidth as kbits/sec
--
--[[ ppp modem
latency, bandwidth_tx, bandwidth_rx = 200, 56, 56
--]]
-- [[ cable modem / adsl
latency, bandwidth_tx, bandwidth_rx = 14, 364, 4 * 1024
--]]
--[[ 100mbit ethernet
latency, bandwidth_tx, bandwidth_rx = 0.6, 100 * 1024, 100 * 1024
--]]
--[[ gigabit ethernet
latency, bandwidth_tx, bandwidth_rx = 0.25, 1024 * 1024, 1024 * 1024
--]]

-- convert values
latency = latency * 1000	       -- ms -> us
bandwidth_tx = bandwidth_tx * 1024 / 8 -- bits/sec -> bytes/sec
bandwidth_rx = bandwidth_rx * 1024 / 8 -- bits/sec -> bytes/sec

-- assume any delays are caused by concurrent data transfers
local concurrency = 1
local function estimate(host, bytes_sent, bytes_recv)
    local rto = bytes_sent / bandwidth_tx + bytes_recv / bandwidth_rx
    return 1e6 * rto * concurrency
end

function rtt_update(host, elapsed, bytes_sent, bytes_recv)
    local rto = estimate(host, bytes_sent, bytes_recv)
    -- print("uRTT", host.name, bytes_sent, bytes_recv, elapsed, latency + rto)

    -- try to estimate how many concurrent transfers there are
    if concurrency > 1 and elapsed < latency + rto then
	concurrency = concurrency - 1
    else
	while elapsed > latency + rto do
	    concurrency = concurrency + 1
	    rto = rto * 2
	end
    end
    -- print("concurrency", concurrency)
end

function rtt_getbandwidth(host)
    return bandwidth_tx / concurrency, bandwidth_rx / concurrency
end

function rtt_getrto(host, bytes_sent, bytes_recv)
    local rtt = estimate(host, bytes_sent, bytes_recv)
    -- print("est", host.name, bytes_sent, bytes_recv, latency + rtt)
    return latency + rtt
end

