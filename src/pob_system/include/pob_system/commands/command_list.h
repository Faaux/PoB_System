#pragma once
#include <array>
#include <vector>

template <typename T>
concept executable = requires(T t)
{
    t.execute();
};

struct base_executable_wrapper
{
    virtual void execute() {}
};
static_assert(executable<base_executable_wrapper>, "base_executable_wrapper needs to be an executable");

template <executable T>
struct executable_pointer_storage : base_executable_wrapper
{
    void execute() override { t->execute(); }
    T* t;
};
static_assert(executable<executable_pointer_storage<base_executable_wrapper>>,
              "executable_pointer_storage needs to be an executable");

static_assert(sizeof(executable_pointer_storage<base_executable_wrapper>) == 16);

struct command_list
{
    using generic_storage = executable_pointer_storage<base_executable_wrapper>;
    template <executable T>
    void add(T* t)
    {
        static_assert(std::is_trivially_destructible<executable_pointer_storage<T>>::value);

        auto& mem = commands.emplace_back();
        auto e = new (mem.data()) executable_pointer_storage<T>;
        e->t = t;
    }

    std::vector<std::array<char, sizeof(generic_storage)>> commands;

    void clear()
    {
        for (auto& command : commands)
        {
            delete command.data();
        }

        commands.clear();
    }
};