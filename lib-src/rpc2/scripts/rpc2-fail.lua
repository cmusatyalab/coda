--
-- Artificially introduce packet delays or failures to simulate a slow and/or
-- unreliable network.
--

print "enabling packet delay/failure functionality"

local packet_loss = 0.1		-- % chance of packet loss
local bandwidth = 64000		-- desired bandwidth (bits/s)
local latency_min = 0.10	-- network latency
local latency_max = 0.15

bandwidth = bandwidth / 8
lat_range = latency_max - latency_min

function slow(queue)
    local drained = time()
    return function (addr, size, color)
	-- packet loss
	if math.random() < packet_loss then return nil end

	-- bandwidth delay
	local now = time()
	local delay = size / bandwidth
	if drained < now then drained = now end
	drained = drained + delay

	-- network delay
	local latency = latency_min
	if lat_range then
	    latency = latency + math.random() * lat_range
	end

	delay = drained - now + latency
	print("delaying", queue, delay)
	return delay
    end
end

fail_delay_tx = slow("tx")
fail_delay_rx = slow("rx")

