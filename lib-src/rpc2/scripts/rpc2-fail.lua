--
-- Artificially introduce packet delays or failures to simulate a slow and/or
-- unreliable network.
--

print "enabling packet delay/failure functionality"

latency = 100000	-- us
bandwidth = 56000 / 8	-- B/s

function dialup_modem(addr, size, color)
    local delay = 1e6 * size / bandwidth
    return delay
end

loss = 0.50 -- 50% chance of packet loss

function lossy_network(addr, size, color)
    if math.random() < loss then
	print("drop", addr)
	return nil
    else
	print("pass", addr)
	return 0
    end
end

fail_delay_tx = dialup_modem
fail_delay_rx = lossy_network

