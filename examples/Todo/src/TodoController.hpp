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