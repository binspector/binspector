/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

#ifndef BINSPECTOR_DOT_HPP
#define BINSPECTOR_DOT_HPP

// boost
#include <boost/filesystem.hpp>

// application
#include <binspector/analyzer.hpp>

/****************************************************************************************************/

void dot_graph(const binspector_analyzer_t::structure_map_t& struct_map,
               const std::string&                            starting_struct,
               const boost::filesystem::path&                output_root);

/****************************************************************************************************/
// BINSPECTOR_DOT_HPP
#endif

/****************************************************************************************************/
