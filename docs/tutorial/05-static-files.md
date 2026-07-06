# 5. Static files

`MapStaticFiles` serves a directory tree under a URL prefix. Path traversal is rejected, ETag and `Last-Modified` headers are set automatically, and `304 Not Modified` is honoured.

## Mount a directory

```cpp title="src/main.cpp" linenums="1"
app->MapStaticFiles("/static", "./public");
```

After this call:

```text title="layout"
public/
├── index.html
├── styles.css
└── js/
    └── app.js
```

is served under `/static/*`:

```bash title="terminal"
curl http://localhost:8080/static/index.html
curl http://localhost:8080/static/js/app.js
```

Directories fall back to `index.html` if it exists.

## Caching and conditional GET

`MapStaticFiles` automatically:

- Sets `ETag` and `Last-Modified` on every response.
- Responds with `304 Not Modified` when the client's `If-None-Match` or `If-Modified-Since` matches.
- Streams the body via chunked transfer encoding for large files.

You don't need to configure any of this — it is on by default.

## Path safety

The handler rejects:

- Paths containing `\0` or `\\`.
- Path segments equal to `.` or `..`.
- Requests that resolve outside the configured `rootPath` after canonicalisation.

A path that fails any of these checks returns `400 Bad Request` or `403 Forbidden` depending on the failure mode.

## Next

Continue with [6. Testing](06-testing.md).