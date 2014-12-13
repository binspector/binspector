/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#ifndef BINSPECTOR_STRING_HPP
#define BINSPECTOR_STRING_HPP

// stdc++
#include <string>

/**************************************************************************************************/

namespace detail {

/**************************************************************************************************/

inline std::size_t make_string_size(const char* value)
{
    return std::strlen(value);
}

inline std::size_t make_string_size(const std::string& value)
{
    return value.size();
}

template <typename T, typename... Args>
inline std::size_t make_string_size(const T& first, Args... args)
{
    return make_string_size(first) + make_string_size(args...);
}

template <typename T>
inline void make_string_append(std::string& result, const T& value)
{
    result += value;
}

template <typename T, typename... Args>
inline void make_string_append(std::string& result, const T& first, Args... args)
{
    make_string_append(result, first);

    make_string_append(result, args...);
}

/**************************************************************************************************/

} // namespace detail

/**************************************************************************************************/

template <typename T, typename... Args>
inline std::string make_string(const T& first, Args... args)
{
    std::string result;

    result.reserve(detail::make_string_size(first, args...));

    detail::make_string_append(result, first, args...);

    return result;
}

/**************************************************************************************************/
// BINSPECTOR_STRING_HPP
#endif

/**************************************************************************************************/
