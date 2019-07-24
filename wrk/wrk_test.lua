dl_time = 1
done = function(summary, latency, requests)
    io.write("--BEGIN--\n")

	io.write(string.format("%f, %f, %f, %f\n", latency.min, latency.max, latency.mean, latency.stdev))
	for _, p in pairs({50, 90, 99, 99.999 }) do
		n = latency:percentile(p)
		io.write(string.format("%f\n", n))
	end

	io.write(string.format("%f, %f, %f, %f\n", requests.min, requests.max, requests.mean, requests.stdev))
	for _, p in pairs({50, 90, 99, 99.999 }) do
		n = requests:percentile(p)
		io.write(string.format("%f\n", n))
	end

	io.write(string.format("%f, %f, %f, %f\n", summary.duration, summary.requests, summary.bytes, summary.requests/(summary.duration/1000000)))
	io.write(string.format("%f, %f, %f, %f, %f\n", summary.errors.connect, summary.errors.read, summary.errors.write, summary.errors.status, summary.errors.timeout))
	
	io.write("--END--\n")
end
--[[
function delay()
   return dl_time
end
--]]
