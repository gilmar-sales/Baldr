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

done = function(summary)
    io.write(string.format(
        "latency avg=%.2fms p99=%.2fms req/s=%.1f errors=connect:%d read:%d write:%d timeout:%d\n",
        summary.latency.mean / 1000,
        summary.latency["99%"] / 1000,
        summary.requests / summary.duration * 1e9,
        summary.errors.connect,
        summary.errors.read,
        summary.errors.write,
        summary.errors.timeout))
end
