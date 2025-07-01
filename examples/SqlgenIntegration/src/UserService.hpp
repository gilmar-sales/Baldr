#pragma once

#include <Skirnir/Skirnir.hpp>
#include <sqlgen/postgres.hpp>

struct User
{
    constexpr static const char* tablename = "user";
    std::string                  id;
    std::string                  email;
};

class UserService
{
  public:
    UserService(const Ref<skr::Logger<UserService>> logger,
                Ref<sqlgen::postgres::Connection>
                    dbConnection) : mLogger(logger), mDbConnection(dbConnection)
    {
    }

    std::vector<User> List(int page, int limit);

  private:
    Ref<skr::Logger<UserService>>     mLogger;
    Ref<sqlgen::postgres::Connection> mDbConnection;
};