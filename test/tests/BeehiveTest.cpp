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

TEST(BeehiveTest, NodeChildTest)
{
    using namespace beehive;
    
    auto dummyProcessor = [](int &, Node<int> const &, TreeState &) { return Status::SUCCESS; };
    std::vector<Node<int>> v {
        {dummyProcessor}, // root       // 0
            {dummyProcessor},           // 1
                {dummyProcessor},       // 2
                {dummyProcessor},       // 3
            {dummyProcessor},           // 4
            {dummyProcessor},           // 5
                {dummyProcessor},       // 6
                    {dummyProcessor},   // 7
                {dummyProcessor},       // 8
            {dummyProcessor},           // 9
    };
    for (auto i = 0; i < 4; ++i) v[0].add_child();
    for (auto i = 0; i < 2; ++i) v[1].add_child();
    for (auto i = 0; i < 2; ++i) v[5].add_child();
    v[6].add_child();
    
    EXPECT_EQ(4, v[0].child_count());
    EXPECT_EQ(2, v[1].child_count());
    EXPECT_EQ(0, v[2].child_count());
    EXPECT_EQ(0, v[3].child_count());
    EXPECT_EQ(0, v[4].child_count());
    EXPECT_EQ(2, v[5].child_count());
    EXPECT_EQ(1, v[6].child_count());
    EXPECT_EQ(0, v[7].child_count());
    EXPECT_EQ(0, v[8].child_count());
    EXPECT_EQ(0, v[9].child_count());
    
    EXPECT_EQ(v[0].first_child(), &v[1]);
    EXPECT_EQ(v[1].first_child(), &v[2]);
    EXPECT_EQ(v[1].next_sibling(), &v[4]);
    EXPECT_EQ(v[2].next_sibling(), &v[3]);
    EXPECT_EQ(v[5].next_sibling(), &v[9]);
    
    EXPECT_EQ(9, v[0].descendent_count());
    EXPECT_EQ(2, v[1].descendent_count());
    EXPECT_EQ(0, v[2].descendent_count());
    EXPECT_EQ(0, v[3].descendent_count());
    EXPECT_EQ(0, v[4].descendent_count());
    EXPECT_EQ(3, v[5].descendent_count());
    EXPECT_EQ(1, v[6].descendent_count());
    EXPECT_EQ(0, v[7].descendent_count());
    EXPECT_EQ(0, v[8].descendent_count());
    EXPECT_EQ(0, v[9].descendent_count());
}

TEST(BeehiveTest, TreeNodesTest)
{
    using namespace beehive;
    
    auto dummy = [](int &) {
        return true;
    };

    auto tree = Builder<int>{} // 0
        .sequence() // 1
            .selector() // 2
                .leaf(dummy) // 3
                .leaf(dummy) // 4
            .end()
            .leaf(dummy) // 5
            .leaf(dummy) // 6
            .sequence() // 7
                .leaf(dummy) // 8
            .end()
        .end()
        .build();

    auto &nodes = tree.nodes();
    EXPECT_EQ(9, nodes.size());
    EXPECT_EQ(nodes[1].child_count(), 4);
    EXPECT_EQ(nodes[2].child_count(), 2);
    EXPECT_EQ(nodes[3].child_count(), 0);
    EXPECT_EQ(nodes[2].next_sibling(), &nodes[5]);
    EXPECT_EQ(nodes[3].next_sibling(), &nodes[4]);
    EXPECT_EQ(nodes[1].descendent_count(), 7);
}

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

TEST(BeehiveTest, ResumeRunningTest)
{
    // Subsequent process() calls should resume at the point where they left off
    // when a RUNNING status is returned. 
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

    auto state = tree.make_state();
    EXPECT_EQ(state.resume_index, 0);
    VisitCountArray visited{};
    auto status = tree.process(state, visited);
    EXPECT_EQ(status, Status::RUNNING);
    EXPECT_EQ(state.resume_index, 1);
    EXPECT_EQ(state.offset, 1);
    EXPECT_EQ(visited[0], 1);
    EXPECT_EQ(visited[1], 1);
    EXPECT_EQ(visited[2], 0);
    status = tree.process(state, visited);
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

TEST(BeehiveTest, TreeNodeTest)
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

    VisitCountArray visited{}, visited2{};
    auto state = tree.make_state();
    auto status = tree.process(state, visited);
    EXPECT_EQ(status, Status::RUNNING);
    EXPECT_EQ(visited[0], 1);
    EXPECT_EQ(visited[1], 1);
    EXPECT_EQ(visited[2], 0);

    auto tree2 = Builder<VisitCountArray>{}
        .tree(tree)
        .build();
    auto state2 = tree2.make_state();

    EXPECT_EQ(tree2.nodes().size(), 6);
    // Expect tree2 NOT to resume where tree1 left off
    status = tree2.process(state2, visited2);
    EXPECT_EQ(status, Status::RUNNING);
    EXPECT_EQ(visited[0], 1); // Would be == 0 if resumed
    EXPECT_EQ(visited[1], 1);
    EXPECT_EQ(visited[2], 0);

    // Expect tree1 to resume where tree1 left off
    status = tree.process(state, visited);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_EQ(visited[0], 1);
    EXPECT_EQ(visited[1], 2);
    EXPECT_EQ(visited[2], 1);
    
    // Expect tree2 to resume where tree2 left off
    status = tree2.process(state2, visited2);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_EQ(visited[0], 1);
    EXPECT_EQ(visited[1], 2);
    EXPECT_EQ(visited[2], 1);
}

TEST(BeehiveTest, TreeCopyTest)
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

    VisitCountArray visited{}, visited2{};
    auto status = tree.process(visited);
    EXPECT_EQ(status, Status::RUNNING);
    EXPECT_EQ(visited[0], 1);
    EXPECT_EQ(visited[1], 1);
    EXPECT_EQ(visited[2], 0);

    // Copy tree
    auto tree2 = tree;
    
    // Expect tree2 NOT to resume where tree1 left off
    status = tree2.process(visited2);
    EXPECT_EQ(status, Status::RUNNING);
    EXPECT_EQ(visited[0], 1); // Would be == 0 if resumed
    EXPECT_EQ(visited[1], 1);
    EXPECT_EQ(visited[2], 0);

    /*
    // Expect tree1 to resume where tree1 left off
    status = tree.process(visited);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_EQ(visited[0], 1);
    EXPECT_EQ(visited[1], 2);
    EXPECT_EQ(visited[2], 1);
    */
}

TEST(BeehiveTest, CustomLeafTest)
{
    using namespace beehive;

    auto tree = Builder<bool>{}
        // Use void_leaf() when your leaf function returns nothing.
        // Automatically returns Status::SUCCESS.
        .void_leaf([](bool &state) { state = true; })
        .build();

    bool state = false;
    auto status = tree.process(state);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_TRUE(state);
    
    tree = Builder<bool>{}
        // Use leaf() when your leaf function returns something.
        // You can return true for success or false for failure.
        .leaf([](bool &state) {
            state = true;
            return true;
        })
        .build();

    state = false;
    status = tree.process(state);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_TRUE(state);

    tree = Builder<bool>{}
        // You can also return a Status enum instead.
        .leaf([](bool &state) {
            state = true;
            return Status::SUCCESS;
        })
        .build();

    state = false;
    status = tree.process(state);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_TRUE(state);
}

TEST(BeehiveTest, CustomDecoratorTest)
{
    using namespace beehive;

    // Custom decorators have one child.
    auto tree = Builder<bool>{}
        .decorator([](bool &state, Node<bool> const &child, TreeState &tree_state) {
            state = true;
            return child.process(state, tree_state);
        })
            .leaf(noop<bool>)
        .end()
        .build();

    bool state = false;
    auto status = tree.process(state);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_TRUE(state);
}

TEST(BeehiveTest, CustomCompositeTest)
{
    using namespace beehive;

    // Custom decorators have one child.
    auto tree = Builder<bool>{}
        .composite([](bool &state, Generator<bool> next_child, TreeState &ts) {
            state = true;
            // Custom implementation of sequence (aka AND)
            
            while (auto const *child = next_child()) {
                auto status = child->process(state, ts);
                if (status != Status::SUCCESS) {
                    return status;
                }
            }
            return Status::SUCCESS;
        })
            .leaf(noop<bool>)
        .end()
        .build();

    bool state = false;
    auto status = tree.process(state);
    EXPECT_EQ(status, Status::SUCCESS);
    EXPECT_TRUE(state);
}

TEST(BeehiveTest, CustomAllocatorTest)
{
    using namespace beehive;
    auto tree = Builder<bool, std::allocator<Node<bool>>>{}
        .leaf(&noop<bool>)
        .build();

}

