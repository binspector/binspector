/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

#ifndef BINSPECTOR_BITREADER_HPP
#define BINSPECTOR_BITREADER_HPP

// stdc++
#include <istream>
#include <stdexcept>
#include <vector>

// boost
#include <boost/cstdint.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/operators.hpp>

// asl
#include <adobe/string.hpp>

/****************************************************************************************************/

typedef std::vector<boost::uint8_t> rawbytes_t;

/****************************************************************************************************/

inline boost::uint64_t bytesize(boost::uint64_t bits)
{
    return bits >> 3; // bits / 8
}

inline boost::uint8_t bitsize(boost::uint64_t bits)
{
    return bits & 7; // zero out all but first 3 bits (bits % 8)
}

/****************************************************************************************************/

struct bitreader_t
{
    /*
        The limitation here is that though the byte offsets are stored as an unsigned 64 bit integer
        its value can only be manipulated with a signed 64 bit integer (to account for the
        possibility of being given negative offsets.) We may want to revisit this issue later.
    */
    struct pos_t : boost::totally_ordered<pos_t>,
                   boost::additive<pos_t>
    {
        class invalid_k { };
    
        pos_t() :
            byte_offset_m(0),
            bit_offset_m(0)
        { }
    
        // We don't want to make this ctor take default parameters to avoid confusing bytes/bits
        pos_t(boost::uint64_t bytes, boost::uint64_t bits) :
            byte_offset_m(bytes + bytesize(bits)),
            bit_offset_m(bitsize(bits))
        { }
    
        explicit pos_t(invalid_k) :
            byte_offset_m(-1),
            bit_offset_m(0)
        { }
    
        bool byte_aligned() const
        {
            // was bit_offset_m == 0 || bitsize(bit_offset_m) == 0,
            // but then realized the latter case would be true for the first case also, so...
            return bitsize(bit_offset_m) == 0;
        }
    
        pos_t& operator+=(const pos_t& value)
        {
            byte_offset_m += value.byte_offset_m;
            bit_offset_m += value.bit_offset_m;

            // handles cases where we now have >= 8 bits in the bit offset.
            byte_offset_m += bytesize(bit_offset_m);
            bit_offset_m = bitsize(bit_offset_m);

            return *this;
        }
    
        pos_t& operator-=(const pos_t& value)
        {
            if (bit_offset_m < value.bit_offset_m)
            {
                --byte_offset_m;
                bit_offset_m += 8;
            }

            byte_offset_m -= value.byte_offset_m;
            bit_offset_m -= value.bit_offset_m;
    
            return *this;
        }

        boost::uint64_t bytes() const { return byte_offset_m; }
        boost::uint8_t  bits() const { return bit_offset_m; }
    
        friend bool operator==(const pos_t& x, const pos_t& y)
        {
            return x.byte_offset_m == y.byte_offset_m && x.bit_offset_m == y.bit_offset_m;
        }
    
        friend bool operator<(const pos_t& x, const pos_t& y)
        {
            // we know the type so can use operator== here
            return x.byte_offset_m < y.byte_offset_m ||
                   (x.byte_offset_m == y.byte_offset_m && x.bit_offset_m < y.bit_offset_m);
        }

        friend std::string& operator<<(std::string& s, const pos_t& x)
        {
            s += boost::lexical_cast<std::string>(x.byte_offset_m);
    
            if (!x.byte_aligned())
            {
                s += ".";
                s += boost::lexical_cast<std::string>(static_cast<int>(x.bit_offset_m));
            }
    
            return s;
        }

        friend std::string serialize(const pos_t& x)
        {
            std::string result;

            result << x;

            return result;
        }

        friend std::ostream& operator<<(std::ostream& s, const pos_t& x)
        {
            return s << serialize(x);
        }

    private:
        boost::uint64_t byte_offset_m; // byte offset in file
        boost::uint8_t  bit_offset_m;  // sub-byte offset in file
    };

    explicit bitreader_t(std::istream& input);

    void  seek(const pos_t& position);    // absolute
    pos_t advance(const pos_t& position); // relative; returns old position

    bool eof() const;
    bool fail() const;
    void clear();

    boost::uint8_t peek(); // peek the next byte

    pos_t size() const { return size_m; }
    pos_t pos() const { return position_m; }

    // The failbit/eofbit state will throw a std::out_of_range. (i.e., reading past eof)
    // All other failbit states will throw a std::runtime_error.
    // In either case the stream will not be cleared of its failbit state.
    rawbytes_t read_bits(boost::uint64_t bits);
    rawbytes_t read_bits(const pos_t& position, boost::uint64_t bits)
        { seek(position); return read_bits(bits); }

    rawbytes_t read(boost::uint64_t bytes)
        { return read_bits(bytes << 3); }
    rawbytes_t read(const pos_t& position, boost::uint64_t bytes)
        { return read_bits(position, bytes << 3); }
    rawbytes_t read(const pos_t& start_position, const pos_t& end_position)
        { return read(start_position, (end_position - start_position + bitreader_t::pos_t(1, 0)).bytes()); }

private:
    void lazy_seek();

    std::istream&   input_m;
    pos_t           size_m;
    pos_t           position_m;
    bool            saught_m;
    boost::uint8_t  remainder_bits_m; // leftover bits from the last read
    boost::uint8_t  remainder_size_m; // number of valid bits in the above (lowest)
};

inline bitreader_t::pos_t bytepos(boost::uint64_t bytes)
{
    return bitreader_t::pos_t(bytes, 0);
}

inline bitreader_t::pos_t bitpos(boost::uint64_t bits)
{
    return bitreader_t::pos_t(0, bits);
}

static const bitreader_t::pos_t invalid_position_k = bitreader_t::pos_t(bitreader_t::pos_t::invalid_k());
static const bitreader_t::pos_t inspection_byte_k = bytepos(1);
static const bitreader_t::pos_t inspection_bit_k = bitpos(1);

typedef bitreader_t::pos_t inspection_position_t; // belch

/****************************************************************************************************/

struct restore_point_t
{
    explicit restore_point_t(bitreader_t& input) :
        input_m(input),
        pos_m(input_m.pos())
    { }

    ~restore_point_t()
    {
        input_m.seek(pos_m);
    }

private:
    bitreader_t&       input_m;
    bitreader_t::pos_t pos_m;
};

/****************************************************************************************************/
// BINSPECTOR_BITREADER_HPP
#endif

/****************************************************************************************************/
