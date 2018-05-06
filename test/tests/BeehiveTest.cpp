#include <beehive/beehive.hpp>

#include <gtest/gtest.h>

struct ZombieState
{
    bool is_hungry{true};
    void eat_food() {};

    bool has_food() const
    {
        return _has_food;
    }
    
    bool _has_food{true};
};

struct EnemiesAroundChecker
{
    bool operator()(ZombieState &) const {
        return _enemies_around;
    }
    bool _enemies_around{false};
};

TEST(BeehiveTest, BuilderTest)
{
    using namespace beehive;
    auto tree = Builder<ZombieState>{}
        .sequence()
            .leaf([](ZombieState &zombie) -> Status { // Passing a lambda
                return zombie.is_hungry ? Status::SUCCESS : Status::FAILURE;
            })
            .leaf(&ZombieState::has_food) // Passed a member function pointer
            .leaf([](ZombieState &x) { return true; })
            .inverter()
                .leaf(EnemiesAroundChecker{}) // Passing functor
            .end()
            .void_leaf(&ZombieState::eat_food) // Void member function
        .end()
        .build();
}
