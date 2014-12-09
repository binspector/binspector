/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#ifndef BINSPECTOR_COMMON_HPP
#define BINSPECTOR_COMMON_HPP

// stdc++
#include <vector>

// boost
#include <boost/cstdint.hpp>

// asl
#include <adobe/forest.hpp>
#include <adobe/array.hpp>
#include <adobe/dictionary.hpp>

// application
#include <binspector/forest.hpp>

/**************************************************************************************************/

rawbytes_t           fetch(bitreader_t&                 input,
                           const inspection_position_t& location,
                           boost::uint64_t              bit_count);
adobe::any_regular_t evaluate(const rawbytes_t& raw,
                              boost::uint64_t   bit_length,
                              atom_base_type_t  base_type,
                              bool              is_big_endian);
adobe::any_regular_t fetch_and_evaluate(bitreader_t&                 input,
                                        const inspection_position_t& location,
                                        boost::uint64_t              bit_count,
                                        atom_base_type_t             base_type,
                                        bool                         is_big_endian);

/**************************************************************************************************/

using namespace adobe::literals; // for the _name user-defined literal.

/**************************************************************************************************/

#define CONSTANT_KEY(x) \
static const adobe::static_name_t key_##x = #x##_name

#define CONSTANT_VALUE(x) \
static const adobe::static_name_t value_##x = #x##_name

#define CONSTANT_PRIVATE_KEY(x) \
static const adobe::static_name_t private_key_##x = "."#x##_name

#include <binspector/constant_names.hpp>

/**************************************************************************************************/

enum conditional_expression_t
{
    none_k = 0,
    if_k,
    else_k
};

/**************************************************************************************************/

enum field_size_t
{
    field_size_none_k = 0,
    field_size_integer_k,
    field_size_while_k,
    field_size_terminator_k,
    field_size_delimiter_k
};

/**************************************************************************************************/

inspection_position_t starting_offset_for(inspection_branch_t branch);
inspection_position_t ending_offset_for(inspection_branch_t branch);

/**************************************************************************************************/

std::string build_path(const_inspection_branch_t main, const_inspection_branch_t branch);

/**************************************************************************************************/

template <typename T>
T contextual_evaluation_of(const adobe::array_t& expression,
                           inspection_branch_t   main_branch,
                           inspection_branch_t   current_branch,
                           bitreader_t&          input)
{
    return contextual_evaluation_of<adobe::any_regular_t>(expression, main_branch, current_branch, input).cast<T>();
}

/**************************************************************************************************/

template <>
adobe::any_regular_t contextual_evaluation_of(const adobe::array_t& expression,
                                              inspection_branch_t   main_branch,
                                              inspection_branch_t   current_branch,
                                              bitreader_t&          input);

template <>
inspection_branch_t  contextual_evaluation_of(const adobe::array_t& expression,
                                              inspection_branch_t   main_branch,
                                              inspection_branch_t   current_branch,
                                              bitreader_t&          input);

/**************************************************************************************************/

template <typename T>
T finalize_lookup(inspection_branch_t root,
                  inspection_branch_t branch,
                  bitreader_t&        input,
                  bool                finalize)
{
    return finalize_lookup<adobe::any_regular_t>(root, branch, input, finalize).cast<T>();
}

template <>
adobe::any_regular_t finalize_lookup(inspection_branch_t root,
                                     inspection_branch_t branch,
                                     bitreader_t&        input,
                                     bool                finalize);

/**************************************************************************************************/

template <typename T>
struct save_restore
{
    save_restore(T& variable) :
        variable_m(variable),
        old_value_m(variable)
    { }

    virtual ~save_restore()
    {
        variable_m = old_value_m;
    }

protected:
    T& variable_m;

private:
    T  old_value_m;
};

/**************************************************************************************************/

template <typename T>
struct temp_assignment : public save_restore<T>
{
    temp_assignment(T& variable, const T& value) :
        save_restore<T>(variable)
    {
        this->variable_m = value;
    }
};

/**************************************************************************************************/

struct attack_vector_t
{
    enum type_t
    {
        type_atom_usage_k,
        type_array_shuffle_k,
    };

    attack_vector_t(type_t                    type,
                    const std::string&        path,
                    const_inspection_branch_t branch,
                    std::size_t               count = 0) :
        type_m(type),
        path_m(path),
        branch_m(branch),
        node_m(*branch),
        count_m(count)
    { }

    type_t                    type_m;
    std::string               path_m;
    const_inspection_branch_t branch_m;
    node_t                    node_m; // Note that it's a copy!
    std::size_t               count_m;
};

inline bool operator<(const attack_vector_t& lhs, const attack_vector_t& rhs)
{
    return lhs.path_m < rhs.path_m;
}

typedef std::vector<attack_vector_t> attack_vector_set_t;

attack_vector_set_t build_attack_vector_set(const inspection_forest_t& forest);

/**************************************************************************************************/
// BINSPECTOR_COMMON_HPP
#endif

/**************************************************************************************************/
