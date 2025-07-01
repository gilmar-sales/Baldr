#include <Baldr/Baldr.hpp>

#include "UserService.hpp"
#include <sqlgen/postgres.hpp>

int main()
{
    auto builder = skr::ApplicationBuilder().AddExtension(BaldrExtension());

    builder.GetServiceCollection().AddScoped<sqlgen::postgres::Connection>(
        [](skr::ServiceProvider& provider) {
            const auto credentials = sqlgen::postgres::Credentials {
                .user     = "postgres",
                .password = "todo-pass",
                .host     = "localhost",
                .dbname   = "postgres",
                .port     = 9000
            };

            const auto conn = sqlgen::postgres::connect(credentials);

            if (!conn)
            {
                throw std::runtime_error("Failed to connect to the database: " +
                                         conn.error().what());
            }

            return conn.value().ptr();
        });
    builder.GetServiceCollection().AddTransient<UserService>();

    auto app = builder.Build<WebApplication>();

    app->MapGet("/users",
                [](Ref<UserService> userService, HttpRequest& request) {
                    return userService->List(1, 10);
                });

    app->Run();

    return 0;
}