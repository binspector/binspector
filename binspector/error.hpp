/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

#ifndef BINSPECTOR_ERROR_HPP
#define BINSPECTOR_ERROR_HPP

// stdc++
#include <stdexcept>

/****************************************************************************************************/

#define require(cond)                        \
    do {                                     \
        if (!(cond))                         \
            throw std::runtime_error(#cond); \
    } while (false);

/****************************************************************************************************/
// BINSPECTOR_ERROR_HPP
#endif

/****************************************************************************************************/
