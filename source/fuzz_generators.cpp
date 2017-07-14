/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// identity
#include <binspector/fuzz_generators.hpp>

// stdc++
#include <random>

// boost

// asl

// application
#include <binspector/common.hpp>
#include <binspector/endian.hpp>
#include <binspector/error.hpp>

/****************************************************************************************************/

namespace {

/****************************************************************************************************/

template <typename T>
inline rawbytes_t decompose(const T& value) {
    const char* rawp(reinterpret_cast<const char*>(&value));

    return rawbytes_t(rawp, rawp + sizeof(value));
}

/****************************************************************************************************/

rawbytes_t decompose(double           value,
                     boost::uint64_t  bit_count,
                     atom_base_type_t base_type,
                     bool             is_big_endian) {
    rawbytes_t result;

    if (base_type == atom_unknown_k) {
        throw std::runtime_error("decompose: unknown atom base type");
    } else if (base_type == atom_float_k) {
        BOOST_STATIC_ASSERT((sizeof(float) == 4));
        BOOST_STATIC_ASSERT((sizeof(double) == 8));

        if (bit_count == (sizeof(float) * 8))
            result = decompose<float>(value);
        else if (bit_count == (sizeof(double) * 8))
            result = decompose<double>(value);
        else
            throw std::runtime_error("decompose: float atom of specified bit count not supported.");
    } else if (bit_count <= 8) {
        if (base_type == atom_signed_k)
            result = decompose<boost::int8_t>(value);
        else if (base_type == atom_unsigned_k)
            result = decompose<boost::uint8_t>(value);
    } else if (bit_count <= 16) {
        if (base_type == atom_signed_k)
            result = decompose<boost::int16_t>(value);
        else if (base_type == atom_unsigned_k)
            result = decompose<boost::uint16_t>(value);
    } else if (bit_count <= 32) {
        if (base_type == atom_signed_k)
            result = decompose<boost::int32_t>(value);
        else if (base_type == atom_unsigned_k)
            result = decompose<boost::uint32_t>(value);
    } else if (bit_count <= 64) {
        if (base_type == atom_signed_k)
            result = decompose<boost::int64_t>(value);
        else if (base_type == atom_unsigned_k)
            result = decompose<boost::uint64_t>(value);
    } else {
        throw std::runtime_error("decompose: invalid bit count");
    }

    // The second step is to do any necessary endian-swapping of the raw byte sequence.
    host_to_endian(result, is_big_endian);

    return result;
}

/****************************************************************************************************/

class zero_generator_t {
    std::size_t bit_count_m;

public:
    explicit zero_generator_t(std::size_t bit_count) : bit_count_m(bit_count) {}

    std::string identifier() const {
        return "z";
    }

    std::size_t bit_count() const {
        return bit_count_m;
    }

    rawbytes_t operator()() const {
        return rawbytes_t(bit_count_m / 8, 0);
    }
};

/****************************************************************************************************/

class ones_generator_t {
    std::size_t bit_count_m;

public:
    explicit ones_generator_t(std::size_t bit_count) : bit_count_m(bit_count) {}

    std::string identifier() const {
        return "o";
    }

    std::size_t bit_count() const {
        return bit_count_m;
    }

    rawbytes_t operator()() const {
        return rawbytes_t(bit_count_m / 8, 0xff);
    }
};

/****************************************************************************************************/

std::uint64_t synthesize(const rawbytes_t& raw,
                         boost::uint64_t   bit_count,
                         atom_base_type_t  base_type,
                         bool              is_big_endian) {
    rawbytes_t byte_set(raw);

    host_to_endian(byte_set, is_big_endian);

    auto p(&byte_set[0]);

    if (bit_count <= 8) {
        if (base_type == atom_signed_k)
            return *reinterpret_cast<const boost::int8_t*>(p);

        return *reinterpret_cast<const boost::uint8_t*>(p);
    }

    if (bit_count <= 16) {
        if (base_type == atom_signed_k)
            return *reinterpret_cast<const boost::int16_t*>(p);

        return *reinterpret_cast<const boost::uint16_t*>(p);
    }

    if (bit_count <= 32) {
        if (base_type == atom_signed_k)
            return *reinterpret_cast<const boost::int32_t*>(p);

        return *reinterpret_cast<const boost::uint32_t*>(p);
    }

    if (bit_count <= 64) {
        if (base_type == atom_signed_k)
            return *reinterpret_cast<const boost::int64_t*>(p);

        return *reinterpret_cast<const boost::uint64_t*>(p);
    }

    throw std::runtime_error("synthesize: invalid bit count");
}

/****************************************************************************************************/

class less_generator_t {
    node_t     node_m;
    rawbytes_t raw_m;

public:
    less_generator_t(node_t node, rawbytes_t raw)
        : node_m(std::move(node)), raw_m(std::move(raw)) {}

    std::string identifier() const {
        return "l";
    }

    std::size_t bit_count() const {
        return node_m.bit_count_m;
    }

    rawbytes_t operator()() const {
        auto          bit_count = node_m.bit_count_m;
        auto          type = node_m.type_m;
        auto          big_endian = node_m.get_flag(atom_is_big_endian_k);
        std::uint64_t x = synthesize(raw_m, bit_count, type, big_endian);

        --x;

        return decompose(x, bit_count, type, big_endian);
    }
};

/****************************************************************************************************/

class more_generator_t {
    node_t     node_m;
    rawbytes_t raw_m;

public:
    more_generator_t(node_t node, rawbytes_t raw)
        : node_m(std::move(node)), raw_m(std::move(raw)) {}

    std::string identifier() const {
        return "m";
    }

    std::size_t bit_count() const {
        return node_m.bit_count_m;
    }

    rawbytes_t operator()() const {
        auto          bit_count = node_m.bit_count_m;
        auto          type = node_m.type_m;
        auto          big_endian = node_m.get_flag(atom_is_big_endian_k);
        std::uint64_t x = synthesize(raw_m, bit_count, type, big_endian);

        ++x;

        return decompose(x, bit_count, type, big_endian);
    }
};

/****************************************************************************************************/

class rand_generator_t {
    std::size_t   bit_count_m;
    std::uint64_t v_m;

    std::uint64_t rnd_v() {
        static thread_local std::mt19937 gen{std::random_device()()};
        static thread_local std::uniform_int_distribution<std::uint64_t> dist(
            0, std::numeric_limits<std::uint64_t>::max());

        return dist(gen);
    }

public:
    explicit rand_generator_t(std::size_t bit_count) : bit_count_m(bit_count), v_m(rnd_v()) {}

    std::string identifier() const {
        return "r_" + std::to_string(v_m >> (64 - bit_count_m));
    }

    std::size_t bit_count() const {
        return bit_count_m;
    }

    rawbytes_t operator()() const {
        auto        all_raw(decompose(v_m));
        std::size_t n(bit_count_m / 8);
#if BINSPECTOR_ENDIAN_LITTLE
        auto first{std::rbegin(all_raw)};
#else
        auto first{std::begin(all_raw)};
#endif

        auto last{first};

        while (n--)
            ++last;

        rawbytes_t result(first, last);

        return result;
    }
};

/****************************************************************************************************/

struct enum_generator_t {
    explicit enum_generator_t(double           value,
                              atom_base_type_t base_type,
                              bool             is_big_endian,
                              std::size_t      bit_count)
        : identifier_m("e_" + to_string_fmt(value, "%.0f")),
          disintegrated_m(decompose(value, bit_count, base_type, is_big_endian)) {}

    std::string identifier() const {
        return identifier_m;
    }

    std::uint64_t bit_count() const {
        return disintegrated_m.size() * 8;
    }

    rawbytes_t operator()() const {
        return disintegrated_m;
    }

private:
    std::string identifier_m;
    rawbytes_t  disintegrated_m;
};

/****************************************************************************************************/

} // namespace

/****************************************************************************************************/

fuzz_generator_t make_zero_generator(std::size_t bit_count) {
    return fuzz_generator_t{zero_generator_t(bit_count)};
}

/****************************************************************************************************/

fuzz_generator_t make_ones_generator(std::size_t bit_count) {
    return fuzz_generator_t{ones_generator_t(bit_count)};
}

/****************************************************************************************************/

fuzz_generator_t make_less_generator(node_t node, rawbytes_t raw) {
    return fuzz_generator_t{less_generator_t(std::move(node), std::move(raw))};
}

/****************************************************************************************************/

fuzz_generator_t make_more_generator(node_t node, rawbytes_t raw) {
    return fuzz_generator_t{more_generator_t(std::move(node), std::move(raw))};
}

/****************************************************************************************************/

fuzz_generator_t make_rand_generator(std::size_t bit_count) {
    return fuzz_generator_t{rand_generator_t(bit_count)};
}

/****************************************************************************************************/

fuzz_generator_t make_enum_generator(const adobe::any_regular_t& value,
                                     atom_base_type_t            base_type,
                                     bool                        is_big_endian,
                                     std::size_t                 bit_count) {
    return fuzz_generator_t{
        enum_generator_t(value.cast<double>(), base_type, is_big_endian, bit_count)};
}

/****************************************************************************************************/
