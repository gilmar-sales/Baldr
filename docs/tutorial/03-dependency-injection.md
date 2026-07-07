# 3. Dependency injection

Baldr uses [Skirnir](https://github.com/gilmar-sales/Skirnir) for dependency injection. Services registered with the application builder are available to handlers via their parameter list — no manual lookup.

## Register a service

```cpp title="src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

class Clock
{
  public:
    virtual ~Clock() = default;
    virtual std::string now() const = 0;
};

class SystemClock final : public Clock
{
  public:
    std::string now() const override
    {
        return "2026-01-01T00:00:00Z";
    }
};

int main()
{
    auto builder =
        skr::ApplicationBuilder().WithExtension<baldr::BaldrExtension>();

    builder.GetServiceCollection()->AddSingleton<Clock>(
        skr::MakeArc<SystemClock>());

    auto app = builder.Build<baldr::WebApplication>();

    app->MapGet("/now", [](skr::Arc<Clock> clock) {
        return std::string(clock->now());
    });

    app->Run();
    return 0;
}
```

The handler takes `skr::Arc<Clock>` as a parameter and Skirnir resolves the registered `SystemClock` instance per request. No globals, no manual `new`.

## Service lifetimes

| Method | Behaviour |
|---|---|
| `AddSingleton<T>(...)` | One instance shared by every request. |
| `AddScoped<T>(...)` | One instance per request scope. |
| `AddTransient<T>(...)` | New instance every resolution. |

For most applications `AddSingleton` is the right choice — handlers are stateless and benefit from sharing immutable collaborators.

## Multiple implementations

Register one type and resolve another:

```cpp title="src/main.cpp"
class IUserRepository { /* ... */ };
class InMemoryUserRepository final : public IUserRepository { /* ... */ };

// Register the interface; resolve by interface in handlers.
builder.GetServiceCollection()->AddSingleton<IUserRepository>(
    skr::MakeArc<InMemoryUserRepository>());
```

Swap `InMemoryUserRepository` for `SqlUserRepository` in one place without touching handlers.

## Next

Continue with [4. Middleware](04-middleware.md).