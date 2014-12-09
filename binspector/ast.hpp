/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#ifndef BINSPECTOR_AST_HPP
#define BINSPECTOR_AST_HPP

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
#include <binspector/bitreader.hpp>
#include <binspector/common.hpp>

/**************************************************************************************************/

typedef adobe::array_t                                        structure_type;
typedef adobe::closed_hash_map<adobe::name_t, structure_type> structure_map_t;

/**************************************************************************************************/

class ast_t
{
public:
    // if the structure has not been previously specified it will be created
    void set_current_structure(adobe::name_t structure_name);

    // these operations take place on the last set_current_structure structure
    void add_named_field(adobe::name_t              name,
                         const adobe::dictionary_t& parameters);

    void add_unnamed_field(const adobe::dictionary_t& parameters);

    void add_typedef(adobe::name_t              typedef_name,
                     const adobe::dictionary_t& typedef_parameters);

    void set_quiet(bool quiet);

    structure_map_t& structure_map() const
        { return structure_map_m; }

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
};

/**************************************************************************************************/
// BINSPECTOR_AST_HPP
#endif

/**************************************************************************************************/
