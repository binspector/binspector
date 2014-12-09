/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

// identity
#include <binspector/bitreader.hpp>

// stc++
#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>

/**************************************************************************************************/

namespace {

/**************************************************************************************************/
// returns a mask for the N lowest bits of the byte.
inline boost::uint8_t mask_for_low_bits(boost::uint8_t numbits)
{
    assert(numbits <= 8);

    boost::uint8_t result(1 << numbits);

    return result - 1;
}

/**************************************************************************************************/
// returns a mask for the N highest bits of the byte.
inline boost::uint8_t mask_for_high_bits(boost::uint8_t numbits)
{
    assert(numbits <= 8);

    return ~mask_for_low_bits(8 - numbits);
}

/**************************************************************************************************/

inline boost::uint8_t mask_for_bits(boost::uint8_t start_bit, boost::uint8_t num_bits)
{
    return mask_for_high_bits(num_bits) >> start_bit;
}

/**************************************************************************************************/

inline boost::uint8_t split_byte(boost::uint8_t& src,
                                 boost::uint8_t& dst,
                                 boost::uint8_t  start_bit,
                                 boost::uint8_t  num_bits)
{
    boost::uint8_t byte_mask(mask_for_bits(start_bit, num_bits));
    boost::uint8_t dst_size(8 - (start_bit + num_bits));

    dst = src & ~byte_mask;
    src &= byte_mask;
    src >>= dst_size;

    return dst_size;
}

/**************************************************************************************************/
#if 0
void merge_byte(boost::uint8_t  src,
                boost::uint8_t  src_start_bit,
                boost::uint8_t& dst,
                boost::uint8_t  dst_start_bit,
                boost::uint8_t  num_bits)
{
    // Copies num_bits of src starting at src_start_bit into dst starting at dst_start_bit.
    // Note: If src_start_bit == dst_start_bit then it's a straight-up mask and merge.
}
#endif
/**************************************************************************************************/

} // namespace

/**************************************************************************************************/
#if 0
#pragma mark -
#endif
/**************************************************************************************************/

bitreader_t::bitreader_t(std::istream& input) :
    input_m(input),
    saught_m(false),
    remainder_bits_m(0),
    remainder_size_m(0)
{
    input_m.clear();

    input.seekg(0, std::ios::end);

    size_m = bytepos(input.tellg());

    input.seekg(0);

    //if (size_m == invalid_position_k)
    //    throw std::runtime_error("bitreader_t: failed to get input file size");
}

/**************************************************************************************************/

void bitreader_t::seek(const pos_t& position)
{
    if (position_m == position)
        return;

    position_m = position;

    saught_m = true;
}

/**************************************************************************************************/

bitreader_t::pos_t bitreader_t::advance(const pos_t& position)
{
    if (position == pos_t())
        return position_m;

    // If we're at or past the end of the file and try to advance again, we're done.
    if (eof())
        throw std::out_of_range("bitreader_t::advance: end of file");

    pos_t result(position_m);

    position_m += position;

    saught_m = true;

    return result;
}

/**************************************************************************************************/

bool bitreader_t::eof() const
{
    return position_m >= size_m;
}

/**************************************************************************************************/

bool bitreader_t::fail() const
{
    return input_m.fail();
}

/**************************************************************************************************/

void bitreader_t::clear()
{
    return input_m.clear();
}

/**************************************************************************************************/

boost::uint8_t bitreader_t::peek()
{
    lazy_seek();

    // If we're at or past the end of the file and try to advance again, we're done.
    if (eof())
        throw std::out_of_range("bitreader_t::peek: end of file");

    return input_m.peek();
}

/**************************************************************************************************/

rawbytes_t bitreader_t::read_bits(boost::uint64_t inbits)
{
    lazy_seek();

    boost::uint64_t read_bytes(bytesize(inbits));
    boost::uint8_t  read_bits(bitsize(inbits));
    bool            byte_aligned_read(read_bits == 0);

    if (read_bytes == 0 && read_bits == 0)
        return rawbytes_t();

    /*
        A viable case is where we want to read bits completely present in the
        remainder_bits_m. Handle that here.
    */
    if (read_bytes == 0)
    {
        if (read_bits == remainder_size_m)
        {
            // A common case where the remainder bits make up a field.

            rawbytes_t result(1, remainder_bits_m & mask_for_low_bits(read_bits));

            remainder_size_m = 0;

            position_m += bitpos(inbits);

            return result;
        }
        else if (read_bits < remainder_size_m)
        {
            // still some bits left over.
            boost::uint8_t diff_size = remainder_size_m - read_bits;

            rawbytes_t result(1, remainder_bits_m >> diff_size);

            remainder_bits_m &= mask_for_low_bits(diff_size);
            remainder_size_m = diff_size;

            position_m += bitpos(inbits);

            return result;
        }
    }

    // Here we are reading X bytes and Y bits, where X*8+Y bits > remainder_bits_m

    std::size_t result_size(static_cast<std::size_t>(byte_aligned_read ?
                                                         read_bytes :
                                                         read_bytes + 1));
    rawbytes_t  result(result_size, 0);

    if (result_size == 0)
        return result; // ask for no bits - you got 'em!

    if (remainder_size_m == 0)
    {
        input_m.read(reinterpret_cast<char*>(&result[0]), result_size);

        /*
            Given there is no remainder, if the read is to be byte-aligned
            then we're done here. If the read is non-byte-aligned then we
            split the last byte's bits between the remainder and the output.
        */
        if (!byte_aligned_read)
        {
            // mask off last byte and set remainder
            remainder_size_m = split_byte(result.back(),
                                          remainder_bits_m,
                                          position_m.bits(),
                                          read_bits);
        }
    }
    else
    {
        /*
        Here we are reading X.Y bytes with Z bits of remainder.

        Preconditions to get here:
            - X is usually nonzero, but can be zero if Y > Z.
            - Y can be zero or nonzero.
            - Z must be nonzero. (Otherwise handled by the above if clause)

        As such we cannot copy the file data as-is, but have to shift everything from the raw read
        down by Z bits and slap the initial remainder in the front of the result.

        Pseudocode:

        for (int N = 0 to result_size)
        {
            // Copy current remainder into high bits of current result byte:
            //     result[N.7..8-Z] = remainder[Z-1..0];
            copy_bits(remainder, Z-1, result[N], 7, Z);

            // Copy 8-Z raw bits into current location in result byte:
            //     result[N.7-Z..0] = raw[N.7..Z];
            copy_bits(raw[N], 7, result[N], 7-Z, 8-Z);

            // Copy remainder of current raw byte into remainder for next iteration:
            //     remainder[Z-1..0] = raw[Z-1..0];
            copy_bits(raw[N], Z-1, remainder[N], Z-1, Z);
        }

        ... and something at the end will get tacked on when Y != Z.

        If I treated the result with a 16-bit window, then the code would look like:
        
        result[0.15..16-Z] = remainder[Z-1..0];

        for (int N = 0 to X) // note: not result_size!
        {
            result[N.15-Z..15-Z-8] = raw[N];
            // note that though we have a 16 bit window we're iterating 8 bits at a time...
            // This allows the whole-hog copy of raw[N] to span over two bytes in a single
            // operation.
            // (Would there be an endianness issue with this method?)
        }

        ... and something at the end will get tacked on when Y != Z.

        // tack on the difference from Y to Z here:
        */

        throw std::logic_error("Asking for more than the remainder present; tell fbrereto to implement this!");

        // here we have a remainder that will require all our bits get shifted accordingly.
        rawbytes_t raw(result_size, 0);

        input_m.read(reinterpret_cast<char*>(&result[0]), result_size);
    }

    position_m += bitpos(inbits);

    // a failure here can be EOF or something else; test for both cases and respond accordingly.
    if (input_m.fail())
    {
        if (input_m.eof())
        {
            throw std::out_of_range("bitreader_t: end of file");
        }
        else
        {
            std::stringstream error;
            std::streamoff    offset(input_m.tellg());
            error << "Input stream fail; offset: " << offset << ", size: " << bitpos(inbits);
            throw std::runtime_error(error.str());
        }
    }

    return result;
}

/**************************************************************************************************/

void bitreader_t::lazy_seek()
{
    if (!saught_m)
        return;

    input_m.seekg(static_cast<std::streamoff>(position_m.bytes()));

    remainder_size_m = 0;

    saught_m = false;
}

/**************************************************************************************************/
