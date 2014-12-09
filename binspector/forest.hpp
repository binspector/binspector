/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#ifndef BINSPECTOR_FOREST_HPP
#define BINSPECTOR_FOREST_HPP

// boost
#include <boost/cstdint.hpp>

// asl
#include <adobe/array.hpp>
#include <adobe/enum_ops.hpp>
#include <adobe/forest.hpp>

// application
#include <binspector/bitreader.hpp>

/**************************************************************************************************/

enum node_flags_t
{
    flags_none_k         = 0,

    /* general flags */
    type_atom_k          = 1 << 0UL,
    type_const_k         = 1 << 1UL,
    type_skip_k          = 1 << 2UL,
    type_slot_k          = 1 << 3UL,
    type_struct_k        = 1 << 4UL,
    is_array_root_k      = 1 << 5UL,
    is_array_element_k   = 1 << 6UL,

    /* atom flags */
    atom_is_big_endian_k = 1 << 7UL,
};

ADOBE_DEFINE_BITSET_OPS(node_flags_t);

template <typename EnumType>
inline void enum_set(EnumType& enumeration, EnumType flag, bool value = true)
{
    if (value)
        enumeration |= flag;
    else
        enumeration &= ~flag;
}

template <typename EnumType>
inline bool enum_get(const EnumType& enumeration, EnumType flag)
{
    return (enumeration & flag) != 0;
}

/**************************************************************************************************/

enum atom_base_type_t
{
    atom_unknown_k = 0,
    atom_signed_k,
    atom_unsigned_k,
    atom_float_k,
};

/**************************************************************************************************/

struct node_t
{
    node_t() :
        flags_m(flags_none_k),
        type_m(atom_unknown_k),
        start_offset_m(invalid_position_k),
        end_offset_m(invalid_position_k),
        cardinal_m(0),
        shuffle_m(false),
        bit_count_m(0),
        use_count_m(0),
        evaluated_m(false),
        no_print_m(false)
    { }

    void set_flag(node_flags_t flag, bool value = true)
        { enum_set(flags_m, flag, value); }
    bool get_flag(node_flags_t flag) const
        { return enum_get(flags_m, flag); }

    /* general fields */
    node_flags_t          flags_m;
    atom_base_type_t      type_m;
    adobe::name_t         name_m;
    std::string           summary_m; // individual array elements have different summaries, that's the point.

    /* struct or array root fields */
    inspection_position_t start_offset_m;
    inspection_position_t end_offset_m;

    /* array root / array element fields */
    boost::uint64_t       cardinal_m; // size for array root; index for array element
    bool                  shuffle_m;  // let hairbrain shuffle these array elements around

    /* atom fields */
    boost::uint64_t       bit_count_m;
    inspection_position_t location_m; // atom's location in the binary file
    std::size_t           use_count_m; // incremented at each call to fetch_and_evaluate

    /* const and slot fields */
    adobe::array_t        expression_m;
    bool                  evaluated_m; // we do lazy evaluation; cache the result and flag
    adobe::any_regular_t  evaluated_value_m;
    bool                  no_print_m; // don't print this constant during output

    /* struct fields */
    adobe::name_t         struct_name_m;

    /* enumerated fields */
    adobe::array_t        option_set_m;
};

#define NODE_PROPERTY_NAME           (&node_t::name_m)

#define NODE_PROPERTY_IS_ATOM        type_atom_k
#define NODE_PROPERTY_IS_CONST       type_const_k
#define NODE_PROPERTY_IS_SKIP        type_skip_k
#define NODE_PROPERTY_IS_SLOT        type_slot_k
#define NODE_PROPERTY_IS_STRUCT      type_struct_k

#define ARRAY_ROOT_PROPERTY_SIZE     (&node_t::cardinal_m)
#define ARRAY_ROOT_PROPERTY_SHUFFLE  (&node_t::shuffle_m)

#define ARRAY_ELEMENT_VALUE_INDEX    (&node_t::cardinal_m)

#define ATOM_PROPERTY_BASE_TYPE      (&node_t::type_m)
#define ATOM_PROPERTY_IS_BIG_ENDIAN  atom_is_big_endian_k
#define ATOM_PROPERTY_BIT_COUNT      (&node_t::bit_count_m)
#define ATOM_VALUE_LOCATION          (&node_t::location_m)
#define ATOM_VALUE_USE_COUNT         (&node_t::use_count_m)

#define SKIP_VALUE_LOCATION          (&node_t::location_m)

#define CONST_VALUE_EXPRESSION       (&node_t::expression_m)
#define CONST_VALUE_IS_EVALUATED     (&node_t::evaluated_m)
#define CONST_VALUE_EVALUATED_VALUE  (&node_t::evaluated_value_m)
#define CONST_VALUE_NO_PRINT         (&node_t::no_print_m)

#define SLOT_VALUE_EXPRESSION        (&node_t::expression_m)

#define STRUCT_PROPERTY_STRUCT_NAME  (&node_t::struct_name_m)

/**************************************************************************************************/

typedef node_t                              forest_node_t;
typedef adobe::forest<forest_node_t>        inspection_forest_t;
typedef inspection_forest_t::iterator       inspection_branch_t;
typedef inspection_forest_t::const_iterator const_inspection_branch_t;
typedef inspection_forest_t::const_preorder_iterator const_preorder_iterator_t;
typedef adobe::depth_fullorder_iterator<boost::range_iterator<inspection_forest_t>::type> depth_full_iterator_t;
typedef std::auto_ptr<inspection_forest_t>  auto_forest_t;

template <typename T>
struct node_member
{
    typedef T(node_t::*value)() const;
};

/**************************************************************************************************/
// A property is something generic to the node's type, like structure name or endianness.
// If the node is an array element we get the property from the parent, otherwise itself.
template <typename T>
T node_property(const_inspection_branch_t branch, T(node_t::*member_function)()const)
{
    const forest_node_t& node(*branch);
    bool                 is_array_element(node.get_flag(is_array_element_k));

    if (is_array_element)
        return node_property<T>(adobe::find_parent(branch), member_function);

    return (node.*member_function)();
}

template <typename T>
T node_property(const_inspection_branch_t branch, T node_t::*member)
{
    const forest_node_t& node(*branch);
    bool                 is_array_element(node.get_flag(is_array_element_k));

    if (is_array_element)
        return node_property<T>(adobe::find_parent(branch), member);

    return node.*member;
}

inline bool node_property(const_inspection_branch_t branch, node_flags_t flag)
{
    const forest_node_t& node(*branch);
    bool                 is_array_element(node.get_flag(is_array_element_k));

    if (is_array_element)
        return node_property(adobe::find_parent(branch), flag);

    return node.get_flag(flag);
}

inline const_inspection_branch_t property_node_for(const_inspection_branch_t branch)
{
    return branch->get_flag(is_array_element_k) ?
               property_node_for(adobe::find_parent(branch)) :
               branch;
}

/**************************************************************************************************/
// A value is something specific to this node, like location or name.
// If the node is an array root, we get the value from the first child, otherwise itself.
template <typename T>
T node_value(const_inspection_branch_t branch, T(node_t::*member_function)()const)
{
    const forest_node_t& node(*branch);
    bool                 is_array_root(node.get_flag(is_array_root_k));

    if (is_array_root)
    {
        inspection_forest_t::const_child_iterator first_child(adobe::child_begin(branch));
        inspection_forest_t::const_child_iterator last_child(adobe::child_end(branch));

        if (first_child != last_child)
            return node_value<T>(first_child.base(), member_function);
    }

    return (node.*member_function)();
}

template <typename T>
T node_value(const_inspection_branch_t branch, T node_t::*member)
{
    const forest_node_t& node(*branch);
    bool                 is_array_root(node.get_flag(is_array_root_k));

    if (is_array_root)
    {
        inspection_forest_t::const_child_iterator first_child(adobe::child_begin(branch));
        inspection_forest_t::const_child_iterator last_child(adobe::child_end(branch));

        if (first_child != last_child)
            return node_value<T>(first_child.base(), member);
    }

    return node.*member;
}

/**************************************************************************************************/
// BINSPECTOR_FOREST_HPP
#endif

/**************************************************************************************************/
