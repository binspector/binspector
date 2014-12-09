/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#ifndef BINSPECTOR_INTERFACE_HPP
#define BINSPECTOR_INTERFACE_HPP

// stdc++
#include <iostream>
#include <map>
#include <stdexcept>

// application
#include <binspector/common.hpp>

/**************************************************************************************************/

class user_quit_exception_t : public std::exception
{
    virtual const char* what() const throw() { return "user quit"; }
};

/**************************************************************************************************/

class binspector_interface_t
{
public:
    binspector_interface_t(std::istream& binary_file,
                     auto_forest_t forest,
                     std::ostream& output);

    void command_line();

    typedef std::vector<std::string>                 command_segment_set_t;
    typedef bool (binspector_interface_t::*command_proc_t)(const command_segment_set_t&);
    typedef std::map<std::string, command_proc_t>    command_map_t;

    // commands
    bool quit(const command_segment_set_t&);
    bool help(const command_segment_set_t&);
    bool print_branch(const command_segment_set_t& = command_segment_set_t());
    bool print_structure(const command_segment_set_t& = command_segment_set_t());
    bool print_string(const command_segment_set_t&);
    bool step_in(const command_segment_set_t&);
    bool step_out(const command_segment_set_t&);
    bool top(const command_segment_set_t&);
    bool detail_field(const command_segment_set_t&);
    bool detail_offset(const command_segment_set_t&);
    bool evaluate_expression(const command_segment_set_t&);
    bool dump_field(const command_segment_set_t&);
    bool dump_offset(const command_segment_set_t&);
    bool save_field(const command_segment_set_t&);
    bool usage_metrics(const command_segment_set_t& = command_segment_set_t());
    bool find_field(const command_segment_set_t&);

    // cl processing helpers
    command_segment_set_t split_command_string(const std::string& command);
    inspection_branch_t   expression_to_node(const std::string& expression_string);

    // output helpers
    template <typename R>
    void print_branch_depth_range(const R& f);
    template <typename R>
    void find_field_in_range(adobe::name_t name, const R& f);
    void print_node(depth_full_iterator_t branch, bool recursing);
    void print_node(inspection_branch_t branch, bool recursing, std::size_t depth = 1);
    void print_structure_children(inspection_branch_t node);
    void detail_struct_or_array_node(inspection_branch_t struct_node);
    void detail_atom_node(inspection_branch_t atom_node);
    void detail_skip(inspection_branch_t skip_node);
    void detail_node(inspection_branch_t node);
    void dump_range(boost::uint64_t first, boost::uint64_t last);
    void save_range(boost::uint64_t first, boost::uint64_t last, const std::string& filename);

    bitreader_t   input_m;
    auto_forest_t forest_m;
    std::ostream& output_m;

    inspection_branch_t node_m;
    std::string         path_m;
    command_map_t       command_map_m;
};

/**************************************************************************************************/
// BINSPECTOR_INTERFACE_HPP
#endif

/**************************************************************************************************/
