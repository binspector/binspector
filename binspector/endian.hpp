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
#include <boost/endian.hpp>

// asl

// application

/****************************************************************************************************/

enum class endian { little, big };

#if defined(BOOST_ENDIAN_LITTLE_BYTE)
    constexpr endian endian_k{endian::little};

    #define BINSPECTOR_ENDIAN_LITTLE 1
    #define BINSPECTOR_ENDIAN_BIG 0
#elif defined(BOOST_ENDIAN_BIG_BYTE)
    constexpr endian endian_k{endian::big};

    #define BINSPECTOR_ENDIAN_BIG 1
    #define BINSPECTOR_ENDIAN_LITTLE 0
#else
    #error Endianness not detected.
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
