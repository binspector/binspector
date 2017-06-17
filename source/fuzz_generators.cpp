/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// identity
#include <binspector/fuzz_generators.hpp>

// stdc
// #include <sys/stat.h>

// stdc++
// #include <iostream>
// #include <fstream>
// #include <memory>
// #include <random>
// #include <set>
#include <random>

// boost
// #include <boost/filesystem.hpp>
// #include <boost/filesystem/fstream.hpp>
// #include <boost/lexical_cast.hpp>
// #include <boost/optional.hpp>
// #include <boost/version.hpp>

// asl
// #include <adobe/string.hpp>
// #include <adobe/iomanip_asl_cel.hpp>
// #include <adobe/fnv.hpp>

// application
// #include <binspector/analyzer.hpp>
// #include <binspector/parser.hpp>
#include <binspector/common.hpp>
#include <binspector/error.hpp>

/****************************************************************************************************/

namespace {

/****************************************************************************************************/

template <typename T>
inline rawbytes_t raw_disintegration(const T& value)
{
    const char* rawp(reinterpret_cast<const char*>(&value));

    return rawbytes_t(rawp, rawp + sizeof(value));
}

/****************************************************************************************************/

class zero_generator_t
{
    std::size_t bit_count_m;

public:
    explicit zero_generator_t(std::size_t bit_count) : bit_count_m(bit_count)
    { }

    std::string identifier() const { return "z"; }

    std::size_t bit_count() const { return bit_count_m; }

    rawbytes_t operator()() const
    {
        return rawbytes_t(bit_count_m / 8, 0);
    }
};

/****************************************************************************************************/

class ones_generator_t
{
    std::size_t bit_count_m;

public:
    explicit ones_generator_t(std::size_t bit_count) : bit_count_m(bit_count)
    { }

    std::string identifier() const { return "o"; }

    std::size_t bit_count() const { return bit_count_m; }

    rawbytes_t operator()() const
    {
        return rawbytes_t(bit_count_m / 8, 0xff);
    }
};

/****************************************************************************************************/

class rand_generator_t
{
    std::size_t   bit_count_m;
    std::uint64_t v_m;

    std::uint64_t rnd_v() {
        static std::mt19937                                 gen{std::random_device()()};
        static std::uniform_int_distribution<std::uint64_t> dist(0, std::numeric_limits<std::uint64_t>::max());

        return dist(gen);
    }

public:
    explicit rand_generator_t(std::size_t bit_count) :
        bit_count_m(bit_count),
        v_m(rnd_v())
    { }

    std::string identifier() const { return "r_" + std::to_string(v_m >> (64 - bit_count_m)); }

    std::size_t bit_count() const { return bit_count_m; }

    rawbytes_t operator()() const
    {
        auto        all_raw(raw_disintegration<>(v_m));
        std::size_t n(bit_count_m / 8);
#if __LITTLE_ENDIAN__ || defined(_M_IX86) || defined(_WIN32)
        auto        first{std::rbegin(all_raw)};
#endif
#if __BIG_ENDIAN__
        auto        first{std::begin(all_raw)};
#endif
        rawbytes_t  result(first, first + n);

        return result;
    }
};

/****************************************************************************************************/

rawbytes_t disintegrate_value(double           value,
                              boost::uint64_t  bit_count,
                              atom_base_type_t base_type,
                              bool             is_big_endian)
{
    rawbytes_t result;

    if (base_type == atom_unknown_k)
    {
        throw std::runtime_error("disintegrate_value: unknown atom base type");
    }
    else if (base_type == atom_float_k)
    {
        BOOST_STATIC_ASSERT((sizeof(float) == 4));
        BOOST_STATIC_ASSERT((sizeof(double) == 8));

        if (bit_count == (sizeof(float) * 8))
            result = raw_disintegration<float>(value);
        else if (bit_count == (sizeof(double) * 8))
            result = raw_disintegration<double>(value);
        else
            throw std::runtime_error("disintegrate_value: float atom of specified bit count not supported.");
    }
    else if (bit_count <= 8)
    {
        if (base_type == atom_signed_k)
            result = raw_disintegration<boost::int8_t>(value);
        else if (base_type == atom_unsigned_k)
            result = raw_disintegration<boost::uint8_t>(value);
    }
    else if (bit_count <= 16)
    {
        if (base_type == atom_signed_k)
            result = raw_disintegration<boost::int16_t>(value);
        else if (base_type == atom_unsigned_k)
            result = raw_disintegration<boost::uint16_t>(value);
    }
    else if (bit_count <= 32)
    {
        if (base_type == atom_signed_k)
            result = raw_disintegration<boost::int32_t>(value);
        else if (base_type == atom_unsigned_k)
            result = raw_disintegration<boost::uint32_t>(value);
    }
    else if (bit_count <= 64)
    {
        if (base_type == atom_signed_k)
            result = raw_disintegration<boost::int64_t>(value);
        else if (base_type == atom_unsigned_k)
            result = raw_disintegration<boost::uint64_t>(value);
    }
    else
    {
        throw std::runtime_error("disintegrate_value: invalid bit count");
    }

    // The second step is to do any necessary endian-swapping of the raw byte sequence.

#if __LITTLE_ENDIAN__ || defined(_M_IX86) || defined(_WIN32)
    if (is_big_endian)
        std::reverse(result.begin(), result.end());
#endif
#if __BIG_ENDIAN__
    if (!is_big_endian)
        std::reverse(result.begin(), result.end());
#endif

    return result;
}

/****************************************************************************************************/

struct enum_generator_t
{
    explicit enum_generator_t(double           value,
                              atom_base_type_t base_type,
                              bool             is_big_endian,
                              std::size_t      bit_count) :
        identifier_m("e_" + to_string_fmt(value, "%.0f")),
        disintegrated_m(disintegrate_value(value, bit_count, base_type, is_big_endian))
    { }

    std::string identifier() const
    {
        return identifier_m;
    }

    std::uint64_t bit_count() const { return disintegrated_m.size() * 8; }

    rawbytes_t operator()() const
    {
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

fuzz_generator_t make_rand_generator(std::size_t bit_count) {
    return fuzz_generator_t{rand_generator_t(bit_count)};
}

/****************************************************************************************************/

fuzz_generator_t make_enum_generator(const adobe::any_regular_t& value,
                                     atom_base_type_t            base_type,
                                     bool                        is_big_endian,
                                     std::size_t                 bit_count) {
    return fuzz_generator_t{enum_generator_t(value.cast<double>(),
                                             base_type,
                                             is_big_endian,
                                             bit_count)};
}

/****************************************************************************************************/
