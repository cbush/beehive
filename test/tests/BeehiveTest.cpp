#include <beehive/beehive.hpp>

#include <gtest/gtest.h>
#include <array>

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

TEST(BeehiveTest, ExampleTest)
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

TEST(BeehiveTest, ResumePendingTest)
{
    using namespace beehive;

    using VisitCountArray = std::array<int, 3>;

    auto tree = Builder<VisitCountArray>{}
        .sequence()
            .leaf([](VisitCountArray &visited) {
                // This never returns RUNNING, so it should never be visited twice.
                return ++visited[0] > 1 ? Status::FAILURE : Status::SUCCESS;
            })
            .leaf([](VisitCountArray &visited) {
                // This should be visited twice after returning RUNNING.
                return ++visited[1] > 1 ? Status::SUCCESS : Status::RUNNING;
            })
            .leaf([](VisitCountArray &visited) {
                // This should only be visited once.
                ++visited[2];
                return true;
            })
        .end()
        .build();

    VisitCountArray visited{};
    auto status = tree.process(visited);
    EXPECT_EQ(status, Status::RUNNING);
    EXPECT_EQ(visited[0], 1);
    EXPECT_EQ(visited[1], 1);
    EXPECT_EQ(visited[2], 0);
    status = tree.process(visited);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_EQ(visited[0], 1);
    EXPECT_EQ(visited[1], 2);
    EXPECT_EQ(visited[2], 1);
}

TEST(BeehiveTest, SequenceTest)
{
    using namespace beehive;
    using VisitedArray = std::array<bool, 3>;

    auto tree = Builder<VisitedArray>{}
        .sequence()
            .leaf([](VisitedArray &visited) {
                visited[0] = true;
                return false;
            })
            .leaf([](VisitedArray &visited) {
                visited[1] = true;
                return true;
            })
            .leaf([](VisitedArray &visited) {
                visited[2] = true;
                return true;
            })
        .end()
        .build();

    VisitedArray visited{};
    auto status = tree.process(visited);
    EXPECT_EQ(status, Status::FAILURE);
    EXPECT_TRUE(visited[0]);
    EXPECT_FALSE(visited[1]);
    EXPECT_FALSE(visited[2]);

    tree = Builder<VisitedArray>{}
        .sequence()
            .leaf([](VisitedArray &visited) {
                visited[0] = true;
                return true;
            })
            .leaf([](VisitedArray &visited) {
                visited[1] = true;
                return Status::RUNNING;
            })
            .leaf([](VisitedArray &visited) {
                visited[2] = true;
                return true;
            })
        .end()
        .build();

    visited = {};
    status = tree.process(visited);
    EXPECT_EQ(status, Status::RUNNING);
    EXPECT_TRUE(visited[0]);
    EXPECT_TRUE(visited[1]);
    EXPECT_FALSE(visited[2]);
    
    tree = Builder<VisitedArray>{}
        .sequence()
            .leaf([](VisitedArray &visited) {
                visited[0] = true;
                return true;
            })
            .leaf([](VisitedArray &visited) {
                visited[1] = true;
                return true;
            })
            .leaf([](VisitedArray &visited) {
                visited[2] = true;
                return true;
            })
        .end()
        .build();

    visited = {};
    status = tree.process(visited);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_TRUE(visited[0]);
    EXPECT_TRUE(visited[1]);
    EXPECT_TRUE(visited[2]);
}

TEST(BeehiveTest, SelectorTest)
{
    using namespace beehive;
    using VisitedArray = std::array<bool, 3>;

    auto tree = Builder<VisitedArray>{}
        .selector()
            .leaf([](VisitedArray &visited) {
                visited[0] = true;
                return false;
            })
            .leaf([](VisitedArray &visited) {
                visited[1] = true;
                return true;
            })
            .leaf([](VisitedArray &visited) {
                visited[2] = true;
                return true;
            })
        .end()
        .build();

    VisitedArray visited{};
    auto status = tree.process(visited);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_TRUE(visited[0]);
    EXPECT_TRUE(visited[1]);
    EXPECT_FALSE(visited[2]);

    tree = Builder<VisitedArray>{}
        .selector()
            .leaf([](VisitedArray &visited) {
                visited[0] = true;
                return false;
            })
            .leaf([](VisitedArray &visited) {
                visited[1] = true;
                return Status::RUNNING;
            })
            .leaf([](VisitedArray &visited) {
                visited[2] = true;
                return true;
            })
        .end()
        .build();

    visited = {};
    status = tree.process(visited);
    EXPECT_EQ(status, Status::RUNNING);
    EXPECT_TRUE(visited[0]);
    EXPECT_TRUE(visited[1]);
    EXPECT_FALSE(visited[2]);
    
    tree = Builder<VisitedArray>{}
        .selector()
            .leaf([](VisitedArray &visited) {
                visited[0] = true;
                return false;
            })
            .leaf([](VisitedArray &visited) {
                visited[1] = true;
                return false;
            })
            .leaf([](VisitedArray &visited) {
                visited[2] = true;
                return false;
            })
        .end()
        .build();

    visited = {};
    status = tree.process(visited);
    EXPECT_EQ(status, Status::FAILURE);
    EXPECT_TRUE(visited[0]);
    EXPECT_TRUE(visited[1]);
    EXPECT_TRUE(visited[2]);
}


TEST(BeehiveTest, InverterTest)
{
    using namespace beehive;

    auto tree = Builder<const int>{}
        .inverter()
            .leaf([](int) {
                return Status::RUNNING;
            })
        .end()
        .build();
    
    auto status = tree.process(1);
    EXPECT_EQ(status, Status::RUNNING);

    tree = Builder<const int>{}
        .inverter()
            .leaf([](int) {
                return false;
            })
        .end()
        .build();
    
    status = tree.process(1);
    EXPECT_EQ(status, Status::SUCCESS);

    tree = Builder<const int>{}
        .inverter()
            .leaf([](int) {
                return true;
            })
        .end()
        .build();
    
    status = tree.process(1);
    EXPECT_EQ(status, Status::FAILURE);
}

TEST(BeehiveTest, SucceederTest)
{
    using namespace beehive;

    auto tree = Builder<const int>{}
        .sequence()
            .succeeder()
                .leaf([](int) { return true; })
            .end()
            .succeeder()
                .leaf([](int) { return Status::RUNNING; })
            .end()
            .succeeder()
                .leaf([](int) { return false; })
            .end()
        .end()
        .build();
    
    auto status = tree.process(1);
    EXPECT_EQ(status, Status::SUCCESS);
}
