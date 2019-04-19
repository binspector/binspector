/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// stdc++
#include <iostream>
#include <fstream>

// boost
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/tee.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/program_options.hpp>
#include <boost/version.hpp>
#include <boost/bind.hpp>

// asl
#include <adobe/config.hpp>

// stlab
#include <stlab/version.hpp>

// application
#include <binspector/analyzer.hpp>
#include <binspector/dot.hpp>
#include <binspector/fuzzer.hpp>
#include <binspector/html_dump.hpp>
#include <binspector/interface.hpp>
#include <binspector/parser.hpp>

/****************************************************************************************************/

typedef std::vector<boost::filesystem::path> path_set;

/****************************************************************************************************/

int main(int argc, char** argv) try {
    boost::program_options::options_description cli_parameters("Options");
    boost::filesystem::path                     template_path;
    boost::filesystem::path                     binary_path;
    boost::filesystem::path                     output_path;
    std::string                                 output_mode;
    std::string                                 starting_struct;
    std::string                                 dump_path;
    path_set                                    include_path_set;
    bool                                        quiet(false);
    bool                                        path_hash(false);
    bool                                        fuzz_recurse(false);

    cli_parameters.add_options()("help,?", "Print this help message then exits")(
        "template,t",
        boost::program_options::value<boost::filesystem::path>(&template_path),
        "Specify the template file on which to base binary processing")(
        "input,i",
        boost::program_options::value<boost::filesystem::path>(&binary_path),
        "Specify the binary file to process")(
        "output-mode,m",
        boost::program_options::value<std::string>(&output_mode)->default_value("cli"),
        "Specify output mode. One of:\n\
       cli: \tUser-interactive command line interface (the default)\n\
      text: \ttext dump of entire analyzed structure (to stdout)\n\
      html: \tformatted HTML output of entire analyzed structure (to stdout)\n\
  validate: \tvalidation of binary file given the template. Only outputs notifications and errors (to stdout), then exits\n\
      fuzz: \tintelligent document fuzzing engine (multi-file output)\n\
      dot:  \tgenerate template file dot graph for visualization")(
        "include,I",
        boost::program_options::value<path_set>(&include_path_set)->composing(),
        "Specify additional include paths during template file parsing -- currently unimplemented")(
        "output-directory,o",
        boost::program_options::value<boost::filesystem::path>(&output_path),
        "Specify directory for results (fuzz and dot output modes only)")(
        "path,p",
        boost::program_options::value<std::string>(&dump_path),
        "With -m text, only dumps the specified path")(
        "quiet,q",
        boost::program_options::bool_switch(&quiet),
        "Suppress notifications and summaries (implied for -m fuzz)")(
        "path-hash,h",
        boost::program_options::bool_switch(&path_hash),
        "hashes output file names (fuzz output mode only)")(
        "fuzz-recurse,r",
        boost::program_options::bool_switch(&fuzz_recurse),
        "Recursive fuzz mode. Generates about 1000 files. Each time a file is generated, there's a good chance it will be used as the basis for another fuzz. Implies --path_hash")(
        "starting-struct,s",
        boost::program_options::value<std::string>(&starting_struct)->default_value("main"),
        "Specify struct to use as root for analysis and -m dot");

    boost::program_options::variables_map var_map;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, cli_parameters), var_map);
    boost::program_options::notify(var_map);

    if (argc == 1 || var_map.count("help")) {
        constexpr auto STLAB_VERSION_MAJOR(STLAB_VERSION / 100000);
        constexpr auto STLAB_VERSION_MINOR(STLAB_VERSION / 100 % 1000);
        constexpr auto STLAB_VERSION_SUBMINOR(STLAB_VERSION % 100);

        std::cout << "Usage:\n"
                  << "  " << boost::filesystem::path(argv[0]).stem().string() << " [options]\n"
                  << '\n'
                  << cli_parameters << '\n'
                  << "Returns:\n"
                  << "  The tool will return 0 iff binary analysis passes; 1 otherwise.\n"
                  << '\n'
                  << "Build Information:\n"
                  << "  Timestamp: " << __DATE__ << " " << __TIME__ << '\n'
                  << "   Compiler: " << BOOST_COMPILER << '\n'
                  << "      stlab: " << STLAB_VERSION_MAJOR << "." << STLAB_VERSION_MINOR << "."
                  << STLAB_VERSION_SUBMINOR << '\n'
                  << "        asl: " << ADOBE_VERSION_MAJOR << "." << ADOBE_VERSION_MINOR << "."
                  << ADOBE_VERSION_SUBMINOR << '\n'
                  << "      boost: " << (BOOST_VERSION / 100000) << "."
                  << (BOOST_VERSION / 100 % 1000) << "." << (BOOST_VERSION % 100) << '\n'
#ifndef NDEBUG
                  << "       Mode: Debug\n";
#else
                  << "       Mode: Release\n";
#endif
        ;

        return 1;
    }

    // Open the template file, if we can.
    if (!exists(template_path)) {
        std::string error("Template file ");

        // REVISIT (fbrereto): performance on these concats
        if (template_path.empty())
            error += "unspecified";
        else
            error += "'" + template_path.string() + "' could not be located or does not exist.";

        throw std::runtime_error(error);
    }

    boost::filesystem::ifstream template_description(template_path);

    if (!template_description)
        throw std::runtime_error("Could not open template file");

    // Open the binary file, if we can.
    if (!exists(binary_path) && output_mode != "dot") {
        std::string error("Binary file ");

        // REVISIT (fbrereto): performance on these concats
        if (binary_path.empty())
            error += "unspecified";
        else
            error += "'" + binary_path.string() + "' could not be located or does not exist.";

        throw std::runtime_error(error);
    }

    boost::filesystem::ifstream binary(binary_path, std::ios_base::binary);

    if (!binary && output_mode != "dot")
        throw std::runtime_error("Could not open binary input file");

    // Set up output and error streams
    std::ostringstream outstream;
    std::ostringstream errstream;
    std::ostringstream combostream;

    typedef boost::iostreams::tee_device<std::ostringstream, std::ostringstream> tee_device_t;
    typedef boost::iostreams::stream<tee_device_t> tee_stream_t;

    tee_device_t dout(combostream, outstream);
    tee_device_t derr(combostream, errstream);
    tee_stream_t sout(dout);
    tee_stream_t serr(derr);

    // kick up the analyzer in preparation for template parsing
    // REVISIT (fbrereto) : consider moving the pass of the binary
    //                      stream to analyze_binary
    binspector_analyzer_t analyzer(binary, sout, serr);

    analyzer.set_quiet(quiet || output_mode == "fuzz");

    try {
        adobe::line_position_t::getline_proc_t getline(
            new adobe::line_position_t::getline_proc_impl_t(
                boost::bind(&get_input_line, boost::ref(template_description), _2)));

        include_path_set.insert(include_path_set.begin(), template_path.parent_path());

        binspector_parser_t(
            template_description,
            adobe::line_position_t(adobe::name_t(template_path.string().c_str()), getline),
            include_path_set,
            boost::bind(&binspector_analyzer_t::set_current_structure, boost::ref(analyzer), _1),
            boost::bind(&binspector_analyzer_t::add_named_field, boost::ref(analyzer), _1, _2),
            boost::bind(&binspector_analyzer_t::add_unnamed_field, boost::ref(analyzer), _1),
            boost::bind(&binspector_analyzer_t::add_typedef, boost::ref(analyzer), _1, _2))
            .parse();
    } catch (const adobe::stream_error_t& error) {
        throw std::runtime_error(adobe::format_stream_error(template_description, error));
    }

    // once the parse is done we don't need the main template file anymore.
    template_description.close();

    // Do the actual analysis, set the return result so we can track errors therein
    int result(analyzer.analyze_binary(starting_struct) == false);

    // clean up our output and error tee buffers
    sout.flush();
    sout.close();
    serr.flush();
    serr.close();

    // grab the analysis forest - this is a huge structure.
    auto_forest_t forest(analyzer.forest());

    if (output_mode == "cli") {
        std::cout << combostream.str();

        binspector_interface_t interface(binary, std::move(forest), std::cout);

        interface.command_line();
    } else if (output_mode == "text") {
        binspector_interface_t interface(binary, std::move(forest), std::cout);
        bool                   ok_to_dump(true);

        if (!dump_path.empty()) {
            binspector_interface_t::command_segment_set_t path_command;

            path_command.push_back("step_in");
            path_command.push_back(dump_path);

            ok_to_dump = interface.step_in(path_command);
        }

        if (ok_to_dump)
            interface.print_branch();
        else
            throw std::runtime_error("Text dump error.");
    } else if (output_mode == "html") {
        binspector_html_dump(binary, std::move(forest), std::cout, outstream.str(), errstream.str());
    } else if (output_mode == "validate") {
        std::cout << combostream.str();
    } else if (output_mode == "fuzz") {
        fuzz(*forest, binary_path, output_path, path_hash || fuzz_recurse, fuzz_recurse);
    } else if (output_mode == "dot") {
        // at this point the source binary isn't needed for fuzzing
        // so we can free it up.
        binary.close();

        dot_graph(analyzer.structure_map(), starting_struct, output_path);
    } else {
        throw std::runtime_error(
            "Unknown output mode. Please refer to -? (--help) or tool documentation for more information.");
    }

    return result;
} catch (const user_quit_exception_t&) {
    std::cout << "Goodbye." << std::endl;
    return 0;
} catch (const std::exception& error) {
    std::cerr << "Fatal error: " << error.what() << std::endl;
    return 1;
} catch (...) {
    std::cerr << "Fatal error: unknown" << std::endl;
    return 1;
}

/****************************************************************************************************/
