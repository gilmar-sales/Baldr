wrk.method = "GET"
wrk.headers["Accept"] = "application/json"

response = function(status, headers, body)
    if status ~= 200 then
        io.write(string.format("non-200 status: %d\n", status))
    end
    if not body or #body == 0 then
        io.write("empty body\n")
    end
end

done = function(summary, latency, requests)
    io.write(string.format(
        "latency avg=%.2fms p99=%.2fms req/s=%.2f errors=connect:%d read:%d write:%d status:%d timeout:%d\n",
        latency.mean / 1000,
        latency:percentile(99) / 1000,
        summary.requests / (summary.duration / 1e6),
        summary.errors.connect,
        summary.errors.read,
        summary.errors.write,
        summary.errors.status,
        summary.errors.timeout))
end
