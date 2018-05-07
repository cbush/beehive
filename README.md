# 🐝 beehive 🐝

A flexible, modern C++ implementation of behavior trees [as described by Chris Simpson](https://www.gamasutra.com/blogs/ChrisSimpson/20140717/221339/).

Behavior trees are often applied to writing AI systems in video games. See [the article on Wikipedia](https://en.wikipedia.org/wiki/Behavior_tree_(artificial_intelligence%2C_robotics_and_control)) for another overview.

# Overview

- Header-only, C++14 template library with no other dependencies.
- Primitive composites and decorators are provided -- e.g. sequence, selector, inverter, etc. -- but you can always add your own!
- Nodes do not have to subclass from anything -- they are just functions.
- Leaf nodes are just process functions with the signature `beehive::Status operator()(Context &context)`. In other words, nodes can be a functor, static function, lambda, etc.
- Dynamic allocation can be kept to a minimum. See Future vision below.

## License

Beehive is provided under the [zlib license](https://en.wikipedia.org/wiki/Zlib_License). This means you can do whatever you want with it, including commercial applications. See [LICENSE](https://github.com/crust/beehive/blob/master/LICENSE).

## Installation

Add this repo as a submodule, then add <path/to/beehive>/include/ to your build.

This project uses CMake. If you are also using CMake, you can `add_subdirectory(path/to/beehive)` and then use `target_link_libraries(YourTarget, beehive)`

In your project, `#include <beehive/beehive.hpp>`.

## Future vision

### Re: memory

Currently, beehive is implemented with std::vector and std::function. The latter on most implementations will have a small function optimization allowing storage of lambdas with small captures and other functors without dynamically allocating space for them. If there's interest, I could wrap the nodes in a custom type-erasing function wrapper that has more inline space for functors, which would further reduce allocation frequency.

Nodes in the tree are naïvely implemented with vectors of their own child nodes, which also leads to unnecessary allocations (still a step up from forcing every node to be a shared_ptr, but anyway...). I'd like to update this so that the tree holds all of the memory up-front.

### Re: resetting stateful nodes/subtrees

Nodes do not have a reset() function, so the tree needs to be rebuilt in order to reinitialize any stateful nodes. This can be mitigated with the builder's `.tree()` function, which allows reuse of builder code for sections of the tree. It would be possible to add support for an optional reset() function in the future, but I might sooner add more functionality around replacing subtrees. It remains to be seen after more real-world use.


# Get started!

If you have not read Chris Simpson's [blog post on the subject](https://www.gamasutra.com/blogs/ChrisSimpson/20140717/221339/), you should do so now. The terminology used in Beehive closely matches that defined or used by Chris Simpson.

## Use the Builder

Trees do not need to be built directly. Instead, use the Builder class to define the tree structure:

    #include <beehive/beehive.hpp>

    struct Context {};

    int main() {
        using namespace beehive;
        auto tree = Builder<Context>{}
            .leaf([](Context &context) {
                auto success = DoSomething();
                return success ? Status::SUCCESS : Status::FAILURE;
            })
            .build();
    }

This builds a behavior tree that looks like this:

            Root
             |
             v
        Do Something

Not bad, but too simplistic to be useful. Take the example from Chris's blog post:

![alt text](https://camo.githubusercontent.com/8897396ec4253a0fd0a7918b2d47fb0c555ce24b/68747470733a2f2f6f7574666f726166696768742e66696c65732e776f726470726573732e636f6d2f323031342f30372f696d61676530302e706e67 "An example from Chris Simpson's blog")

That tree could be defined as follows:

    auto tree = Builder<ZombieState>{}
        .sequence()
            .leaf([](ZombieState &zombie) -> Status {
                return zombie.is_hungry ? Status::SUCCESS : Status::FAILURE;
            })
            .leaf(&ZombieState::has_food)
            .inverter()
                .leaf(EnemiesAroundChecker{})
            .end()
            .void_leaf(&ZombieState::eat_food)
        .end()
        .build();

### Building composites and decorators (branch nodes)

The above example uses many of the primitives available of Beehive. Note that the convenience functions `.sequence()` and `.inverter()` were used: these are shorthands for `.composite(&beehive::sequence<ZombieState>)` and `.decorator(&beehive::inverter<ZombieState>)`, respectively. You can pass any callable -- including lambdas, free functions, functors, static member functions and member functions -- to `.composite()` that matches this callable signature:

    beehive::Status operator()(std::vector<beehive::Node> const &child_nodes, Context &context);
    
This function is the process function called when the Tree is on this node. In this, you can iterate through the provided child nodes (calling `process()` on them) in whatever order you wish, then return a `beehive::Status`.

Similarly, you can pass any callable of the following signature to `.decorator()`:

    beehive::Status operator()(beehive::Node const &child, Context &context);


### Use `.end()` when done with branch nodes

The branch nodes (i.e. composites and decorators) require a corresponding call to `.end()` on the builder, kind of like closing a tag in XML. Leaf nodes do not need an `.end()` because they can't have any children.

(Technically decorators wouldn't need an `.end()` either, because they can only have exactly one child, but when laying it out with the appropriate indentation, it looks weird.) It helps to format the calls so that the builder code's indentation matches the nesting level of the calls. But don't worry! However you want to format it, if you make a mistake in the builder, Beehive will assert when you try to run your code. (TODO: Use exceptions?)

The tree's root node is essentially a decorator that just returns the result of its child `process()` function. Therefore the root can not have more than one child. Use `.build()` to finalize the tree.

### Building leaf nodes

Defining a leaf node is a matter of providing the process function. This is the function that the tree will run when it reaches that node. The functions can take many forms, but they boil down to this signature:

    beehive::Status operator()(Context &);

where Context is the template parameter passed in to the Builder in the first place.

Use the `.leaf()` function on the builder to add a leaf. No corresponding `.end()` call is required, as leaves may not have children.

Actually, it's possible to return `bool` from your leaf node process function, in case it never returns beehive::Status::RUNNING. Return true for SUCCESS, false for FAILURE.

If you want to use a function with any return type and do not care about the actual result, you can use the builder's `.void_leaf()` function. The resulting leaf will call whatever function you passed and always return SUCCESS.

In the above example, various approaches to adding the function are demonstrated:

- with a lambda: `.leaf(&ZombieState::has_food) { return beehive::Status::SUCCESS; }`
- with a member function pointer: `.leaf(&ZombieState::has_food)` (given `bool ZombieState::has_food() const`). This works because the `ZombieState &` passed in as the first parameter becomes the `this` of the member function call.
- with a functor: `.leaf(EnemiesAroundChecker{})` -- given

        struct EnemiesAroundChecker {
            beehive::Status operator()(ZombieState const &) const;
            // maybe additional state/context members
        }

- with a void member function: `.void_leaf(&ZombieState::eat_food)`. The result of `ZombieState::eat_food()` is ignored.

Additionally, you could use a static member function pointer, a free function pointer or a lambda with captures.


### The Context object

Your tree is associated with some context object defined by you. That context type will be specified in the template parameter for pretty much every template in Beehive: Tree<Context>, Builder<Context>, etc.

Whenever you call `process()` on a tree, you pass in the Context object by reference. That instance will be passed around to all the nodes that the process call reaches.

The above example used a `ZombieState` context similar to this:

    struct ZombieState // An app-specific context object
    {
        bool is_hungry{true};
        void eat_food();
        
        beehive::Status has_food() const
        {
            return _has_food? beehive::Status::SUCCESS : beehive::Status::FAILURE;
        }
        
        bool _has_food{true};
    };


### Attaching another tree

Suppose you already have a tree built for the given Context type. You can reuse the builder code by using the builder's `.tree()` function to attach the entire tree as a child.

    // given `tree` above
    auto tree2 = Builder<ZombieState>{}
        .sequence()
            .tree(tree)
            // ... etc.
        .end()
        .build();


### Process

Once the tree is built, you can run `process()` on it with a Context instance. When and how you decide to recreate the instance is up to you.

    ZombieState state = make_state(); // initialized state
    tree.process(state);


# Primitives reference

The following section outlines the out-of-the-box Composites, Decorators and Leafs available in Beehive, found in namespace `beehive`. This list may not be entirely complete. See the docs (Doxygen) for details. The `<C>` means template parameter Context. Lower-case names means it's a function, capitals means it is a class/functor.

## Composites

Composites have at least 1 child and are used to filter their child's process result.

### `sequence<C>`

Processes child nodes in order. Logical AND of the child nodes.

- Returns SUCCESS if all children returned success.
- Stops processing and returns RUNNING if a child returned RUNNING.
- Stops processing and returns FAILURE if a child returned FAILURE.

### `selector<C>`

Processes child nodes in order. Logical OR of the child nodes.

- Stops processing and returns SUCCESS if any child returned SUCCESS.
- Stops processing and returns RUNNING if any child returned RUNNING.
- Returns FAILURE if no child returned SUCCESS or RUNNING.

## Decorators

Decorators are composites with exactly 1 child.

### `inverter<C>`

- Returns SUCCESS if the child returned FAILED.
- Returns PENDING if the child returned PENDING.
- Returns FAILED if the child returned SUCCESS.

### `succeeder<C>`

- Returns SUCCESS regardless of the child's status.

## Leafs

Nothing here.

