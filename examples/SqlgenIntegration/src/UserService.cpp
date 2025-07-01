#include "UserService.hpp"

std::vector<User> UserService::List(int page, int take)
{
    const auto query = sqlgen::read<std::vector<User>> | sqlgen::limit(take);

    auto test = rfl::Ref<sqlgen::postgres::Connection>::make(*mDbConnection);

    auto users = query(test);

    if (users.has_value())
        return users.value();

    mLogger->LogError("Failed to fetch users: {}", users.error().what());

    return std::vector<User> {};
}
