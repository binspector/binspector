/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// identity
#include <binspector/interface.hpp>

// stdc++
#include <fstream>

// asl
#include <adobe/istream.hpp>
#include <adobe/implementation/expression_parser.hpp>

/****************************************************************************************************/

namespace {

/****************************************************************************************************/

inline void indent_stream(std::ostream& stream, std::size_t count)
    { for (; count != 0; --count) stream << "    "; }

void stream_out(const adobe::any_regular_t& value,
                boost::uint64_t             /*bit_length*/,
                atom_base_type_t            base_type,
                std::ostream&               s)
{
    double d(value.cast<double>());

    if (base_type == atom_signed_k)
        s << static_cast<signed long long>(d);
    else if (base_type == atom_unsigned_k)
        s << static_cast<unsigned long long>(d);
    else if (base_type == atom_float_k)
        s << d;
}

/****************************************************************************************************/

} // namespace

/****************************************************************************************************/
#if 0
#pragma mark -
#endif
/****************************************************************************************************/

binspector_interface_t::binspector_interface_t(std::istream& binary_file,
                                   auto_forest_t forest,
                                   std::ostream& output) :
    input_m(binary_file),
    forest_m(forest),
    output_m(output),
    node_m(forest_m->begin())
{
    command_map_m.insert(command_map_t::value_type("q",                   &binspector_interface_t::quit));
    command_map_m.insert(command_map_t::value_type("quit",                &binspector_interface_t::quit));
    command_map_m.insert(command_map_t::value_type("?",                   &binspector_interface_t::help));
    command_map_m.insert(command_map_t::value_type("help",                &binspector_interface_t::help));
    command_map_m.insert(command_map_t::value_type("ll",                  &binspector_interface_t::print_structure));
    command_map_m.insert(command_map_t::value_type("ls",                  &binspector_interface_t::print_structure));
    command_map_m.insert(command_map_t::value_type("ps",                  &binspector_interface_t::print_structure));
    command_map_m.insert(command_map_t::value_type("print_struct",        &binspector_interface_t::print_structure));
    command_map_m.insert(command_map_t::value_type("pb",                  &binspector_interface_t::print_branch));
    command_map_m.insert(command_map_t::value_type("print_branch",        &binspector_interface_t::print_branch));
    command_map_m.insert(command_map_t::value_type("str",                 &binspector_interface_t::print_string));
    command_map_m.insert(command_map_t::value_type("print_string",        &binspector_interface_t::print_string));
    command_map_m.insert(command_map_t::value_type("cd",                  &binspector_interface_t::step_in));
    command_map_m.insert(command_map_t::value_type("si",                  &binspector_interface_t::step_in));
    command_map_m.insert(command_map_t::value_type("step_in",             &binspector_interface_t::step_in));
    command_map_m.insert(command_map_t::value_type("so",                  &binspector_interface_t::step_out));
    command_map_m.insert(command_map_t::value_type("step_out",            &binspector_interface_t::step_out));
    command_map_m.insert(command_map_t::value_type("t",                   &binspector_interface_t::top));
    command_map_m.insert(command_map_t::value_type("top",                 &binspector_interface_t::top));
    command_map_m.insert(command_map_t::value_type("df",                  &binspector_interface_t::detail_field));
    command_map_m.insert(command_map_t::value_type("detail_field",        &binspector_interface_t::detail_field));
    command_map_m.insert(command_map_t::value_type("do",                  &binspector_interface_t::detail_offset));
    command_map_m.insert(command_map_t::value_type("detail_offset",       &binspector_interface_t::detail_offset));
    command_map_m.insert(command_map_t::value_type("ee",                  &binspector_interface_t::evaluate_expression));
    command_map_m.insert(command_map_t::value_type("eval",                &binspector_interface_t::evaluate_expression));
    command_map_m.insert(command_map_t::value_type("evaluate_expression", &binspector_interface_t::evaluate_expression));
    command_map_m.insert(command_map_t::value_type("duf",                 &binspector_interface_t::dump_field));
    command_map_m.insert(command_map_t::value_type("dump_field",          &binspector_interface_t::dump_field));
    command_map_m.insert(command_map_t::value_type("duo",                 &binspector_interface_t::dump_offset));
    command_map_m.insert(command_map_t::value_type("dump_offset",         &binspector_interface_t::dump_offset));
    command_map_m.insert(command_map_t::value_type("sf",                  &binspector_interface_t::save_field));
    command_map_m.insert(command_map_t::value_type("save_field",          &binspector_interface_t::save_field));
    command_map_m.insert(command_map_t::value_type("usage_metrics",       &binspector_interface_t::usage_metrics));
    command_map_m.insert(command_map_t::value_type("um",                  &binspector_interface_t::usage_metrics));
    command_map_m.insert(command_map_t::value_type("ff",                  &binspector_interface_t::find_field));
    command_map_m.insert(command_map_t::value_type("find_field",          &binspector_interface_t::find_field));
}

/****************************************************************************************************/

bool binspector_interface_t::quit(const command_segment_set_t&)
{
    throw user_quit_exception_t();

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::help(const command_segment_set_t&)
{
    output_m << "Please consult the readme for more detailed help.\n";
    output_m << '\n';
    output_m << "quit (q)\n";
    output_m << "    Terminates the binspector\n";
    output_m << '\n';
    output_m << "help (?)\n";
    output_m << "    Prints binspector help\n";
    output_m << '\n';
    output_m << "print_struct (ps) (ls) (ll)\n";
    output_m << "    Displays a synopsis of the structure at the current path. For example:\n";
    output_m << '\n';
    output_m << "print_branch (pb)\n";
    output_m << "    Displays a complete synopsis of the current structure at the current path. Executing print_branch at $main$ will output the entire contents of the analysis. For leaf structures this command is equivalent to print_struct.\n";
    output_m << '\n';
    output_m << "print_string <path> (str)\n";
    output_m << "    Displays the field specified by the path as a string. Values that have no graphical representation (i.e., std::isgraph(c) == false) are output as their ASCII value in hex prepended with an \"\\x\".\n";
    output_m << '\n';
    output_m << "step_in <path> (si) (cd)\n";
    output_m << "    Sets the path to the structure defined by the path. This can be done both relative to the current path and absolutely from $main$.\n";
    output_m << '\n';
    output_m << "step_out (so)\n";
    output_m << "    Sets the path to be the parent structure of the current path. Note that in the case of an array element, the parent structure is the array root and not the structure that contains the array.\n";
    output_m << '\n';
    output_m << "top (t)\n";
    output_m << "    Sets the path to $main$\n";
    output_m << '\n';
    output_m << "detail_field <path> (df)\n";
    output_m << "    Prints out detailed information about the path specified. For example:\n";
    output_m << '\n';
    output_m << "detail_offset <offset> (do)\n";
    output_m << "    Searches the binary file analysis for the atom that interprets the byte at the provided offset. Currently only local data is included in the search (not remote data) though its inclusion is planned for a future release. For example:\n";
    output_m << '\n';
    output_m << "evaluate_expression <expression> (eval) (ee)\n";
    output_m << "    Allows for the evaluation of an expression whose result is immediately output. For example the following prints the starting offset of the main.dib_header.bpp field:\n";
    output_m << '\n';
    output_m << "dump_field <field1> <field2> (duf)\n";
    output_m << "    Dumps the on-disk bytes interpreted by the field (in the case one field is supplied) or range of fields (in the case two fields are supplied). Dump format is in five columns: the first four columns are the hexidecimal representation of 4 bytes each. The fifth column is the ASCII representation of the first four columns. If a byte fails std::isgraph a '.' is subsituted as the glyph in the fifth column.\n";
    output_m << '\n';
    output_m << "dump_offset <start_offset> <end_offset> (duo)\n";
    output_m << "    Same behavior as dump_field but takes a starting and ending offset into the file instead of field(s). The byte at the end offset is included in the dump.\n";
    output_m << '\n';
    output_m << "find_field name (ff)\n";
    output_m << "    Finds all the child fields of the current path with the specified name. Good for when you know a field is under a path, but you're not sure exactly where.\n";
    output_m << '\n';
    output_m << '\n';

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::print_branch(const command_segment_set_t&)
{
    inspection_branch_t begin(adobe::leading_of(node_m));
    inspection_branch_t end(adobe::trailing_of(node_m));

    print_branch_depth_range(std::make_pair(depth_full_iterator_t(begin),
                                            depth_full_iterator_t(++end)));

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::print_structure(const command_segment_set_t&)
{
    print_node(adobe::leading_of(node_m), true, 0);

    inspection_forest_t::child_iterator iter(adobe::child_begin(node_m));
    inspection_forest_t::child_iterator last(adobe::child_end(node_m));

    for (; iter != last; ++iter)
        print_node(iter.base(), false, 1);

    print_node(adobe::trailing_of(node_m), true, 0);

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::print_string(const command_segment_set_t& parameters)
{
#if !BOOST_WINDOWS
    using std::isgraph;
#endif

    std::size_t parameter_size(parameters.size());

    if (parameter_size < 2)
    {
        std::cerr << "Usage: print_string <expression>\n";

        return false;
    }

    std::string expression_string;

    for (std::size_t i(1); i < parameter_size; ++i)
        expression_string += parameters[i] + std::string(" ");

    try
    {
        std::stringstream      input(expression_string);
        adobe::line_position_t position(__FILE__, __LINE__);
        adobe::array_t         expression;

        adobe::expression_parser(input, position).require_expression(expression);

        inspection_branch_t node(contextual_evaluation_of<inspection_branch_t>(expression,
                                                                               forest_m->begin(),
                                                                               node_m,
                                                                               input_m));
        inspection_position_t      start_byte_offset(starting_offset_for(node));
        inspection_position_t      end_byte_offset(ending_offset_for(node));
        inspection_position_t      size(end_byte_offset - start_byte_offset + inspection_byte_k);
        std::size_t                bytes(static_cast<std::size_t>(size.bytes()));

        input_m.seek(start_byte_offset);

        rawbytes_t str(input_m.read(bytes));

        for (std::size_t i(0); i < bytes; ++i)
        {
            unsigned char c(str[i]);

            if (isgraph(c))
                output_m << c;
            else
            {
                output_m << "\\x" << std::hex;
                output_m.width(2);
                output_m.fill('0');
                output_m << (static_cast<int>(c) & 0xff) << std::dec;
            }
        }

        output_m << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "print_string error: " << error.what() << '\n';

        return false;
    }

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::step_in(const command_segment_set_t& parameters)
{
    if (parameters.size() != 2)
    {
        std::cerr << "Usage: step_in <substruct>\n";

        return false;
    }

    static const std::string up_k("..");

    const std::string& substruct(parameters[1]);

    if (substruct == up_k)
    {
        static const command_segment_set_t step_out_cmd_k(1, std::string("step_out"));

        step_out(step_out_cmd_k);
    }
    else
    {
        try
        {
            node_m = expression_to_node(substruct);
        }
        catch (const std::exception& error)
        {
            std::cerr << "step_in error: " << error.what() << '\n';

            return false;
        }
    }

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::step_out(const command_segment_set_t& parameters)
{
    if (parameters.size() != 1)
    {
        std::cerr << "Usage: step_out\n";

        return false;
    }

    node_m.edge() = adobe::forest_leading_edge;

    if (node_m != forest_m->begin())
        node_m = adobe::find_parent(node_m);

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::top(const command_segment_set_t& parameters)
{
    if (parameters.size() != 1)
    {
        std::cerr << "Usage: top\n";

        return false;
    }

    node_m = forest_m->begin();

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::detail_field(const command_segment_set_t& parameters)
{
    if (parameters.size() != 2)
    {
        std::cerr << "Usage: detail_field field\n";

        return false;
    }

    try
    {
        detail_node(expression_to_node(parameters[1]));
    }
    catch (const std::exception& error)
    {
        std::cerr << "detail_field error: " << error.what() << '\n';

        return false;
    }

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::detail_offset(const command_segment_set_t& parameters)
{
    if (parameters.size() != 2)
    {
        std::cerr << "Usage: detail_offset offset\n";

        return false;
    }

    boost::uint64_t       offset(std::atoi(parameters[1].c_str()));
    inspection_branch_t   current(forest_m->begin());
    inspection_position_t analysis_end(ending_offset_for(current));

    if (offset > analysis_end.bytes())
    {
        std::cerr << "Error: offset " << offset << " beyond analyzed range (" << analysis_end << ")\n";

        return false;
    }

    while (true)
    {
        inspection_forest_t::child_iterator iter(adobe::child_begin(current));
        inspection_forest_t::child_iterator last(adobe::child_end(current));

        if (iter == last)
            break;

        for (; iter != last; ++iter)
        {
            inspection_branch_t   iter_base(iter.base());
            bool                  is_valid(node_property(iter_base, NODE_PROPERTY_IS_STRUCT) ||
                                           node_property(iter_base, NODE_PROPERTY_IS_ATOM) ||
                                           node_property(iter_base, NODE_PROPERTY_IS_SKIP));
            inspection_position_t start_byte_offset(starting_offset_for(iter_base));
            inspection_position_t end_byte_offset(ending_offset_for(iter_base));

            if (is_valid && start_byte_offset.bytes() <= offset && end_byte_offset.bytes() >= offset)
            {
                current = iter_base;

                break;
            }
        }
    }

    if (node_property(current, NODE_PROPERTY_IS_STRUCT) ||
        node_property(current, NODE_PROPERTY_IS_ATOM) ||
        node_property(current, NODE_PROPERTY_IS_SKIP))
    {
        detail_node(current);
    }
    else
    {
        std::cerr << "Could not find details about offset " << offset << '\n';
    }

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::evaluate_expression(const command_segment_set_t& parameters)
{
    std::size_t parameter_size(parameters.size());

    if (parameter_size < 2)
    {
        std::cerr << "Usage: evaluate_expression <expression>\n";

        return false;
    }

    std::string expression_string;

    for (std::size_t i(1); i < parameter_size; ++i)
        expression_string += parameters[i] + std::string(" ");

    try
    {
        std::stringstream      input(expression_string);
        adobe::line_position_t position(__FILE__, __LINE__);
        adobe::array_t         expression;

        adobe::expression_parser(input, position).require_expression(expression);

        output_m << contextual_evaluation_of<adobe::any_regular_t>(expression,
                                                                   forest_m->begin(),
                                                                   node_m,
                                                                   input_m)
                 << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << "evaluate_expression error: " << error.what() << '\n';

        return false;
    }

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::dump_field(const command_segment_set_t& parameters)
{
    if (parameters.size() < 2)
    {
        std::cerr << "Usage: dump_field field1 <field2>\n";

        return false;
    }

    inspection_position_t start_offset;
    inspection_position_t end_offset;

    if (parameters.size() == 2)
    {
        inspection_branch_t leaf(expression_to_node(parameters[1]));
        
        start_offset = starting_offset_for(leaf);
        end_offset = ending_offset_for(leaf) + inspection_byte_k;
    }
    else if (parameters.size() == 3)
    {
        inspection_branch_t leaf1(expression_to_node(parameters[1]));
        inspection_branch_t leaf2(expression_to_node(parameters[2]));
        
        start_offset = starting_offset_for(leaf1);
        end_offset = ending_offset_for(leaf2) + inspection_byte_k;
    }
    else
    {
        std::cerr << "dump_field error: too many parameters\n";

        return false;
    }

    dump_range(start_offset.bytes(), end_offset.bytes());

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::dump_offset(const command_segment_set_t& parameters)
{
    if (parameters.size() != 3)
    {
        std::cerr << "Usage: dump_offset start_offset end_offset\n";

        return false;
    }
    
    dump_range(std::atoi(parameters[1].c_str()),
               std::atoi(parameters[2].c_str()) + 1);

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::save_field(const command_segment_set_t& parameters)
{
    if (parameters.size() < 2)
    {
        std::cerr << "Usage: save_field field1 <field2> file\n";

        return false;
    }
    
    inspection_position_t start_offset;
    inspection_position_t end_offset;
    std::string           filename;

    if (parameters.size() == 3)
    {
        inspection_branch_t leaf(expression_to_node(parameters[1]));
        
        start_offset = starting_offset_for(leaf);
        end_offset = ending_offset_for(leaf) + inspection_byte_k;

        filename = parameters[2];
    }
    else if (parameters.size() == 4)
    {
        inspection_branch_t leaf1(expression_to_node(parameters[1]));
        inspection_branch_t leaf2(expression_to_node(parameters[2]));
        
        start_offset = starting_offset_for(leaf1);
        end_offset = ending_offset_for(leaf2) + inspection_byte_k;

        filename = parameters[3];
    }
    else
    {
        std::cerr << "save_field error: too many parameters\n";

        return false;
    }

    save_range(start_offset.bytes(), end_offset.bytes(), filename);

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::usage_metrics(const command_segment_set_t& /*parameters*/)
{
    attack_vector_set_t usage_set(build_attack_vector_set(*forest_m));

    output_m << "Found " << usage_set.size() << " weak points:\n";

    for (attack_vector_set_t::const_iterator iter(usage_set.begin()), last(usage_set.end()); iter != last; ++iter)
    {
        if (iter->type_m == attack_vector_t::type_atom_usage_k)
            output_m << iter->path_m << ": " << iter->count_m << '\n';
        else if (iter->type_m == attack_vector_t::type_array_shuffle_k)
            output_m << iter->path_m << ": shuffle\n";
    }

    return true;
}

/****************************************************************************************************/

bool binspector_interface_t::find_field(const command_segment_set_t& parameters)
{
    if (parameters.size() != 2)
    {
        std::cerr << "Usage: find_field name\n";

        return false;
    }

    adobe::name_t field_name(parameters[1].c_str());

    inspection_branch_t begin(adobe::leading_of(node_m));
    inspection_branch_t end(adobe::trailing_of(node_m));

    find_field_in_range(field_name,
                        std::make_pair(const_preorder_iterator_t(begin),
                                       const_preorder_iterator_t(++end)));

    return true;
}

/****************************************************************************************************/

template <typename R>
void binspector_interface_t::find_field_in_range(adobe::name_t name, const R& f)
{
    typedef typename boost::range_iterator<R>::type iterator;

    for (iterator iter(boost::begin(f)), last(boost::end(f)); iter != last; ++iter)
        if (iter->name_m == name)
            output_m << build_path(forest_m->begin(), iter.base()) << '\n';
}

/****************************************************************************************************/

void binspector_interface_t::save_range(boost::uint64_t first, boost::uint64_t last, const std::string& filename)
{
    std::ofstream output(filename.c_str());

    if (output.fail())
    {
        std::cerr << "save_range error: '" << filename << "' failed to open for write.\n";

        return;
    }

    input_m.seek(bytepos(first));

    static const std::size_t block_size_k(1024); // 1K chunks; easy enough to adjust
    boost::uint64_t          range_size(last - first);
    boost::uint64_t          block_count(range_size / block_size_k);
    boost::uint64_t          leftovers(range_size - (block_count * block_size_k));

    rawbytes_t block_buffer(input_m.read(leftovers));

    output.write(reinterpret_cast<char*>(&block_buffer[0]),
                 static_cast<std::streamsize>(leftovers));

    for (std::size_t i(0); i < block_count; ++i)
    {
        block_buffer = input_m.read(block_size_k);
        output.write(reinterpret_cast<char*>(&block_buffer[0]), block_size_k);
    }
}

/****************************************************************************************************/

void binspector_interface_t::dump_range(boost::uint64_t first, boost::uint64_t last)
{
#if !BOOST_WINDOWS
    using std::isgraph;
#endif

    boost::uint64_t            size(last - first);
    boost::uint64_t            n(0);
    std::vector<unsigned char> char_dump;

    input_m.seek(bytepos(first));

    while (n != size)
    {
        rawbytes_t buffer(input_m.read(1));

        if (input_m.fail())
        {
            input_m.clear();
            std::cerr << "Input stream failure reading byte " << n << '\n';
            return;
        }

        unsigned char c(buffer[0]);

        output_m.width(2);
        output_m.fill('0');
        output_m << std::hex << (static_cast<unsigned int>(c) & 0xff) << std::dec;

        if (n % 4 == 3)
            output_m << ' ';

        char_dump.push_back(isgraph(c) ? c : '.');

        if (char_dump.size() == 16)
        {
            adobe::copy(char_dump, std::ostream_iterator<char>(output_m));
            char_dump.clear();
            output_m << '\n';
        }

        ++n;
    }

    if (!char_dump.empty())
    {
        while (n % 16 != 0)
        {
            if (n % 4 == 3)
                output_m << ' ';

            output_m << "  ";

            ++n;
        }

        adobe::copy(char_dump, std::ostream_iterator<unsigned char>(output_m));
        output_m << '\n';
    }
}

/****************************************************************************************************/

void binspector_interface_t::command_line()
{
    static std::vector<char> cl_buffer_s(1024, 0);

    do
    {
        command_segment_set_t command_segment_set(split_command_string(&cl_buffer_s[0]));

        if (!command_segment_set.empty())
        {
            command_map_t::const_iterator found(command_map_m.find(command_segment_set[0]));

            if (found == command_map_m.end())
                output_m << "'" << command_segment_set[0] << "' not recognized. Type '?' or 'help' for help.\n";
            else
                (this->*found->second)(command_segment_set);
        }

        path_m = build_path(forest_m->begin(), node_m);

        output_m << '$' << path_m << "$ ";
    } while (std::cin.getline(&cl_buffer_s[0], cl_buffer_s.size()));
}

/****************************************************************************************************/

std::vector<std::string> binspector_interface_t::split_command_string(const std::string& command)
{
#if !BOOST_WINDOWS
    using std::isspace;
#endif

    std::vector<std::string> result;
    std::string              segment;
    std::vector<char>        command_buffer(command.begin(), command.end());

    // faster performance to pop items off the back instead of the front
    std::reverse(command_buffer.begin(), command_buffer.end());

    while (!command_buffer.empty())
    {
        char c(command_buffer.back());

        if (isspace(c))
        {
            result.push_back(segment);
            segment = std::string();

            while (!command_buffer.empty() && isspace(command_buffer.back()))
                command_buffer.pop_back();
        }
        else
        {
            segment += c;

            command_buffer.pop_back();
        }
    }

    if (!segment.empty())
        result.push_back(segment);

    return result;
}

/****************************************************************************************************/

inspection_branch_t binspector_interface_t::expression_to_node(const std::string& expression_string)
{
    std::stringstream      input(expression_string);
    adobe::line_position_t position(__FILE__, __LINE__);
    adobe::array_t         expression;

    adobe::expression_parser(input, position).require_expression(expression);

    return contextual_evaluation_of<inspection_branch_t>(expression,
                                                         forest_m->begin(),
                                                         node_m,
                                                         input_m);
}

/****************************************************************************************************/

template <typename R>
void binspector_interface_t::print_branch_depth_range(const R& f)
{
    typedef typename boost::range_iterator<R>::type iterator;

    for (iterator iter(boost::begin(f)), last(boost::end(f)); iter != last; ++iter)
        print_node(iter, true);
}

/****************************************************************************************************/

void binspector_interface_t::print_node(inspection_branch_t branch, bool recursing, std::size_t depth)
{
    bool          is_atom(node_property(branch, NODE_PROPERTY_IS_ATOM));
    bool          is_const(node_property(branch, NODE_PROPERTY_IS_CONST));
    bool          is_struct(node_property(branch, NODE_PROPERTY_IS_STRUCT));
    bool          is_skip(node_property(branch, NODE_PROPERTY_IS_SKIP));
    bool          is_slot(node_property(branch, NODE_PROPERTY_IS_SLOT));
    adobe::name_t name(node_property(branch, NODE_PROPERTY_NAME));
    bool          is_array_element(branch->get_flag(is_array_element_k));
    bool          is_array_root(branch->get_flag(is_array_root_k));
    bool          is_noprint(is_slot ||
                             (is_const && node_value(branch, CONST_VALUE_NO_PRINT)));

    if (is_noprint)
        return;

    if (branch.edge() == adobe::forest_leading_edge)
    {
        indent_stream(output_m, depth);

        if (is_struct)
        {
            adobe::name_t struct_name(node_property(branch, STRUCT_PROPERTY_STRUCT_NAME));

            output_m << '(' << struct_name << ") ";
        }
        else if (is_atom)
        {
            atom_base_type_t      base_type(node_property(branch, ATOM_PROPERTY_BASE_TYPE));
            bool                  is_big_endian(node_property(branch, ATOM_PROPERTY_IS_BIG_ENDIAN));
            boost::uint64_t       bit_count(node_property(branch, ATOM_PROPERTY_BIT_COUNT));

            output_m << '('
                     << (base_type == atom_signed_k   ? 's' :
                         base_type == atom_unsigned_k ? 'u' :
                         base_type == atom_float_k    ? 'f' :
                                                        'X')
                     << bit_count
                     << (bit_count > 8 ? is_big_endian ? "b" : "l" : "")
                     << ") ";
        }
        else if (is_const)
        {
            output_m << "(const) ";
        }
        else if (is_skip)
        {
            output_m << "(skip) ";
        }

        if (is_array_root)
        {
            boost::uint64_t size(node_property(branch, ARRAY_ROOT_PROPERTY_SIZE));

            output_m << name << '[' << size << ']';
        }
        else if (is_array_element)
        {
            boost::uint64_t index(node_value(branch, ARRAY_ELEMENT_VALUE_INDEX));

            output_m << '[' << index << ']';
        }
        else if (is_skip)
        {
            inspection_position_t start_byte_offset(starting_offset_for(branch));
            inspection_position_t end_byte_offset(ending_offset_for(branch));
            inspection_position_t size(end_byte_offset - start_byte_offset + inspection_byte_k);

            output_m << name << '[' << size << ']';
        }
        else if (is_const)
        {
            output_m << name;
        }
        else
        {
            output_m << name;
        }

        if (!branch->summary_m.empty())
        {
            output_m << " (" << branch->summary_m << ')';
        }

        if (is_const)
        {
            output_m << ": ";

            if (node_value(branch, CONST_VALUE_IS_EVALUATED))
            {
                const adobe::any_regular_t& value(node_value(branch, CONST_VALUE_EVALUATED_VALUE));

                // any_regular_t only knows how to serialize a handful of types;
                // sadly that means we have to special-case those it cannot handle.

                if (value.type_info() == typeid(inspection_position_t))
                    output_m << value.cast<inspection_position_t>();
                else
                    output_m << value;
            }
            else
            {
                const adobe::array_t& const_expression(node_value(branch, CONST_VALUE_EXPRESSION));

                output_m << contextual_evaluation_of<adobe::any_regular_t>(const_expression,
                                                                           forest_m->begin(),
                                                                           adobe::find_parent(branch),
                                                                           input_m);
            }
        }

        if (!is_array_root && is_atom)
        {
            atom_base_type_t      base_type(node_property(branch, ATOM_PROPERTY_BASE_TYPE));
            bool                  is_big_endian(node_property(branch, ATOM_PROPERTY_IS_BIG_ENDIAN));
            boost::uint64_t       bit_count(node_property(branch, ATOM_PROPERTY_BIT_COUNT));
            inspection_position_t position(node_value(branch, ATOM_VALUE_LOCATION));

            output_m << ": ";

            stream_out(fetch_and_evaluate(input_m, position, bit_count, base_type, is_big_endian),
                       bit_count,
                       base_type,
                       output_m);
        }

        if (recursing && (is_struct || (is_array_root && node_property(branch, ARRAY_ROOT_PROPERTY_SIZE) != 0)))
        {
            output_m << '\n';
            indent_stream(output_m, depth);
            output_m << "{";
        }

        output_m << '\n';
    }
    else if (recursing && (is_struct || (is_array_root && node_property(branch, ARRAY_ROOT_PROPERTY_SIZE) != 0))) // trailing edge and recursing
    {
        indent_stream(output_m, depth);
        output_m << "}\n";
    }
}

/****************************************************************************************************/

void binspector_interface_t::print_node(depth_full_iterator_t branch, bool recursing)
{
    print_node(branch.base(), recursing, branch.depth());
}

/****************************************************************************************************/

void binspector_interface_t::detail_struct_or_array_node(inspection_branch_t struct_node)
{
    bool is_array_root(struct_node->get_flag(is_array_root_k));
    bool is_struct(node_property(struct_node, NODE_PROPERTY_IS_STRUCT));

    output_m << "     path: " << build_path(forest_m->begin(), struct_node) << '\n'
             << "     type: " << (is_array_root ? "array" : "struct") << '\n';

    if (is_struct)
        output_m << "   struct: " << node_property(struct_node, STRUCT_PROPERTY_STRUCT_NAME) << '\n';

    if (is_array_root)
        output_m << " elements: " << node_property(struct_node, ARRAY_ROOT_PROPERTY_SIZE) << '\n';

    inspection_forest_t::child_iterator first_child(adobe::child_begin(struct_node));
    inspection_forest_t::child_iterator last_child(adobe::child_end(struct_node));

    if (first_child == last_child)
        return;

    inspection_position_t start_byte_offset(starting_offset_for(struct_node));
    inspection_position_t end_byte_offset(ending_offset_for(struct_node));
    inspection_position_t size(end_byte_offset - start_byte_offset + inspection_byte_k);
    boost::uint64_t       bytes(size.bytes());

    output_m << "    bytes: [ " << start_byte_offset << " .. " << end_byte_offset << " ]\n"
             << "     size: " << bytes << " bytes";

    if (bytes >= 1024)
        output_m << " (" << bytes / 1024.0 << " KB)";

    if (bytes >= 1024 * 1024)
        output_m << " (" << bytes / (1024.0 * 1024) << " MB)";

    if (bytes >= 1024 * 1024 * 1024)
        output_m << " (" << bytes / (1024.0 * 1024 * 1024) << " GB)";

    output_m << '\n';
}

/****************************************************************************************************/

void binspector_interface_t::detail_atom_node(inspection_branch_t atom_node)
{
    atom_base_type_t      base_type(node_property(atom_node, ATOM_PROPERTY_BASE_TYPE));
    bool                  is_big_endian(node_property(atom_node, ATOM_PROPERTY_IS_BIG_ENDIAN));
    boost::uint64_t       bit_count(node_property(atom_node, ATOM_PROPERTY_BIT_COUNT));
    inspection_position_t position(node_value(atom_node, ATOM_VALUE_LOCATION));
    rawbytes_t            raw(input_m.read_bits(position, bit_count));
    adobe::any_regular_t  value(evaluate(raw, bit_count, base_type, is_big_endian));

    output_m << "     path: " << build_path(forest_m->begin(), atom_node) << '\n'
             << "   format: " << bit_count << "-bit "
                              << (base_type == atom_signed_k   ? "signed" :
                                  base_type == atom_unsigned_k ? "unsigned" :
                                  base_type == atom_float_k    ? "float" :
                                                                 "<ERR>") << ' '
                              << (bit_count > 8 ? is_big_endian ? "big-endian" : "little-endian" : "")
                              << '\n'
             << "   offset: " << position;

    output_m << '\n';

    output_m << "      raw: " << std::hex;
    for (std::size_t i(0); i < raw.size(); ++i)
    {
        output_m << "0x";
        output_m.width(2);
        output_m.fill('0');
        output_m << static_cast<int>(raw[i]) << ' ';
    }
    output_m << std::dec << '\n';

    output_m << "    value: " << std::dec;
    stream_out(value, bit_count, base_type, output_m);
    output_m << " (0x" << std::hex;
    stream_out(value, bit_count, base_type, output_m);
    output_m << ")\n" << std::dec;
}

/****************************************************************************************************/

void binspector_interface_t::detail_skip(inspection_branch_t skip_node)
{
    output_m << "     path: " << build_path(forest_m->begin(), skip_node) << '\n'
             << "     type: skip\n";

    inspection_position_t start_byte_offset(starting_offset_for(skip_node));
    inspection_position_t end_byte_offset(ending_offset_for(skip_node));
    inspection_position_t size(end_byte_offset - start_byte_offset + inspection_byte_k);

    output_m << "    bytes: [ " << start_byte_offset << " .. " << end_byte_offset << " ]\n";
    output_m << "     size: " << size.bytes() << " bytes";

    output_m << '\n';
}

/****************************************************************************************************/

void binspector_interface_t::detail_node(inspection_branch_t node)
{
    bool is_struct(node_property(node, NODE_PROPERTY_IS_STRUCT));
    bool is_skip(node_property(node, NODE_PROPERTY_IS_SKIP));
    bool is_array_root(node->get_flag(is_array_root_k));

    if (is_struct || is_array_root)
    {
        detail_struct_or_array_node(node);
    }
    else if (is_skip)
    {
        detail_skip(node);
    }
    else // atom (either singleton or array element)
    {
        detail_atom_node(node);
    }
}

/****************************************************************************************************/
