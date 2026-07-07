#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

struct UserDto
{
    std::string name;
    int         age = 0;
};

struct IdArg
{
    std::string id;
};

struct AddressDto
{
    std::string city;
    std::string street;
};

struct NestedUserDto
{
    std::string                     name;
    AddressDto                      address;
    std::optional<int>              score;
    std::vector<std::string>        tags;
    std::array<int, 3>              ratings;
    std::optional<AddressDto>       billing;
    std::optional<std::vector<int>> luckyNumbers;
};

struct OptionalOnlyDto
{
    std::optional<int>         maybeInt;
    std::optional<std::string> maybeString;
    std::optional<AddressDto>  maybeAddress;
};
