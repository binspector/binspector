/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

#ifndef BINSPECTOR_OSTREAM_HPP
#define BINSPECTOR_OSTREAM_HPP

// stdc++
#include <stdexcept>

// stlab
#include <stlab/scope.hpp>

/****************************************************************************************************/

namespace detail {

/****************************************************************************************************/

struct helper {
    template <typename F>
    explicit helper(F&& f) : _commit(std::forward<F>(f)) {}

    helper(helper&&) = default;

    ~helper() {
        _commit(_ss.str());
    }

    template <typename U>
    void append(U&& x) {
        _ss << std::forward<U>(x);
    }

    std::stringstream                _ss;
    std::function<void(std::string)> _commit;
};

/****************************************************************************************************/

template <typename U>
// coverity[pass_by_value] 
helper operator<<(helper s, U&& x) {
    s.append(std::forward<U>(x));
    return s;
}

/****************************************************************************************************/

} // namespace detail

/****************************************************************************************************/
// tsos = thread safe ostream

template <typename T>
class tsos {
    friend class helper;

    T          _ostream;
    std::mutex _m;

public:
    template <typename... Args>
    explicit tsos(Args&&... args) : _ostream(std::forward<Args>(args)...) {}

    void commit(std::string s) {
        stlab::scope<std::lock_guard<std::mutex>>(_m, [&] { _ostream << std::move(s); });
    }
};

/****************************************************************************************************/

template <typename T, typename U>
detail::helper operator<<(tsos<T>& s, U&& x) {
    return detail::helper([&_s = s](std::string str) { _s.commit(std::move(str)); })
           << std::forward<U>(x);
}

/****************************************************************************************************/
// BINSPECTOR_OSTREAM_HPP
#endif

/****************************************************************************************************/
