# Tutorial: Hello World

This tutorial walks you through your first Baldr application in roughly 15 minutes. Each page is a single concept and builds on the previous one, so work through them in order.

| Page | Concept |
|---|---|
| [1. Hello world](01-hello-world.md) | Build, run, request the endpoint |
| [2. Routing and results](02-routing-and-results.md) | `MapGet`/`MapPost`, JSON vs text responses |
| [3. Dependency injection](03-dependency-injection.md) | Register a service, resolve it in a handler |
| [4. Middleware](04-middleware.md) | Cross-cutting concerns (logging, request id) |
| [5. Static files](05-static-files.md) | Serve a directory tree |
| [6. Testing](06-testing.md) | Run and extend the GoogleTest suite |

## What you need

- A C++ compiler with C++26 support — GCC 16+ is what the CI uses.
- CMake 3.28 or newer.
- Git (CMake's `FetchContent` clones Baldr and its dependencies).

If you haven't installed these yet, follow the [Get started](../get-started.md) prerequisites section before continuing.

## Where the code lives

Every page assumes a project laid out as:

```text title="layout"
my_app/
├── CMakeLists.txt
└── src/
    └── main.cpp
```

with `CMakeLists.txt` declaring Baldr via `FetchContent` exactly as in [Get started](../get-started.md#installation-with-cmake). The tutorial pages only ever edit `src/main.cpp`.

## Next

Continue with [1. Hello world](01-hello-world.md).