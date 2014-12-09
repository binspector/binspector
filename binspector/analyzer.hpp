/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#ifndef BINSPECTOR_ANALYZER_HPP
#define BINSPECTOR_ANALYZER_HPP

// stdc++
#include <iostream>
#include <map>

// boost
#include <boost/cstdint.hpp>
#include <boost/function.hpp>

// asl
#include <adobe/array.hpp>
#include <adobe/copy_on_write.hpp>
#include <adobe/dictionary.hpp>
#include <adobe/forest.hpp>

// application
#include <binspector/ast.hpp>
#include <binspector/bitreader.hpp>
#include <binspector/common.hpp>

/**************************************************************************************************/

typedef adobe::copy_on_write<adobe::dictionary_t>               cow_dictionary_t;
typedef adobe::closed_hash_map<adobe::name_t, cow_dictionary_t> typedef_map_t;

/**************************************************************************************************/

class binspector_analyzer_t
{
public:
    explicit binspector_analyzer_t(std::istream& binary_file,
                                   const ast_t&  ast,
                                   std::ostream& output,
                                   std::ostream& error);

    void set_quiet(bool quiet);

    // analysis and results routines
    bool analyze_binary(const std::string& starting_struct);

    auto_forest_t forest()
        { return forest_m; }

private:
    // inspection related
    inspection_branch_t   new_branch(inspection_branch_t with_parent);
    bool                  analyze_with_structure(const structure_type& structure,
                                                 inspection_branch_t   parent);

    inspection_position_t make_location(boost::uint64_t bit_count)
    {
        if (current_sentry_m != invalid_position_k &&
            input_m.pos() >= current_sentry_m)
            {
            // NOT std::out_of_range -- we use that for the document EOF
            // throw std::runtime_error(error);
            error_m << current_sentry_set_path_m << " sentry barrier breach\n";
            }

        return input_m.advance(bitpos(bit_count));
    }

    std::string build_path(const_inspection_branch_t branch)
    {
        return ::build_path(forest_m->begin(), branch);
    }

    bool jump_into_structure(adobe::name_t       structure_name,
                             inspection_branch_t parent);

    bool jump_into_structure(const adobe::dictionary_t& field,
                             inspection_branch_t        parent);
    template <typename T>
    T eval_here(const adobe::array_t& expression);

    template <typename T>
    T identifier_lookup(adobe::name_t identifier);

    void signal_end_of_file();

    std::string last_path() { return build_path(current_leaf_m); }

    bitreader_t           input_m;
    std::ostream&         output_m;
    std::ostream&         error_m;
    const ast_t&          ast_m;
    inspection_branch_t   current_leaf_m;
    typedef_map_t         current_typedef_map_m;
    adobe::any_regular_t  current_enumerated_value_m;
    adobe::array_t        current_enumerated_option_set_m;
    bool                  current_enumerated_found_m;
    bitreader_t::pos_t    current_sentry_m;
    std::string           current_sentry_set_path_m;
    auto_forest_t         forest_m;
    bool                  eof_signalled_m;
    bool                  quiet_m;

    // Error reporting helper variables
    adobe::name_t last_name_m;
    std::string   last_filename_m;
    uint32_t      last_line_number_m;
};

/**************************************************************************************************/

template <typename T>
const T& value_for(const adobe::dictionary_t& dict, adobe::name_t key)
{
    adobe::dictionary_t::const_iterator found(dict.find(key));

    if (found == dict.end())
        throw std::runtime_error(adobe::make_string("Key ", key.c_str(), " not found"));

    return found->second.cast<T>();
}

/**************************************************************************************************/

template <typename T>
const T& value_for(const adobe::dictionary_t& dict, adobe::name_t key, const T& default_value)
{
    adobe::dictionary_t::const_iterator found(dict.find(key));

    return found == dict.end() ? default_value : found->second.cast<T>();
}

/**************************************************************************************************/

adobe::dictionary_t typedef_lookup(const typedef_map_t&       typedef_map,
                                   const adobe::dictionary_t& src_field);

/**************************************************************************************************/
// BINSPECTOR_ANALYZER_HPP
#endif

/**************************************************************************************************/
