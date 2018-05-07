#ifndef BEEHIVE_BEHAVIOR_TREE_HPP
#define BEEHIVE_BEHAVIOR_TREE_HPP

#include <cassert>
#include <functional>
#include <type_traits>
#include <vector>

/*!
 \file beehive.hpp
*/

namespace beehive
{

/*!
 \brief The status returned by process functions.
*/
enum class Status
{
    FAILURE = 0, //!< Returned when the process function has failed.
    RUNNING, //!< Returned when the outcome of process has not been determined yet.
    SUCCESS //!< Returns when the process has succeeded.
};

/*!
 \brief A handle on a process function. This should not be built directly, see #beehive::Builder.
*/
template<typename C>
struct Node
{
    using ProcessFunction = std::function<Status(Node const &self, C &context)>;

    Node(ProcessFunction process): _process(move(process)) {}

    Status process(C &context) const
    {
        return _process(*this, context);
    }

    std::vector<Node> children;

private:
    ProcessFunction _process;
};

/*!
 \brief Composites define how to run the process() function on the children.
*/
template<typename C>
using Composite = std::function<Status(std::vector<Node<C>> const &children, C &context)>;

/*!
 \brief Composite that returns success if all children return success.
*/
template<typename C>
Status sequence(std::vector<Node<C>> const &children, C &context)
{
    for (auto &child : children)
    {
        auto status = child.process(context);
        if (status != Status::SUCCESS)
        {
            return status;
        }
    }
    return Status::SUCCESS;
}

/*!
 \brief Composite that returns success on the first successful call.
*/
template<typename C>
Status selector(std::vector<Node<C>> const &children, C &context)
{
    for (auto &child : children)
    {
        auto status = child.process(context);
        if (status != Status::FAILURE)
        {
            return status;
        }
    }
    return Status::FAILURE;
}

/*!
 \brief A decorator is a composite that may only have a single child.
*/
template<typename C>
using Decorator = std::function<Status(Node<C> const &child, C &context)>;

/*!
 \brief Decorator that just returns the result of the child. Not very useful...
*/
template<typename C>
Status forwarder(Node<C> const &child, C &context)
{
    return child.process(context);
}

/*!
 \brief Decorator that inverts the result of its child node.
*/
template<typename C>
Status inverter(Node<C> const &child, C &context)
{
    const auto status = child.process(context);
    if (status == Status::RUNNING)
    {
        return status;
    }
    return status == Status::FAILURE ? Status::SUCCESS : Status::FAILURE;
}

/*!
 \brief Decorator that returns success regardless of the child result.
*/
template<typename C>
Status succeeder(Node<C> const &child, C &context)
{
    child.process(context);
    return Status::SUCCESS;
}


template<typename ReturnType, typename ContextType>
using BasicLeaf = std::function<ReturnType(ContextType &context)>; //!< Leaf nodes are the `process()` function taking the mutable context and must return a status.

template<typename C>
using Leaf = BasicLeaf<Status, C>; //!< A Leaf function takes a Context & and returns a Status.

template<typename C>
using BoolLeaf = BasicLeaf<bool, C>; //!< A Leaf function returning bool returns SUCCESS on true and FAILURE on false. It is not possible to return RUNNING from such a function.

template<typename C>
using VoidLeaf = BasicLeaf<void, C>; //!< A Leaf function returning anything other than bool or Status can be added using #beehive::BuilderBase::void_leaf. The return value is ignored and SUCCESS is returned.

/*!
 \brief A leaf that always succeeds. Not very useful...
*/
template<typename C>
Status noop(C &)
{
    return Status::SUCCESS;
}

/*!
 \brief The behavior tree class which passes the ContextType around. See #beehive::Builder for making one.
*/
template<typename ContextType>
class Tree
{
public:
    using Context = ContextType;

    /*!
     \brief Default constructor. Running process() while the tree is empty is a no-op.
    */
    Tree();
    
    /*!
     \brief Constructs a tree with the given node root.
        See #beehive::Builder.
    */
    Tree(Node<Context> root);

    /*!
     \brief Process with the given context reference.
    */
    Status process(Context &context);

    /*!
     \brief Retrieves the root.
    */
    Node<ContextType> const &root() const &;
    
    /*!
     \brief Retrieves the root.
    */
    Node<ContextType> &&root() &&;

private:
    Node<ContextType> _root;
};

template<typename C>
Tree<C>::Tree(Node<Context> root)
    : _root(std::move(root))
{}

template<typename C>
Tree<C>::Tree()
    : _root(make_leaf(Leaf<C>(&noop<C>)))
{}

template<typename C>
Status Tree<C>::process(Context &context)
{
    return _root.process(context);
}

template<typename C>
Node<C> const &Tree<C>::root() const &
{
    return _root;
}

template<typename C>
Node<C> &&Tree<C>::root() &&
{
    return std::move(_root);
}

/// @cond
template<typename C>
auto make_branch(Decorator<C> f) -> typename Node<C>::ProcessFunction;

template<typename C>
auto make_branch(Composite<C> f) -> typename Node<C>::ProcessFunction;

template<typename C>
auto make_leaf(Leaf<C> f) -> typename Node<C>::ProcessFunction;

template<typename C>
auto make_leaf(VoidLeaf<C> f) -> typename Node<C>::ProcessFunction;

template<typename C>
auto make_leaf(BoolLeaf<C> f) -> typename Node<C>::ProcessFunction;
/// @endcond

template<typename C>
class Builder;

/*!
 \brief A helper for building trees which can be instantiated as #beehive::Builder.
*/
template<typename C>
class BuilderBase
{
public:
    /// @cond
    enum class Type
    {
        COMPOSITE,
        DECORATOR,
    };
    /// @endcond

    /*!
     \brief Adds the given composite to the tree. Composites have one or more children.

     \note The composite builder must call end() to signify end of child list.
    */
    BuilderBase composite(Composite<C> composite);

    /*!
     \brief Adds the given decorator to the tree. Decorators have exactly one child.
    
     \note The decorator builder must call end() to signify the end of the child list.
    */
    BuilderBase decorator(Decorator<C> decorator);
    
    // Note: "no known conversion" warnings here could indicate that you forgot to return something from your lambda.
    /*!
     \brief Adds the given leaf to the tree. Leaves have no children.
    */
    BuilderBase &leaf(Leaf<C> leaf);
    
    /*!
     \brief Convenience wrapper so that bool functions can be used. Translates true
        result to Status::SUCCESS, false to Status::FAILURE and never returns Status:RUNNING.
    */
    BuilderBase &leaf(BoolLeaf<C> leaf);

    /*!
     \brief Convenience wrapper for a void function, or really a function returning any type other than bool or Status. This always returns Status::SUCCESS.
    */
    BuilderBase &void_leaf(VoidLeaf<C> leaf);

    /*!
     \brief Copies another tree as a subtree at the current node.
    */
    BuilderBase &tree(Tree<C> subtree);
    
    /*!
     \brief Closes the composite or decorator branch.
     
        Each call to composite() or decorator() must have a corresponding end().
    */
    BuilderBase &end();

    /*!
     \brief Finalizes the tree by returning a copy. This will assert if done while
        a decorator or composite branch is still 'open'.
    */
    virtual Tree<C> build() const &;
    
    /*!
     \brief Finalizes the tree by returning a tree constructed with the builder's
        root node. The builder is then invalid.
    */
    virtual Tree<C> build() &&;

    /*!
     \brief Shorthand for `composite(&sequence<C>)`.
    */
    BuilderBase sequence();
    
    /*!
     \brief Shorthand for `composite(&selector<C>)`.
    */
    BuilderBase selector();

    /*!
     \brief Shorthand for `decorator(&inverter<C>)`.
    */
    BuilderBase inverter();

    /*!
     \brief Shorthand for `decorator(&succeeder<C>)`.
    */
    BuilderBase succeeder();

protected:
    /// @cond
    BuilderBase(BuilderBase &parent, Node<C> *node, Type type)
        : _node(node)
        , _parent(parent)
        , _type(type)
    {}

    Node<C> *_node;

private:
    template<typename LeafType>
    BuilderBase &_leaf(LeafType &&leaf);

    template<typename BranchType>
    BuilderBase _branch(BranchType &&branch);

    BuilderBase &_parent;
    Type _type{};
    /// @endcond
};

/*!
 \brief Defines the tree structure and instantiates it.
 
    This Builder pattern is inspired by arvidsson's implementation, BrainTree.
 \sa #beehive::BuilderBase
*/
template<typename C>
class Builder
    : public BuilderBase<C>
{
public:
    /*!
     \brief The context type.
    */
    using Context = C;

    /*!
     \brief Begins construction of a tree.
    */
    Builder()
        : BuilderBase<C>(*this, nullptr, BuilderBase<C>::Type::DECORATOR)
        , _root(make_branch(Decorator<C>(&forwarder<C>)))
    {
        this->_node = &_root;
    }
    
    Builder(Builder const &) = delete; //!< Deleted copy constructor.
    Builder(Builder &&) = default; //!< Move constructor.
    Builder &operator=(Builder const &) = delete; //!< Deleted copy assignment operator.
    Builder &operator=(Builder &&) = default; //!< Move assignment operator.

    virtual Tree<C> build() const & override
    {
        assert(_root.children.size()); // must have at least one leaf node added
        return {_root};
    }

    virtual Tree<C> build() && override
    {
        assert(_root.children.size()); // must have at least one leaf node added
        return {std::move(_root)};
    }

private:
    Node<C> _root;
};

/// @cond
template<typename C>
auto make_branch(Decorator<C> f) -> typename Node<C>::ProcessFunction
{
    return [process = move(f)](Node<C> const &self, C &context)
    {
        assert(self.children.size() == 1); // invariant violation!
        return process(self.children[0], context);
    };
}

template<typename C>
auto make_branch(Composite<C> f) -> typename Node<C>::ProcessFunction
{
    return [process = move(f)](Node<C> const &self, C &context)
    {
        return process(self.children, context);
    };
}

template<typename C>
auto make_leaf(Leaf<C> f) -> typename Node<C>::ProcessFunction
{
    return [process = move(f)](Node<C> const &self, C &context)
    {
        assert(self.children.empty()); // invariant violation!
        return process(context);
    };
}

template<typename C>
auto make_leaf(VoidLeaf<C> f) -> typename Node<C>::ProcessFunction
{
    return make_leaf(Leaf<C>{[void_process = move(f)](C &context)
    {
        void_process(context);
        return Status::SUCCESS;
    }});
}

template<typename C>
auto make_leaf(BoolLeaf<C> f) -> typename Node<C>::ProcessFunction
{
    return make_leaf(Leaf<C>{[bool_process = move(f)](C &context)
    {
        const bool result = bool_process(context);
        return result ? Status::SUCCESS : Status::FAILURE;
    }});
}

template<typename C>
auto BuilderBase<C>::composite(Composite<C> composite) -> BuilderBase
{
    return _branch(std::move(composite));
}

template<typename C>
auto BuilderBase<C>::decorator(Decorator<C> decorator) -> BuilderBase
{
    return _branch(std::move(decorator));
}

template<typename C>
template<typename BranchType>
auto BuilderBase<C>::_branch(BranchType &&branch) -> BuilderBase
{
    assert((_type != Type::DECORATOR) || _node->children.empty()); // Decorators may only have one child!
    _node->children.emplace_back(make_branch(move(branch)));
    auto type = std::is_same<
        typename std::decay<BranchType>::type,
        Decorator<C>
    >::value ? Type::DECORATOR : Type::COMPOSITE;
    return {*this, &_node->children.back(), type};
}

template<typename C>
template<typename LeafType>
auto BuilderBase<C>::_leaf(LeafType &&leaf) -> BuilderBase &
{
    assert((_type != Type::DECORATOR) || _node->children.empty()); // Decorators may only have one child!
    _node->children.emplace_back(make_leaf(move(leaf)));
    return *this;
}

template<typename C>
auto BuilderBase<C>::leaf(Leaf<C> leaf) -> BuilderBase &
{
    return _leaf(std::move(leaf));
}

template<typename C>
auto BuilderBase<C>::leaf(BoolLeaf<C> leaf) -> BuilderBase &
{
    return _leaf(std::move(leaf));
}

template<typename C>
auto BuilderBase<C>::void_leaf(VoidLeaf<C> leaf) -> BuilderBase &
{
    return _leaf(std::move(leaf));
}

template<typename C>
auto BuilderBase<C>::tree(Tree<C> subtree) -> BuilderBase &
{
    assert((_type != Type::DECORATOR) || _node->children.empty()); // Decorators may only have one child!
    _node->children.emplace_back(std::move(subtree).root());
    return *this;
}

template<typename C>
auto BuilderBase<C>::end() -> BuilderBase &
{
    assert(_node->children.size()); // can't have composite/decorator without children!
    return _parent;
}

template<typename C>
auto BuilderBase<C>::build() const & -> Tree<C>
{
    assert(false); // unterminated tree!
    return {make_branch(Decorator<C>(&forwarder<C>))};
}

template<typename C>
auto BuilderBase<C>::build() && -> Tree<C>
{
    assert(false); // unterminated tree!
    return {make_branch(Decorator<C>(&forwarder<C>))};
}

#define BH_IMPLEMENT_SHORTHAND(Type, Name) \
    template<typename Context> \
    auto BuilderBase<Context>::Name() -> BuilderBase \
    { \
        return _branch(Type<Context>{&beehive::Name<Context>}); \
    }

BH_IMPLEMENT_SHORTHAND(Composite, sequence);
BH_IMPLEMENT_SHORTHAND(Composite, selector);
BH_IMPLEMENT_SHORTHAND(Decorator, inverter);
BH_IMPLEMENT_SHORTHAND(Decorator, succeeder);

#undef BH_IMPLEMENT_SHORTHAND
/// @endcond

} // namespace beehive

#endif
