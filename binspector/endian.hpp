/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

#ifndef BINSPECTOR_ENDIAN_HPP
#define BINSPECTOR_ENDIAN_HPP

// stdc++
#include <algorithm>

// boost

// asl

// application

/****************************************************************************************************/

enum class endian { little, big };

#if __LITTLE_ENDIAN__ || defined(_M_IX86) || defined(_WIN32)
constexpr endian endian_k{endian::little};

#define BINSPECTOR_ENDIAN_LITTLE 1
#define BINSPECTOR_ENDIAN_BIG 0
#endif

#if __BIG_ENDIAN__
constexpr endian endian_k{endian::big};

#define BINSPECTOR_ENDIAN_BIG 1
#define BINSPECTOR_ENDIAN_LITTLE 0
#endif

constexpr bool endian_big_k{endian_k == endian::big};
constexpr bool endian_little_k{endian_k == endian::little};

template <typename C>
void host_to_endian(C& c, bool embiggen) {
    if (embiggen != endian_big_k)
        std::reverse(begin(c), end(c));
}

/****************************************************************************************************/
// BINSPECTOR_ENDIAN_HPP
#endif

/****************************************************************************************************/
