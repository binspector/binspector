/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// stdc
#include <sys/stat.h>

// stdc++
#include <iostream>
#include <fstream>
#include <memory>
#include <random>
#include <set>

// boost
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/version.hpp>

// asl
#include <adobe/string.hpp>
#include <adobe/iomanip_asl_cel.hpp>
#include <adobe/fnv.hpp>

// stlab
#include <stlab/scope.hpp>

// application
#include <binspector/analyzer.hpp>
#include <binspector/parser.hpp>
#include <binspector/fuzz_generators.hpp>
#include <binspector/ostream.hpp>

/****************************************************************************************************/

namespace {

typedef tsos<boost::filesystem::ofstream> tsofstream_t;

/****************************************************************************************************/

template <typename T>
auto block_on(const stlab::future<T>& f) {
    while (!f.get_try()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    return *f.get_try();
}

/****************************************************************************************************/

inline auto read(async_bitreader reader, const node_t& node) {
    return reader(node.location_m, node.bit_count_m);
}

/****************************************************************************************************/

inline rawbytes_t read_sync(async_bitreader reader, const node_t& node) {
    return block_on(read(reader, node));
}

/****************************************************************************************************/
// might want to make this into a standalone struct that can be owned by the fuzzer, so the
// set isn't shared across multiple fuzzes (if we ever get there.)
bool validate_unique_outfile(const boost::filesystem::path& file, tsofstream_t& output) {
    static std::set<boost::filesystem::path> file_set;
    static std::mutex                        m;

    return stlab::scope<std::lock_guard<std::mutex>>(m, [&]{
        if (file_set.find(file) != file_set.end()) {
            // oops. Don't want to redo an file we've already made.
            // Figure out why you got here and fix it, yo!

            output << "    ! Internal error: duplicate file: " << file.c_str() << '\n';

            return false;
        }

        // Track this file location to make sure it's unique across the files generated.
        file_set.insert(file);

        return true;
    });
};

/****************************************************************************************************/

bool duplicate_file(const boost::filesystem::path& src,
                    const boost::filesystem::path& dst,
                    tsofstream_t&                  output) {
    if (!validate_unique_outfile(dst, output))
        return false;

    boost::system::error_code ec;

    copy_file(src, dst, boost::filesystem::copy_option::fail_if_exists, ec);

    if (ec) {
        output << "    ! Could not replace " << dst.c_str() << '\n';

        return false;
    }

// gotta set the right permissions; we probably need a chmod equivalent
// on windows before this will function over there.

#if BOOST_WINDOWS
// necessary for windows?
#else
    chmod(dst.c_str(), S_IRUSR | S_IWUSR);
#endif

    return true;
}

/****************************************************************************************************/

inline void rawbytes_out(std::ofstream& output, const rawbytes_t& data) {
    if (data.empty())
        return;

    output.write(reinterpret_cast<const char*>(&data[0]), data.size());
}

/****************************************************************************************************/

struct fuzzer_t {
    fuzzer_t(async_bitreader                reader,
             const boost::filesystem::path& input_path,
             const boost::filesystem::path& output_root,
             bool                           path_hash,
             bool                           recurse);

    void fuzz(const inspection_forest_t& forest);

private:
    typedef std::vector<const_inspection_branch_t> node_set_t;

    boost::optional<std::string> fuzz_location_with_opt(const boost::filesystem::path& input_path,
                                                        const inspection_position_t&   location,
                                                        const fuzz_generator_t&        generator);

    std::size_t fuzz_location_with(const boost::filesystem::path& input_path,
                                   const inspection_position_t&   location,
                                   const fuzz_generator_t&        generator) {
        return fuzz_location_with_opt(input_path, location, generator) ? 1 : 0;
    }

    std::size_t fuzz_location_with_enumeration(const inspection_position_t& location,
                                               const attack_vector_t&       entry);

    std::size_t attack_chunk_array_with(const node_set_t& node_set);

    std::size_t attack_chunk_array(const attack_vector_t& entry);

    std::size_t attack_used_value(const attack_vector_t& entry);

    void usage_entry_report(const attack_vector_t& entry);

    bool should_recurse() const;

    std::size_t fuzz_round(boost::filesystem::path    input_path,
                           const inspection_forest_t& forest,
                           const attack_vector_set_t& attack_vector_set);

    void fuzz_recursive(const inspection_forest_t& forest,
                        const attack_vector_set_t& attack_vector_set);

    void fuzz_flat(const inspection_forest_t& forest,
                   const attack_vector_set_t& attack_vector_set);

    async_bitreader             reader_m;
    boost::filesystem::path     input_path_m;
    boost::filesystem::path     base_output_path_m;
    std::string                 basename_m;
    std::string                 extension_m;
    tsofstream_t                output_m; // summary output
    bool                        path_hash_m;
    bool                        recurse_m;
};

/****************************************************************************************************/

boost::optional<std::string> fuzzer_t::fuzz_location_with_opt(const boost::filesystem::path& input_path,
                                                              const inspection_position_t&   location,
                                                              const fuzz_generator_t&        generator) {
    auto bit_count(generator.bit_count());

    if (bit_count % 8 != 0 || !location.byte_aligned()) {
        output_m
            << "    ! Bit-level/byte-unaligned fuzzing is not yet supported. Location skipped.\n";

        return boost::optional<std::string>();
    }

    std::string attack_id = basename_m + "_" + serialize(location);

    if (path_hash_m) {
        if (recurse_m) {
            // append the input path so recursive fuzzes will have different
            // file names if they wind up attacking the same location.
            attack_id += input_path.string();
        }

        attack_id = to_string_fmt(adobe::fnv1a<64>(attack_id), "%llx");
    }

    // REVISIT (fbrereto) : String concatenation here.
    std::string id       = generator.identifier();
    std::string cur_name = attack_id + "_" + std::move(id) + extension_m;

    boost::filesystem::path output_path(base_output_path_m / cur_name);

    if (!duplicate_file(input_path, output_path, output_m))
        return boost::optional<std::string>();

    boost::filesystem::fstream fuzzed(output_path, std::ios::binary | std::ios::in | std::ios::out);

    // std::cerr << canonical(output_path).string() << '\n';

    if (!fuzzed) {
        perror("!");

        throw std::runtime_error("Could not open file for fuzzed output.");
    }

    std::fstream::pos_type pos(location.bytes());

    fuzzed.seekp(pos);

    std::size_t byte_count(bit_count / 8);
    rawbytes_t  fuzz_data(generator());

    fuzzed.write(reinterpret_cast<const char*>(&fuzz_data[0]), byte_count);

    output_m << "    > " << cur_name << '\n';

    return boost::optional<std::string>(cur_name);
}

/****************************************************************************************************/

std::size_t fuzzer_t::fuzz_location_with_enumeration(const inspection_position_t& location,
                                                     const attack_vector_t&       entry) {
    if (entry.node_m.get_flag(is_array_element_k))
        std::cerr << "WARNING: fuzzing an array entry; the forest may not be set up properly.\n";

    const adobe::array_t& enumerated_option_set(entry.node_m.option_set_m);
    std::size_t           result(0);

    for (const auto& enum_option : enumerated_option_set)
        result +=
            fuzz_location_with(input_path_m,
                               location,
                               make_enum_generator(enum_option,
                                                   entry.node_m.type_m,
                                                   entry.node_m.get_flag(atom_is_big_endian_k),
                                                   entry.node_m.bit_count_m));

    return result;
}

/****************************************************************************************************/

std::size_t fuzzer_t::attack_chunk_array_with(const node_set_t& node_set) {
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
    for (std::size_t i(0); i < count; ++i) {
        inspection_position_t start_offset(node_set[i]->start_offset_m);
        inspection_position_t end_offset(node_set[i]->end_offset_m);

        if (start_offset == invalid_position_k || end_offset == invalid_position_k ||
            !start_offset.byte_aligned() || !end_offset.byte_aligned() ||
            start_offset > end_offset) {
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

    if (startpos == invalid_position_k || endpos == invalid_position_k) {
        output_m << "    ! bad blocking\n";

        return 0;
    }

    rawbytes_t file_prefix(input.read(bytepos(0), startpos - inspection_byte_k));
    rawbytes_t file_suffix(
        input.read(endpos + inspection_byte_k, input.size() - inspection_byte_k));

    // std::cerr << "prefix is : " << file_prefix.size() << '\n';
    // std::cerr << "suffix is : " << file_suffix.size() << '\n';

    // we start at 1 instead of 0 to reflect the fact we're avoiding the
    // 'no shift' option; each result will be different than the source file.
    for (std::size_t i(1); i < count; ++i) {
        // shift the blocks one to the left.
        std::rotate(element_byte_set.begin(), element_byte_set.begin() + 1, element_byte_set.end());

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

/****************************************************************************************************/

std::size_t fuzzer_t::attack_chunk_array(const attack_vector_t& entry) {
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
    //typedef child_node_set_t::iterator             iterator;

    child_node_set_t                          node_set;
    inspection_forest_t::const_child_iterator iter(adobe::child_begin(entry.branch_m));
    inspection_forest_t::const_child_iterator last(adobe::child_end(entry.branch_m));

    for (; iter != last; ++iter)
        node_set.push_back(iter.base());

    if (entry.node_m.cardinal_m != node_set.size()) {
        output_m << "    ! cardinality/node_count mismatch!\n";

        return 0;
    }

    return attack_chunk_array_with(node_set);
}

/****************************************************************************************************/

void fuzzer_t::usage_entry_report(const attack_vector_t& entry) {
    // output_m << " > fuzzing " << bit_count << "b @ " << location << '\n';

    inspection_position_t location(entry.node_m.location_m);
    boost::uint64_t       bit_count(entry.node_m.bit_count_m);

    output_m << entry.path_m << '\n';

    if (recurse_m)
        output_m << "    ? base_input  : " << input_path_m.leaf().string() << "\n";

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

    output_m << "    ? big_endian  : " << std::boolalpha
             << entry.node_m.get_flag(atom_is_big_endian_k) << '\n';

    if (!entry.node_m.option_set_m.empty())
        output_m << "    ? enum_count  : " << entry.node_m.option_set_m.size() << '\n';
}

/****************************************************************************************************/

std::size_t fuzzer_t::attack_used_value(const attack_vector_t& entry) {
    usage_entry_report(entry);

    inspection_position_t location(entry.node_m.location_m);
    boost::uint64_t       bit_count(entry.node_m.bit_count_m);
    std::size_t           result(0);

    result += fuzz_location_with(input_path_m, location, make_zero_generator(bit_count));

    result += fuzz_location_with(input_path_m, location, make_ones_generator(bit_count));

    rawbytes_t raw{read_sync(reader_m, entry.node_m)};

    result +=
        fuzz_location_with(input_path_m, location, make_less_generator(entry.node_m, raw));

    result +=
        fuzz_location_with(input_path_m, location, make_more_generator(entry.node_m, raw));

    result += fuzz_location_with(input_path_m, location, make_rand_generator(bit_count));

    if (!entry.node_m.option_set_m.empty()) {
        // We're looking at an enumerated value. Each of the options
        // in the set are values to which we can set this enumeration,
        // so let's go and do that just to see what breaks.

        result += fuzz_location_with_enumeration(location, entry);
    }

    return result;
}

/****************************************************************************************************/

void fuzzer_t::fuzz_recursive(const inspection_forest_t& forest,
                              const attack_vector_set_t& attack_vector_set) {
    std::size_t result(0);
    std::size_t progress(0);

    std::cerr << "Fuzzing random weak points ";

    std::vector<stlab::future<std::size_t>> futures;

    constexpr std::size_t n_k{10000};

    while (true) {
        while (futures.size() < 10) {
            futures.push_back(stlab::async(stlab::default_executor,
                                           [_this = this, &_forest = forest, &_avs = attack_vector_set] {
                return _this->fuzz_round(_this->input_path_m, _forest, _avs);
            }));
        }

        auto mid = std::partition(begin(futures), end(futures), [](auto f){
            return static_cast<bool>(f.get_try());
        });

        for (auto iter(begin(futures)); iter != mid; ++iter) {
            result += *iter->get_try();
        }

        futures.erase(begin(futures), mid);

        if (result > n_k)
            break;

        std::size_t next_progress(static_cast<double>(result) / n_k * 100);

        if (progress != next_progress)
            std::cerr << '.';

        progress = next_progress;
    }

    for (const auto& f : futures) {
        result += block_on(f);
    }

    std::cerr << " done. Generated " << result << " files\n";
}

/****************************************************************************************************/

bool fuzzer_t::should_recurse() const {
    static thread_local std::mt19937                     gen{std::random_device()()};
    static thread_local std::uniform_real_distribution<> r_dist(0, 1);

    return r_dist(gen) < 0.8;
}

/****************************************************************************************************/

std::size_t fuzzer_t::fuzz_round(boost::filesystem::path    input_path,
                                 const inspection_forest_t& forest,
                                 const attack_vector_set_t& attack_vector_set) {
    static thread_local std::mt19937 gen{std::random_device()()};

    std::uniform_int_distribution<std::size_t> dist(0, attack_vector_set.size() - 1);
    std::size_t                                attack_index(dist(gen));
    const attack_vector_t&                     attack(attack_vector_set[attack_index]);
    std::size_t                                result(0);

    if (attack.type_m == attack_vector_t::type_atom_usage_k) {
        std::size_t                                options_count(attack.node_m.option_set_m.size());
        std::uniform_int_distribution<std::size_t> dist(0, options_count + 4);
        std::size_t                                selected(dist(gen));
        fuzz_generator_t                           g;

        if (selected == 0) {
            g = fuzz_generator_t(make_zero_generator(attack.node_m.bit_count_m));
        } else if (selected == 1) {
            g = fuzz_generator_t(make_ones_generator(attack.node_m.bit_count_m));
        } else if (selected == 2) {
            g = fuzz_generator_t(make_less_generator(attack.node_m, read_sync(reader_m, attack.node_m)));
        } else if (selected == 3) {
            g = fuzz_generator_t(make_more_generator(attack.node_m, read_sync(reader_m, attack.node_m)));
        } else if (selected == 4) {
            g = fuzz_generator_t(make_rand_generator(attack.node_m.bit_count_m));
        } else {
            selected -= 5;

            g = make_enum_generator(attack.node_m.option_set_m[selected],
                                    attack.node_m.type_m,
                                    attack.node_m.get_flag(atom_is_big_endian_k),
                                    attack.node_m.bit_count_m);
        }

        usage_entry_report(attack);

        boost::optional<std::string> new_path(fuzz_location_with_opt(input_path, attack.node_m.location_m, g));

        if (new_path) {
            ++result;

            if (should_recurse()) {
                result += fuzz_round(base_output_path_m / *new_path, forest, attack_vector_set);
            }
        }
    }

    return result;
}

/****************************************************************************************************/

void fuzzer_t::fuzz_flat(const inspection_forest_t& forest,
                         const attack_vector_set_t& attack_vector_set) {
    std::size_t count(attack_vector_set.size());
    std::size_t progress(0);
    std::size_t result(0);

    std::cerr << "Fuzzing " << count << " weak points ";

    for (std::size_t i(0); i < count; ++i) {
        const attack_vector_t& attack(attack_vector_set[i]);

        if (attack.type_m == attack_vector_t::type_atom_usage_k)
            result += attack_used_value(attack);
        else if (attack.type_m == attack_vector_t::type_array_shuffle_k)
            result += attack_chunk_array(attack);
        else
            output_m << "    ! ERROR: Unknown attack vector type.\n";

        std::size_t next_progress(static_cast<double>(i) / count * 100);

        if (progress != next_progress)
            std::cerr << '.';

        progress = next_progress;
    }

    std::cerr << " done. Generated " << result << " files\n";
}

/****************************************************************************************************/

void fuzzer_t::fuzz(const inspection_forest_t& forest) {
    if (!is_directory(base_output_path_m))
        throw std::runtime_error(
            adobe::make_string("Output directory failure: ", base_output_path_m.string().c_str()));

    attack_vector_set_t attack_vector_set(build_attack_vector_set(forest));

    if (!recurse_m)
        fuzz_flat(forest, attack_vector_set);
    else
        fuzz_recursive(forest, attack_vector_set);
}

/****************************************************************************************************/

boost::filesystem::path get_base_output_path(const boost::filesystem::path& input_path,
                                             const boost::filesystem::path& output_root) {
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

/****************************************************************************************************/

fuzzer_t::fuzzer_t(async_bitreader                reader,
                   const boost::filesystem::path& input_path,
                   const boost::filesystem::path& output_root,
                   bool                           path_hash,
                   bool                           recurse)
    : reader_m(reader), input_path_m(input_path), base_output_path_m(get_base_output_path(input_path_m, output_root)),
      basename_m(input_path_m.stem().string()), extension_m(input_path_m.extension().string()),
      output_m(base_output_path_m / (basename_m + "_fuzzing_summary.txt")), path_hash_m(path_hash),
      recurse_m(recurse) {}

/****************************************************************************************************/

} // namespace

/****************************************************************************************************/

void fuzz(const inspection_forest_t&     forest,
          const boost::filesystem::path& input_path,
          const boost::filesystem::path& output_root,
          bool                           path_hash,
          bool                           recurse) {
    fuzzer_t(make_async_bitreader(input_path), input_path, output_root, path_hash, recurse).fuzz(forest);
}

/****************************************************************************************************/
