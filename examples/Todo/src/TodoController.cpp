#include "TodoController.hpp"

#include <Baldr/Http/FromBody.hpp>
#include <Baldr/Http/FromParams.hpp>
#include <Baldr/Http/Results/Result.hpp>

#include <simdjson/simdjson.h>

#include <utility>
#include <variant>

TodoController::TodoController(skr::Arc<ITodoRepository> repository) :
    mRepository(std::move(repository))
{
}

void TodoController::Register(baldr::WebApplication& app)
{
    app.MapGroup("/api/todos", [this](auto& group) {
        group.MapGet("/").Handle([this]() { return mRepository->List(); });

        group.MapGet("/:id").Handle(
            [this](baldr::FromParams<IdParam> params)
                -> std::variant<baldr::JsonResult, baldr::StatusResult> {
                auto found = mRepository->GetById(params.value.id);

                if (!found)
                    return baldr::Results::Status(baldr::StatusCode::NotFound);

                return baldr::Results::Json(*found);
            });

        group.MapPost("/").Handle(
            [this](baldr::FromBody<CreateTodoDto> body) -> baldr::JsonResult {
                if (body.value.title.empty())
                    return baldr::JsonResult(R"({"error":"title is required"})",
                                             baldr::StatusCode::BadRequest);

                auto todo = mRepository->Create(std::move(body.value.title),
                                                body.value.done);

                return baldr::JsonResult(simdjson::to_json_string(todo),
                                         baldr::StatusCode::Created);
            });

        group.MapPut("/:id").Handle(
            [this](baldr::FromParams<IdParam> params,
                   baldr::FromBody<UpdateTodoDto>
                       body)
                -> std::variant<baldr::JsonResult, baldr::StatusResult> {
                if (body.value.title.empty())
                    return baldr::Results::Status(
                        baldr::StatusCode::BadRequest);

                auto updated = mRepository->Update(params.value.id,
                                                   std::move(body.value.title),
                                                   body.value.done);

                if (!updated)
                    return baldr::Results::Status(baldr::StatusCode::NotFound);

                return baldr::Results::Json(*updated);
            });

        group.MapDelete("/:id").Handle(
            [this](baldr::FromParams<IdParam> params) -> baldr::StatusResult {
                if (!mRepository->Delete(params.value.id))
                    return baldr::StatusResult(baldr::StatusCode::NotFound);

                return baldr::StatusResult(baldr::StatusCode::NoContent);
            });
    });
}