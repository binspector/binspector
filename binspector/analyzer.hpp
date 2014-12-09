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

class binspector_analyzer_t
{
public:
    typedef adobe::array_t                                                                    structure_type;
    typedef adobe::closed_hash_map<adobe::name_t, structure_type>                             structure_map_t;
    typedef adobe::closed_hash_map<adobe::name_t, adobe::copy_on_write<adobe::dictionary_t> > typedef_map_t;

    explicit binspector_analyzer_t(std::istream& binary_file,
                                   std::ostream& output,
                                   std::ostream& error);

    // if the structure has not been previously specified it will be created
    void set_current_structure(adobe::name_t structure_name);

    // these operations take place on the last set_current_structure structure
    void add_named_field(adobe::name_t              name,
                         const adobe::dictionary_t& parameters);

    void add_unnamed_field(const adobe::dictionary_t& parameters);

    void add_typedef(adobe::name_t              typedef_name,
                     const adobe::dictionary_t& typedef_parameters);

    void set_quiet(bool quiet);

    // analysis and results routines
    bool analyze_binary(const std::string& starting_struct);

    auto_forest_t forest()
        { return forest_m; }

    const structure_map_t& structure_map() const
        { return structure_map_m; }

private:
    // inspection related
    inspection_branch_t   new_branch(inspection_branch_t with_parent);
    bool                  analyze_with_structure(const structure_type& structure,
                                                 inspection_branch_t   parent);
    const structure_type& structure_for(adobe::name_t structure_name);

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
    structure_map_t       structure_map_m;
    structure_type*       current_structure_m;
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

adobe::dictionary_t typedef_lookup(const binspector_analyzer_t::typedef_map_t& typedef_map,
                                   const adobe::dictionary_t&                  src_field);

/**************************************************************************************************/
// BINSPECTOR_ANALYZER_HPP
#endif

/**************************************************************************************************/
