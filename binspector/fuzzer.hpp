/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

#ifndef BINSPECTOR_FUZZER_HPP
#define BINSPECTOR_FUZZER_HPP

// boost
#include <boost/filesystem.hpp>

// application
#include <binspector/forest.hpp>

/****************************************************************************************************/

void fuzz(std::istream&                  binary,
          const inspection_forest_t&     forest,
          const boost::filesystem::path& input_path,
          const boost::filesystem::path& output_root,
          bool                           path_hash,
          bool                           recurse);

/****************************************************************************************************/
// BINSPECTOR_FUZZER_HPP
#endif

/****************************************************************************************************/
