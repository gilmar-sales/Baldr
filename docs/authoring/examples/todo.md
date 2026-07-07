# Todo example

[`examples/Todo`](https://github.com/gilmar-sales/Baldr/tree/main/examples/Todo) is a small CRUD service for a `Todo` resource. It pulls together most of the building blocks a real Baldr app uses: a singleton repository registered through DI, a controller that groups its routes under `/api/todos`, bound path / body / query parameters via `baldr::FromParams` / `baldr::FromBody` / `baldr::FromQuery`, validation errors expressed as typed results, and an OpenAPI 3.0.3 spec plus Scalar UI.

## Source

[`examples/Todo/src/main.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/Todo/src/main.cpp):

```cpp title="examples/Todo/src/main.cpp" linenums="1"
#include <Baldr/Baldr.hpp>

#include "InMemoryTodoRepository.hpp"
#include "TodoController.hpp"

int main()
{
    auto builder = skr::ApplicationBuilder()
                       .WithExtension<baldr::BaldrExtension>()
                       .WithExtension<baldr::BaldrOpenApiExtension>();

    builder.GetServiceCollection()
        ->AddSingleton<ITodoRepository, InMemoryTodoRepository>();

    auto app = builder.Build<baldr::WebApplication>();

    baldr::MapScalarUi(*app);

    TodoController controller(
        app->GetRootServiceProvider()->GetService<ITodoRepository>());
    controller.Register(*app);

    app->Run();

    return 0;
}
```

[`examples/Todo/src/Todo.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/Todo/src/Todo.hpp):

```cpp title="Todo.hpp" linenums="1"
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Todo
{
    int64_t     id;
    std::string title;
    bool        done;
    std::string created_at;
};

struct CreateTodoDto
{
    std::string title;
    bool        done = false;
};

struct UpdateTodoDto
{
    std::optional<std::string> title;
    std::optional<bool>        done;
};

struct IdParam
{
    int64_t id;
};

struct PageQuery
{
    std::optional<int> pageSize;
    std::optional<int> page;

    struct Normalized
    {
        int pageSize;
        int page;
        int offset;
    };

    Normalized normalized() const noexcept
    {
        int ps = pageSize.value_or(500);
        if (ps < 1)
            ps = 1;
        if (ps > 500)
            ps = 500;
        int p = page.value_or(1);
        if (p < 1)
            p = 1;
        return Normalized { .pageSize = ps,
                            .page     = p,
                            .offset   = (p - 1) * ps };
    }
};

struct TodoPage
{
    std::vector<Todo> items;
    int               page;
    int               pageSize;
    int64_t           total;
};
```

[`examples/Todo/src/ITodoRepository.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/Todo/src/ITodoRepository.hpp):

```cpp title="ITodoRepository.hpp" linenums="1"
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Todo.hpp"

class ITodoRepository
{
  public:
    virtual ~ITodoRepository() = default;

    virtual std::vector<Todo>    List(int limit, int offset)       = 0;
    virtual long long            Count()                          = 0;
    virtual std::optional<Todo>  GetById(int64_t id)              = 0;
    virtual Todo                 Create(std::string title,
                                        bool        done)        = 0;
    virtual std::optional<Todo>  Update(int64_t     id,
                                        std::string title,
                                        bool        done)        = 0;
    virtual bool                 Delete(int64_t id)              = 0;
};
```

[`examples/Todo/src/InMemoryTodoRepository.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/Todo/src/InMemoryTodoRepository.hpp):

```cpp title="InMemoryTodoRepository.hpp" linenums="1"
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ITodoRepository.hpp"

class InMemoryTodoRepository final : public ITodoRepository
{
  public:
    InMemoryTodoRepository()          = default;
    ~InMemoryTodoRepository() override = default;

    InMemoryTodoRepository(const InMemoryTodoRepository&)            = delete;
    InMemoryTodoRepository& operator=(const InMemoryTodoRepository&) = delete;

    std::vector<Todo> List(int limit, int offset) override;

    long long Count() override;

    std::optional<Todo> GetById(int64_t id) override;

    Todo Create(std::string title, bool done) override;

    std::optional<Todo> Update(int64_t id, std::string title,
                               bool done) override;

    bool Delete(int64_t id) override;

  private:
    std::mutex                                       mMutex;
    std::unordered_map<int64_t, Todo>                mTodos;
    std::atomic<int64_t>                             mNextId { 1 };
};
```

[`examples/Todo/src/TodoController.hpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/Todo/src/TodoController.hpp):

```cpp title="TodoController.hpp" linenums="1"
#pragma once

#include <Skirnir/Skirnir.hpp>

#include <Baldr/Application/WebApplication.hpp>

#include "ITodoRepository.hpp"

class TodoController
{
  public:
    explicit TodoController(skr::Arc<ITodoRepository> repository);

    void Register(baldr::WebApplication& app);

  private:
    skr::Arc<ITodoRepository> mRepository;
};
```

[`examples/Todo/src/TodoController.cpp`](https://github.com/gilmar-sales/Baldr/blob/main/examples/Todo/src/TodoController.cpp):

```cpp title="TodoController.cpp" linenums="1"
#include "TodoController.hpp"

#include <Baldr/Http/FromBody.hpp>
#include <Baldr/Http/FromParams.hpp>
#include <Baldr/Http/FromQuery.hpp>
#include <Baldr/Http/Results/Result.hpp>
#include <Baldr/Http/Results/TypedResults.hpp>

#include <utility>
#include <variant>

TodoController::TodoController(skr::Arc<ITodoRepository> repository) :
    mRepository(std::move(repository))
{
}

struct ValidationError
{
    std::string field;
    std::string message;
};

void TodoController::Register(baldr::WebApplication& app)
{
    app.MapGroup("/api/todos", [this](auto& group) {
        group.MapGet("/")
            .WithSummary("List todos (paged)")
            .Handle([this](baldr::FromQuery<PageQuery> q)
                        -> std::variant<
                            baldr::JsonResult<ValidationError,
                                              baldr::StatusCode::BadRequest>,
                            baldr::JsonResult<TodoPage,
                                              baldr::StatusCode::OK>> {
                if (!q.isOk())
                    return baldr::Results::Json<ValidationError,
                                                baldr::StatusCode::BadRequest>(
                        ValidationError { "query", q.error->message });

                auto n     = q.value.normalized();
                auto items = mRepository->List(n.pageSize, n.offset);
                auto total = mRepository->Count();
                return baldr::Results::Json<TodoPage,
                                            baldr::StatusCode::OK>(
                    TodoPage { .items    = std::move(items),
                               .page     = n.page,
                               .pageSize = n.pageSize,
                               .total    = total });
            });

        group.MapGet("/:id")
            .WithSummary("Get a todo by id")
            .Handle([this](baldr::FromParams<IdParam> params)
                        -> std::variant<
                            baldr::JsonResult<Todo, baldr::StatusCode::OK>,
                            baldr::NotFoundResult> {
                auto found = mRepository->GetById(params.value.id);

                if (!found)
                    return baldr::Results::NotFound();

                return baldr::Results::Json<Todo, baldr::StatusCode::OK>(
                    *found);
            });

        group.MapPost("/")
            .WithSummary("Create a todo")
            .Handle([this](baldr::FromBody<CreateTodoDto> body)
                        -> std::variant<
                            baldr::JsonResult<Todo, baldr::StatusCode::Created>,
                            baldr::JsonResult<ValidationError,
                                              baldr::StatusCode::BadRequest>> {
                if (body.value.title.empty())
                    return baldr::Results::Json<ValidationError,
                                                baldr::StatusCode::BadRequest>(
                        ValidationError { "title", "title is required" });

                auto todo = mRepository->Create(std::move(body.value.title),
                                                body.value.done);

                return baldr::Results::Json<Todo, baldr::StatusCode::Created>(
                    std::move(todo));
            });

        group.MapPut("/:id")
            .WithSummary("Update a todo")
            .Handle([this](baldr::FromParams<IdParam> params,
                           baldr::FromBody<UpdateTodoDto>
                               body)
                        -> std::variant<
                            baldr::JsonResult<Todo, baldr::StatusCode::OK>,
                            baldr::JsonResult<ValidationError,
                                              baldr::StatusCode::BadRequest>,
                            baldr::NotFoundResult> {
                if (body.value.title.empty())
                    return baldr::JsonResult<ValidationError,
                                             baldr::StatusCode::BadRequest>(
                        ValidationError { "title", "title is required" });

                auto updated = mRepository->Update(
                    params.value.id, std::move(body.value.title),
                    body.value.done);

                if (!updated)
                    return baldr::Results::NotFound();

                return baldr::Results::Json<Todo, baldr::StatusCode::OK>(
                    *updated);
            });

        group.MapDelete("/:id")
            .WithSummary("Delete a todo")
            .Handle([this](baldr::FromParams<IdParam> params)
                        -> std::variant<baldr::NotFoundResult,
                                        baldr::NoContentResult> {
                if (!mRepository->Delete(params.value.id))
                    return baldr::Results::NotFound();

                return baldr::Results::NoContent();
            });
    });
}
```

## What it shows

- Registering an interface-to-implementation binding with `AddSingleton<ITodoRepository, InMemoryTodoRepository>()` so the rest of the app can depend on the abstraction.
- Resolving a singleton from the root service provider in `main` and passing it into a controller manually (instead of taking it as a route-handler parameter).
- Grouping routes under a common prefix with `app.MapGroup("/api/todos", [](auto& group) { ... })`.
- Binding typed path, body, and query parameters with `baldr::FromParams<T>`, `baldr::FromBody<T>`, and `baldr::FromQuery<T>`, where the wrapped type aggregates the route params, JSON body, or query string.
- Modelling multiple success and error outcomes from a single handler with `std::variant` of `JsonResult<T, Status>` and result markers such as `NotFoundResult` / `NoContentResult`.
- Returning a `ValidationError` DTO under `400 Bad Request` from `POST`, `PUT`, and paged `GET` when the input fails to bind — distinct from a bare `BadRequestResult`.
- Paginating the list endpoint with `FromQuery<PageQuery>` (`pageSize` clamped to `[1, 500]`, `page` clamped to `>= 1`) and returning a `TodoPage` envelope carrying `items`, `page`, `pageSize`, and `total` so clients can detect when more data is available.
- Wiring the OpenAPI extension and the Scalar UI so the controller's routes appear in the generated spec at `/openapi.json` (query parameters included automatically) and the UI at `/scalar`.

## Try it

```bash
cmake -S . -B build
cmake --build build
./build/Todo
```

In another terminal:

```bash
# List (empty at first; defaults: page=1, pageSize=500)
curl http://localhost:8080/api/todos/

# Create
curl -i -X POST http://localhost:8080/api/todos/ \
     -H 'Content-Type: application/json' \
     -d '{"title":"Write docs","done":false}'

# Get by id
curl http://localhost:8080/api/todos/1

# Update
curl -X PUT http://localhost:8080/api/todos/1 \
     -H 'Content-Type: application/json' \
     -d '{"title":"Write docs","done":true}'

# Delete
curl -i -X DELETE http://localhost:8080/api/todos/1

# Pagination — page 1 of 10, then page 2
curl 'http://localhost:8080/api/todos/?page=1&pageSize=10'
curl 'http://localhost:8080/api/todos/?page=2&pageSize=10'

# Validation error (missing field on POST)
curl -i -X POST http://localhost:8080/api/todos/ \
     -H 'Content-Type: application/json' \
     -d '{"title":"","done":false}'

# Validation error (malformed query value)
curl -i 'http://localhost:8080/api/todos/?pageSize=abc'

# Browse the spec and UI
curl http://localhost:8080/openapi.json | jq '.paths, .components.schemas'
# Scalar UI is served at /scalar
```

## Next steps

- See [Dependency injection](../../usage/dependency-injection.md) for `AddSingleton` / `AddTransient` semantics.
- See [Results](../../usage/results.md) for the typed result family and variant returns.
- See [OpenAPI extension](../../extensions/openapi.md) for the spec generation that the `WithSummary` calls feed.
- See [Route options](../../usage/route-options.md) for the full list of metadata setters.
- Browse [all examples](../examples.md).
