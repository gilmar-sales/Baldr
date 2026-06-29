# Extensions

In Baldr, an **extension** is a C++ component that plugs into the application — most commonly, a middleware that adds a cross-cutting concern to every request.

This page lists the extensions shipped with Baldr and links to their dedicated documentation pages.

!!! info "Baldr extensions vs. Zensical extensions"
    "Extensions" is also the name of a category in this documentation site (powered by [Zensical](https://zensical.org)). Those Zensical extensions are unrelated to Baldr extensions — they add features to the documentation site itself, such as search and social cards.

## Shipped extensions

| Extension | Purpose | Page |
| --- | --- | --- |
| `LoggingMiddleware` | Logs every request and response with timing | [Logging](logging.md) |
| `RateLimitMiddleware` | Rejects clients that exceed a configured rate | [Rate limit](rate-limit.md) |

Both live under the [`src/Baldr/`](https://github.com/gilmar-sales/Baldr/tree/main/src/Baldr) directory and are header-only — including the header registers the middleware with the framework.

## Writing your own extension

Any class that implements [`IMiddleware`](../usage/middleware.md#the-imiddleware-interface) can be registered as an extension:

1. Implement `IMiddleware`.
2. Register the implementation with the service collection.
3. Add the middleware to the pipeline with `app->Use<T>()`.

See the [Middleware usage page](../usage/middleware.md#writing-your-own) for a complete walkthrough.