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
            .Handle(
                [this](baldr::FromQuery<PageQuery> q)
                    -> std::variant<
                        baldr::JsonResult<ValidationError,
                                          baldr::StatusCode::BadRequest>,
                        baldr::JsonResult<TodoPage, baldr::StatusCode::OK>> {
                    if (!q.isOk())
                        return baldr::Results::Json<
                            ValidationError, baldr::StatusCode::BadRequest>(
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
                if (body.value.title && body.value.title->empty())
                    return baldr::JsonResult<ValidationError,
                                             baldr::StatusCode::BadRequest>(
                        ValidationError { "title", "title must not be empty" });

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