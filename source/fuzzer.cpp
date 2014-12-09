/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

// stdc
#include <sys/stat.h>

// stdc++
#include <iostream>
#include <fstream>
#include <set>

// boost
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
//#include <boost/timer/timer.hpp>
#include <boost/version.hpp>

// asl
#include <adobe/string.hpp>
#include <adobe/iomanip_asl_cel.hpp>

// application
#include <binspector/analyzer.hpp>
#include <binspector/parser.hpp>

/**************************************************************************************************/

namespace {

/**************************************************************************************************/

struct zero_generator_t
{
    static const std::string& identifier()
    {
        static const std::string value_s("z");

        return value_s;
    }

    rawbytes_t operator()(std::size_t bit_count)
    {
        std::size_t bytecount(bit_count / 8);

        if (bit_count % 8 != 0)
            ++bytecount;

        return rawbytes_t(bytecount, 0);
    }
};

/**************************************************************************************************/

struct ones_generator_t
{
    static const std::string& identifier()
    {
        static const std::string value_s("o");

        return value_s;
    }

    rawbytes_t operator()(std::size_t bit_count)
    {
        std::size_t bytecount(bit_count / 8);

        if (bit_count % 8 != 0)
            ++bytecount;

        return rawbytes_t(bytecount, 0xff);
    }
};

/**************************************************************************************************/

template <typename T>
inline rawbytes_t raw_disintegration(const T& value)
{
    const char* rawp(reinterpret_cast<const char*>(&value));

    return rawbytes_t(rawp, rawp + sizeof(value));
}

/**************************************************************************************************/

rawbytes_t disintegrate_value(const adobe::any_regular_t& regular_value,
                              boost::uint64_t             bit_count,
                              atom_base_type_t            base_type,
                              bool                        is_big_endian)
{
    rawbytes_t result;

    // The first step is to convert the POD to a raw byte sequence.

    double value(regular_value.cast<double>());

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

/**************************************************************************************************/

struct enumerated_generator_t
{
    explicit enumerated_generator_t(const adobe::any_regular_t& value,
                                    atom_base_type_t            base_type,
                                    bool                        is_big_endian) :
        value_m(value),
        base_type_m(base_type),
        is_big_endian_m(is_big_endian)
    { }

    std::string identifier()
    {
        std::stringstream stream;

        stream << "e_" << adobe::begin_asl_cel << value_m << adobe::end_asl_cel;

        return stream.str();
    }

    rawbytes_t operator()(std::size_t bit_count)
    {
        return disintegrate_value(value_m,
                                  bit_count,
                                  base_type_m,
                                  is_big_endian_m);
    }

private:
    adobe::any_regular_t value_m;
    atom_base_type_t     base_type_m;
    bool                 is_big_endian_m;
};

/**************************************************************************************************/
// might want to make this into a standalone struct that can be owned by the fuzzer, so the
// set isn't shared across multiple fuzzes (if we ever get there.)
bool validate_unique_outfile(const boost::filesystem::path& file,
                             std::ofstream&                 output)
{
    static std::set<boost::filesystem::path> file_set;

    if (file_set.find(file) != file_set.end())
    {
        // oops. Don't want to redo an file we've already made.
        // Figure out why you got here and fix it, yo!

        output << "    ! Internal error: duplicate file: " << file.c_str() << '\n';

        return false;
    }

    // Track this file location to make sure it's unique across the files generated.
    file_set.insert(file);

    return true;
};

/**************************************************************************************************/

bool duplicate_file(const boost::filesystem::path& src,
                    const boost::filesystem::path& dst,
                    std::ofstream&                 output)
{
    if (!validate_unique_outfile(dst, output))
        return false;

    if (exists(dst) && !remove(dst))
    {
        output << "    ! Could not replace " << dst.c_str() << '\n';

        return false;
    }

    copy_file(src, dst);

    // gotta set the right permissions; we probably need a chmod equivalent
    // on windows before this will function over there.

#if BOOST_WINDOWS
    // necessary for windows?
#else
    chmod(dst.c_str(), S_IRUSR | S_IWUSR);
#endif

    return true;
}

/**************************************************************************************************/

inline void rawbytes_out(std::ofstream& output, const rawbytes_t& data)
{
    if (data.empty())
        return;

    output.write(reinterpret_cast<const char*>(&data[0]), data.size());
}

/**************************************************************************************************/

struct fuzzer_t
{
    fuzzer_t(const boost::filesystem::path& input_path,
             const boost::filesystem::path& output_root);

    void fuzz(const inspection_forest_t& forest);

private:
    typedef std::vector<const_inspection_branch_t> node_set_t;

    template <typename FuzzGenerator>
    std::size_t fuzz_location_with(const inspection_position_t& location,
                                   boost::uint64_t              bit_count,
                                   FuzzGenerator                generator);

    std::size_t fuzz_location_with_enumeration(const inspection_position_t& location,
                                               boost::uint64_t              bit_count,
                                               const attack_vector_t&       entry);

    std::size_t fuzz_shuffle_set_with(const node_set_t& node_set);

    std::size_t fuzz_shuffle_entry(const attack_vector_t& entry);

    std::size_t fuzz_usage_entry(const attack_vector_t& entry);

    boost::filesystem::path     input_path_m;
    boost::filesystem::path     base_output_path_m;
    std::string                 basename_m;
    std::string                 extension_m;
    boost::filesystem::ofstream output_m; // summary output
};

/**************************************************************************************************/

template <typename FuzzGenerator>
std::size_t fuzzer_t::fuzz_location_with(const inspection_position_t& location,
                                         boost::uint64_t              bit_count,
                                         FuzzGenerator                generator)
{
    if (bit_count % 8 != 0 || !location.byte_aligned())
    {
        output_m << "    ! Bit-level/byte-unaligned fuzzing is not yet supported. Location skipped.\n";

        return 0;
    }

    std::string cur_name(basename_m);
    
    // REVISIT (fbrereto) : String concatenation here.
    cur_name += "_" + serialize(location) + "_" + generator.identifier() + extension_m;

    boost::filesystem::path output_path(base_output_path_m / cur_name);

    if (!duplicate_file(input_path_m, output_path, output_m))
        return 0;

    boost::filesystem::fstream fuzzed(output_path, std::ios::binary | std::ios::in | std::ios::out);

    // std::cerr << canonical(output_path).string() << '\n';

    if (!fuzzed)
        {
        perror("!");

        throw std::runtime_error("Could not open file for fuzzed output.");
        }

    std::fstream::pos_type pos(location.bytes());

    fuzzed.seekp(pos);

    std::size_t byte_count(bit_count / 8);
    rawbytes_t  fuzz_data(generator(bit_count));

    fuzzed.write(reinterpret_cast<const char*>(&fuzz_data[0]), byte_count);

    output_m << "    > " << cur_name << '\n';

    return 1;
}

/**************************************************************************************************/

std::size_t fuzzer_t::fuzz_location_with_enumeration(const inspection_position_t& location,
                                                     boost::uint64_t              bit_count,
                                                     const attack_vector_t&       entry)
{
    if (entry.node_m.get_flag(is_array_element_k))
        std::cerr << "WARNING: fuzzing an array entry; the forest may not be set up properly.\n";

    const adobe::array_t& enumerated_option_set(entry.node_m.option_set_m);
    std::size_t           result(0);

    for (adobe::array_t::const_iterator iter(enumerated_option_set.begin()), last(enumerated_option_set.end()); iter != last; ++iter)
        result += fuzz_location_with(location,
                                     bit_count,
                                     enumerated_generator_t(*iter,
                                                            entry.node_m.type_m,
                                                            entry.node_m.get_flag(atom_is_big_endian_k)));

    return result;
}

/**************************************************************************************************/

std::size_t fuzzer_t::fuzz_shuffle_set_with(const node_set_t& node_set)
{
    std::size_t count(node_set.size());

    // no sense in proceeding in these cases.
    if (count == 0 || count == 1)
        return 0;

    std::vector<rawbytes_t>     element_byte_set;
    boost::filesystem::ifstream bfinput(input_path_m, std::ios::binary);
    bitreader_t                 input(bfinput);
    inspection_position_t       startpos(invalid_position_k);
    inspection_position_t       endpos(invalid_position_k);

    // First we get the raw bytes of each element in the array, which we'll later
    // shuffle around and write out in their shuffled sequence starting at the
    // earliest starting offset found among the nodes.
    //
    // NOTE (fbrereto) : It is assumed all data from the beginning of the first
    //                   node to the end of the last node is in a contiguous block
    //                   in the file, and that the shuffling will be done within
    //                   this block.
    for (std::size_t i(0); i < count; ++i)
    {
        inspection_position_t start_offset(node_set[i]->start_offset_m);
        inspection_position_t end_offset(node_set[i]->end_offset_m);

        if (start_offset == invalid_position_k || end_offset == invalid_position_k ||
            !start_offset.byte_aligned() || !end_offset.byte_aligned() ||
            start_offset > end_offset)
        {
            output_m << "    ! invalid child node position(s)\n";

            return 0;
        }

        element_byte_set.push_back(input.read(start_offset, end_offset));

        // std::cerr << "element " << i << " : [" << start_offset << ".."
        //           << end_offset << "], size: "
        //           << element_byte_set.back().size() << '\n';

        if (startpos == invalid_position_k || start_offset < startpos)
            startpos = start_offset;

        if (endpos == invalid_position_k || end_offset > endpos)
            endpos = end_offset;
    }

    if (startpos == invalid_position_k || endpos == invalid_position_k)
    {
        output_m << "    ! bad blocking\n";

        return 0;
    }

    rawbytes_t file_prefix(input.read(bytepos(0),
                                      startpos - inspection_byte_k));
    rawbytes_t file_suffix(input.read(endpos + inspection_byte_k,
                                      input.size() - inspection_byte_k));

    basename_m += "_" + boost::lexical_cast<std::string>(startpos.bytes());

    // std::cerr << "prefix is : " << file_prefix.size() << '\n';
    // std::cerr << "suffix is : " << file_suffix.size() << '\n';

    // we start at 1 instead of 0 to reflect the fact we're avoiding the
    // 'no shift' option; each result will be different than the source file.
    for (std::size_t i(1); i < count; ++i)
    {
        // shift the blocks one to the left.
        std::rotate(element_byte_set.begin(),
                    element_byte_set.begin() + 1,
                    element_byte_set.end());

        std::string cur_name(basename_m);

        cur_name += "_s_" + boost::lexical_cast<std::string>(i) + extension_m;

        boost::filesystem::path output_path(base_output_path_m / cur_name);

        if (!validate_unique_outfile(output_path, output_m))
            continue;

        if (exists(output_path) && !remove(output_path))
            throw std::runtime_error("Could not replace file for fuzzed output.");

        boost::filesystem::ofstream output(output_path, std::ios::binary);

        rawbytes_out(output, file_prefix);

        for (std::size_t j(0); j < count; ++j)
            rawbytes_out(output, element_byte_set[j]);

        rawbytes_out(output, file_suffix);

        output_m << "    > " << cur_name << '\n';
    }

    return (count - 1);
}

/**************************************************************************************************/

std::size_t fuzzer_t::fuzz_shuffle_entry(const attack_vector_t& entry)
{
    std::size_t size = static_cast<std::size_t>(entry.node_m.cardinal_m);

    output_m << entry.path_m << '\n';
    output_m << "    ? attack_type : shuffle\n";
    output_m << "    ? array_size  : " << size << '\n';

    // There are N! possible permutations of shuffledness... so with a
    // decently-sized array you're looking at an intractably large set
    // of fuzzed documents. (e.g., 25 elements = 25! = 1.551121e25
    // possible permutations.) As such our current attack scheme will
    // be a simple rotation, so each array element gets a chance to be
    // at every position within the document. This will give us N - 1
    // files upon output and is a good start for an attack of this type.

    typedef std::vector<const_inspection_branch_t> child_node_set_t;
    typedef child_node_set_t::iterator             iterator;

    child_node_set_t                          node_set;
    inspection_forest_t::const_child_iterator iter(adobe::child_begin(entry.branch_m));
    inspection_forest_t::const_child_iterator last(adobe::child_end(entry.branch_m));

    for (; iter != last; ++iter)
        node_set.push_back(iter.base());

    if (entry.node_m.cardinal_m != node_set.size())
    {
        output_m << "    ! cardinality/node_count mismatch!\n";

        return 0;
    }

    return fuzz_shuffle_set_with(node_set);
}

/**************************************************************************************************/

std::size_t fuzzer_t::fuzz_usage_entry(const attack_vector_t& entry)
{
    // output_m << " > fuzzing " << bit_count << "b @ " << location << '\n';

    inspection_position_t location(entry.node_m.location_m);
    boost::uint64_t       bit_count(entry.node_m.bit_count_m);

    output_m << entry.path_m << '\n';
    output_m << "    ? attack_type : usage\n";
    output_m << "    ? use_count   : " << entry.count_m << '\n';
    output_m << "    ? offset      : " << location << '\n';
    output_m << "    ? bits        : " << bit_count << '\n';
    output_m << "    ? type        : ";

    if (entry.node_m.type_m == atom_signed_k)
        output_m << "signed";
    else if (entry.node_m.type_m == atom_unsigned_k)
        output_m << "unsigned";
    else if (entry.node_m.type_m == atom_float_k)
        output_m << "float";
    else // (entry.node_m.type_m == atom_unknown_k)
        output_m << "unknown";

    output_m << '\n';

    output_m << "    ? big_endian  : " << std::boolalpha << entry.node_m.get_flag(atom_is_big_endian_k) << '\n';

    if (!entry.node_m.option_set_m.empty())
        output_m << "    ? enum_count  : " << entry.node_m.option_set_m.size() << '\n';

    std::size_t result(0);

    result += fuzz_location_with(location,
                                 bit_count,
                                 zero_generator_t());

    result += fuzz_location_with(location,
                                 bit_count,
                                 ones_generator_t());

    if (!entry.node_m.option_set_m.empty())
    {
        // We're looking at an enumerated value. Each of the options
        // in the set are values to which we can set this enumeration,
        // so let's go and do that just to see what breaks.

        result += fuzz_location_with_enumeration(location,
                                                 bit_count,
                                                 entry);
    }

    return result;
}

/**************************************************************************************************/

void fuzzer_t::fuzz(const inspection_forest_t& forest)
{
    if (!is_directory(base_output_path_m))
        throw std::runtime_error(adobe::make_string("Output directory failure: ",
                                                    base_output_path_m.string().c_str()));

    attack_vector_set_t attack_vector_set(build_attack_vector_set(forest));
    std::size_t         count(attack_vector_set.size());
    std::size_t         progress(0);
    std::size_t         result(0);

    std::cerr << "Fuzzing " << count << " weak points ";

    for (std::size_t i(0); i < count; ++i)
    {
        const attack_vector_t& attack(attack_vector_set[i]);

        if (attack.type_m == attack_vector_t::type_atom_usage_k)
            result += fuzz_usage_entry(attack);
        else if (attack.type_m == attack_vector_t::type_array_shuffle_k)
            result += fuzz_shuffle_entry(attack);
        else
            output_m << "    ! ERROR: Unknown attack vector type.\n";

        std::size_t next_progress(static_cast<double>(i) / count * 100);

        if (progress != next_progress)
            std::cerr << '.';

        progress = next_progress;
    }

    std::cerr << " done.\n";
    std::cerr << "Generated " << result << " files\n";
}

/**************************************************************************************************/

boost::filesystem::path get_base_output_path(const boost::filesystem::path& input_path,
                                             const boost::filesystem::path& output_root)
{
    boost::filesystem::path result;

    if (exists(output_root))
        result = output_root;
    else
        result = input_path.parent_path();

    result /= "fuzzed";

    if (!exists(result))
        boost::filesystem::create_directory(result);

    return result;
}

/**************************************************************************************************/

fuzzer_t::fuzzer_t(const boost::filesystem::path& input_path,
                   const boost::filesystem::path& output_root) :
    input_path_m(input_path),
    base_output_path_m(get_base_output_path(input_path_m, output_root)),
    basename_m(input_path_m.stem().string()),
    extension_m(input_path_m.extension().string()),
    output_m(base_output_path_m / (basename_m + "_fuzzing_summary.txt"))
{ }

/**************************************************************************************************/

} // namespace

/**************************************************************************************************/

void fuzz(const inspection_forest_t&     forest,
          const boost::filesystem::path& input_path,
          const boost::filesystem::path& output_root)
{
    fuzzer_t(input_path, output_root).fuzz(forest);
}

/**************************************************************************************************/
