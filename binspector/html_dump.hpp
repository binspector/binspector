/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

#ifndef BINSPECTOR_HTML_DUMP_HPP
#define BINSPECTOR_HTML_DUMP_HPP

// stdc++
#include <iostream>
#include <string>

// application
#include <binspector/common.hpp>

/****************************************************************************************************/

void binspector_html_dump(std::istream&      input,
                          auto_forest_t      aforest,
                          std::ostream&      output,
                          const std::string& coutput,
                          const std::string& cerrput);

/****************************************************************************************************/
// BINSPECTOR_HTML_DUMP
#endif

/****************************************************************************************************/
