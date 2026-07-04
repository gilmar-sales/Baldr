# Extensions

In Baldr, an **extension** is a piece of functionality that plugs into the application through Skirnir's DI container. There are two flavours:

- **Middleware** — cross-cutting components that intercept every request. They live under the **Middleware** section — see [Middleware overview](../middleware/overview.md).
- **Service extensions** — components that contribute DI-registered services and route handlers. They implement `skr::IExtension` (see [`src/Baldr/BaldrExtension.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/BaldrExtension.hpp) for the canonical example).

This section documents the service extensions that ship with Baldr.

!!! info "Baldr extensions vs. Zensical extensions"
    "Extensions" is also the name of a category in this documentation site (powered by [Zensical](https://zensical.org)). Those Zensical extensions are unrelated to Baldr extensions — they add features to the documentation site itself, such as search and social cards.

## Shipped extensions

| Extension | Purpose | Page |
| --- | --- | --- |
| `BaldrExtension` | Wires up `Router`, `MiddlewareProvider`, and `HttpServer`. Always required. | Implicit — added by every example |
| `BaldrOpenApiExtension` | Renders an OpenAPI 3.0.3 document from the router and serves it as JSON | [OpenAPI](openapi.md) |

## Writing your own

For service extensions that need their own DI services and/or routes, implement `skr::IExtension` (see [`src/Baldr/BaldrExtension.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/BaldrExtension.hpp) and [`src/Baldr/OpenApi/BaldrOpenApiExtension.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/src/Baldr/OpenApi/BaldrOpenApiExtension.hpp) for reference). Cross-cutting request-level concerns belong in middleware instead — see [Middleware overview](../middleware/overview.md#writing-your-own).