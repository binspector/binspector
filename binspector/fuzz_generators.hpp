/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

#ifndef BINSPECTOR_FUZZ_GENERATORS_HPP
#define BINSPECTOR_FUZZ_GENERATORS_HPP

// stdc+++
#include <string>
#include <memory>

// asl
#include <adobe/any_regular.hpp>

// application
#include <binspector/bitreader.hpp>
#include <binspector/forest.hpp>

/****************************************************************************************************/

class fuzz_generator_t {
    struct concept {
        virtual ~concept() = default;
        virtual std::string identifier() const = 0;
        virtual std::size_t bit_count() const = 0;
        virtual rawbytes_t operator()() const = 0;
    };

    template <typename G>
    struct model : public concept {
        virtual ~model() = default;

        explicit model(G&& g) : _g(std::forward<G>(g)) { }

        std::string identifier() const override { return _g.identifier(); }
        std::size_t bit_count() const override { return _g.bit_count(); }
        rawbytes_t operator()() const override { return _g(); }

        G _g;
    };

    std::unique_ptr<concept> _ptr;

public:
    fuzz_generator_t() = default;

    template <typename G>
    fuzz_generator_t(G&& g) : _ptr(std::make_unique<model<G>>(std::forward<G>(g))) { }

    std::string identifier() const {
        return _ptr->identifier();
    }

    std::size_t bit_count() const {
        return _ptr->bit_count();
    }

    rawbytes_t operator()() const {
        return (*_ptr)();
    }
};

/****************************************************************************************************/

fuzz_generator_t make_zero_generator(std::size_t bit_count);

/****************************************************************************************************/

fuzz_generator_t make_ones_generator(std::size_t bit_count);

/****************************************************************************************************/

fuzz_generator_t make_less_generator(node_t node, rawbytes_t raw);

/****************************************************************************************************/

fuzz_generator_t make_more_generator(node_t node, rawbytes_t raw);

/****************************************************************************************************/

fuzz_generator_t make_rand_generator(std::size_t bit_count);

/****************************************************************************************************/

fuzz_generator_t make_enum_generator(const adobe::any_regular_t& value,
                                     atom_base_type_t            base_type,
                                     bool                        is_big_endian,
                                     std::size_t                 bit_count);

/****************************************************************************************************/
// BINSPECTOR_FUZZ_GENERATORS_HPP
#endif

/****************************************************************************************************/
