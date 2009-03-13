--
-- RPC2's bandwidth and latency estimator
--

print "loading adaptive estimator"

-- These globals are set by RPC2
-- RPC2_RETRIES = 5
-- RPC2_TIMEOUT = time(15.0)

local RTT_SCALE = 8
local RTTVAR_SCALE = 4
local BW_SCALE = 16

local function estimate(host, bytes_sent, bytes_recv)
    local BW_send, BW_recv, rtt_lat, rtt_send, rtt_recv, rto

    BW_send, BW_recv = rtt_getbandwidth(host)
    rtt_lat = host.RTT / RTT_SCALE
    rtt_send = bytes_sent / BW_send
    rtt_recv = bytes_recv / BW_recv
    rto = rtt_lat + rtt_send + rtt_recv

    return rto, rtt_lat, rtt_send, rtt_recv
end

local function update_bw(bw_lo, bw_hi, rtt, bytes)
    local bw_cur, bw_est
    local BW_SCALE = BW_SCALE

    bw_cur = rtt / bytes
    bw_est = 1 / bw_lo
    bw_est = bw_est + (bw_cur - bw_est) / BW_SCALE
    bw_lo = 1 / bw_est

    bw_cur = bytes / rtt
    bw_hi = bw_hi + (bw_cur - bw_hi) / BW_SCALE

    return bw_lo, bw_hi
end

function rtt_init(host)
    host.RTT = time(0)
    host.RTTvar = time(0)
    host.BW_send_lo = 100000
    host.BW_send_hi = 100000
    host.BW_recv_lo = 100000
    host.BW_recv_hi = 100000
end

function rtt_update(host, elapsed, bytes_sent, bytes_recv)
    local rto, rtt_lat, rtt_send, rtt_recv, err

    if elapsed == 0 then return end

    -- Get current estimates
    rto, rtt_lat, rtt_send, rtt_recv = estimate(host, bytes_sent, bytes_recv)

    --[[
    print("uRTT", host.name, elapsed, bytes_sent, bytes_recv, rto, rtt_lat,
	  host.BW_send_lo, host.BW_send_hi, host.BW_recv_lo, host.BW_recv_hi)
    --]]

    -- Calculate error and desired correction
    if elapsed >= rto then
	err = (elapsed - rto) / 3
	rtt_send = rtt_send + err
	rtt_recv = rtt_recv + err
    else
	err = elapsed / 3
	rtt_send = err
	rtt_recv = err
	err = err - rtt_lat
    end

    -- Update estimates
    host.RTT = host.RTT + err
    err = err - host.RTTvar / RTTVAR_SCALE
    host.RTTvar = host.RTTvar + err

    host.BW_send_lo, host.BW_send_hi =
	update_bw(host.BW_send_lo, host.BW_send_hi, rtt_send, bytes_sent)

    host.BW_recv_lo, host.BW_recv_hi =
	update_bw(host.BW_recv_lo, host.BW_recv_hi, rtt_recv, bytes_recv)
end

function rtt_getbandwidth(host)
    local avg_send, avg_recv
    avg_send = (host.BW_send_lo + host.BW_send_hi) / 2
    avg_recv = (host.BW_recv_lo + host.BW_recv_hi) / 2
    return avg_send, avg_recv
end

function rtt_getrto(host, bytes_sent, bytes_recv)
    local rtt = (estimate(host, bytes_sent, bytes_recv))
    -- print("est", host.name, rtt, bytes_sent, bytes_recv)
    return rtt + host.RTTvar / 2
end

-- build array of precalculated timeout/retry values
-- initialized with keepalive
T = { [-1] = RPC2_TIMEOUT / 4, [0] = 0 }

-- calculate desired timeout for each successive retry
t = RPC2_TIMEOUT
for i = RPC2_RETRIES, 1, -1 do
    t = t / 2
    T[i] = t
end

function rtt_retryinterval(host, attempt, bytes_sent, bytes_recv)
    local rto, timeout, retry

    rto = (estimate(host, bytes_sent, bytes_recv))
    retry = T[attempt]

    if retry and retry < rto then return rto end

    return retry
end

