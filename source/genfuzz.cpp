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

struct genfuzz_t
{
    explicit genfuzz_t(const boost::filesystem::path& output_root);

    void fuzz(const inspection_forest_t& forest);

private:
    typedef std::vector<const_inspection_branch_t> node_set_t;

    boost::filesystem::path     input_path_m;
    boost::filesystem::path     base_output_path_m;
    std::string                 basename_m;
    std::string                 extension_m;
    boost::filesystem::ofstream output_m; // summary output
};

/**************************************************************************************************/

void genfuzz_t::fuzz(const inspection_forest_t& forest)
{
    if (!is_directory(base_output_path_m))
        throw std::runtime_error(adobe::make_string("Output directory failure: ",
                                                    base_output_path_m.string().c_str()));

    std::cerr << "genfuzz_t::fuzz\n";

    output_m << "genfuzz_t::fuzz\n";
}

/**************************************************************************************************/

boost::filesystem::path get_base_output_path(const boost::filesystem::path& output_root)
{
    boost::filesystem::path result;

    if (exists(output_root))
        result = output_root;

    result /= "genfuzz";

    if (!exists(result))
        boost::filesystem::create_directory(result);

    return result;
}

/**************************************************************************************************/

genfuzz_t::genfuzz_t(const boost::filesystem::path& output_root) :
    base_output_path_m(get_base_output_path(output_root)),
    basename_m("genfuzz"),
    extension_m("fixme.txt"),
    output_m(base_output_path_m / (basename_m + "_fuzzing_summary.txt"))
{ }

/**************************************************************************************************/

} // namespace

/**************************************************************************************************/

void genfuzz(const inspection_forest_t&     forest,
             const boost::filesystem::path& output_root)
{
    genfuzz_t(output_root).fuzz(forest);
}

/**************************************************************************************************/
