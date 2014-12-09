/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#ifndef BINSPECTOR_AST_HPP
#define BINSPECTOR_AST_HPP

// stdc++

// boost
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

// asl
#include <adobe/array.hpp>
#include <adobe/dictionary.hpp>

// application

/**************************************************************************************************/

typedef adobe::array_t                                        structure_type;
typedef adobe::closed_hash_map<adobe::name_t, structure_type> structure_map_t;

/**************************************************************************************************/

class ast_t
{
public:
    ast_t();

    // if the structure has not been previously specified it will be created
    void set_current_structure(adobe::name_t structure_name);

    // these operations take place on the last set_current_structure structure
    void add_named_field(adobe::name_t              name,
                         const adobe::dictionary_t& parameters);

    void add_unnamed_field(const adobe::dictionary_t& parameters);

    void add_typedef(adobe::name_t              typedef_name,
                     const adobe::dictionary_t& typedef_parameters);

    const structure_type& structure_for(adobe::name_t structure_name) const;

private:
    structure_map_t structure_map_m;
    structure_type* current_structure_m;
};

/**************************************************************************************************/

typedef std::vector<boost::filesystem::path> path_set;

/**************************************************************************************************/

ast_t make_ast(const boost::filesystem::path& template_path,
               path_set                       include_path_set);

/**************************************************************************************************/
// BINSPECTOR_AST_HPP
#endif

/**************************************************************************************************/
