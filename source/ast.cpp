/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

// identity
#include <binspector/ast.hpp>

// boost
#include <boost/bind.hpp>

// asl
#include <adobe/algorithm/copy.hpp>
#include <adobe/dictionary_set.hpp>
#include <adobe/implementation/token.hpp>
#include <adobe/string.hpp>

// application
#include <binspector/parser.hpp>

/**************************************************************************************************/

namespace {

/**************************************************************************************************/

bool has_field_named(const adobe::array_t& structure,
                     adobe::name_t         field)
{
    for (const auto& regular_field : structure)
    {
        const adobe::dictionary_t&          cur_field(regular_field.cast<adobe::dictionary_t>());
        adobe::dictionary_t::const_iterator field_name(cur_field.find(key_field_name));

        if (field_name != cur_field.end() &&
            field_name->second.cast<adobe::name_t>() == field)
            return true;
    }

    return false;
}

void check_duplicate_field_name(const structure_type& current_structure, adobe::name_t name)
{
    if (!has_field_named(current_structure, name))
        return;

    throw std::runtime_error(adobe::make_string("Duplicate field name '",
                                                name.c_str(),
                                                "'"));
}

/**************************************************************************************************/

} // namespace

/**************************************************************************************************/
#if 0
#pragma mark -
#endif
/**************************************************************************************************/

ast_t::ast_t() :
    current_structure_m(nullptr)
{
}

/**************************************************************************************************/

void ast_t::set_current_structure(adobe::name_t structure_name)
{
    current_structure_m = &structure_map_m[structure_name];
}

/**************************************************************************************************/

void ast_t::add_named_field(adobe::name_t              name,
                            const adobe::dictionary_t& parameters)
{
    if (current_structure_m == 0)
        return;

    structure_type& current_structure(*current_structure_m);

    check_duplicate_field_name(current_structure, name);

    current_structure.push_back(adobe::any_regular_t(parameters));
}

/**************************************************************************************************/

void ast_t::add_unnamed_field(const adobe::dictionary_t& parameters)
{
    if (current_structure_m == 0)
        return;

    structure_type& current_structure(*current_structure_m);

    current_structure.push_back(adobe::any_regular_t(parameters));
}

/**************************************************************************************************/

void ast_t::add_typedef(adobe::name_t              /*typedef_name*/,
                        const adobe::dictionary_t& typedef_parameters)
{
    if (current_structure_m == 0)
        return;

    structure_type& current_structure(*current_structure_m);

    // we'll need some kind of check_duplicate_type_name
    // check_duplicate_field_name(current_structure, typedef_name);

    current_structure.push_back(adobe::any_regular_t(typedef_parameters));
}

/**************************************************************************************************/

const structure_type& ast_t::structure_for(adobe::name_t structure_name) const
{
    auto structure(structure_map_m.find(structure_name));

    if (structure == end(structure_map_m))
        throw std::runtime_error(adobe::make_string("Could not find structure '", structure_name.c_str(), "'"));

    return structure->second;
}

/**************************************************************************************************/

ast_t make_ast(const boost::filesystem::path& template_path,
               path_set                       include_path_set)
{
    ast_t result;

    // Open the template file, if we can.
    if (!exists(template_path))
    {
        std::string error("Template file ");

        // REVISIT (fbrereto): performance on these concats
        if (template_path.empty())
            error += "unspecified";
        else
            error += "'"
                  + template_path.string()
                  + "' could not be located or does not exist.";

        throw std::runtime_error(error);
    }

    boost::filesystem::ifstream template_description(template_path);

    if (!template_description)
        throw std::runtime_error("Could not open template file");

    try
    {
        adobe::line_position_t::getline_proc_t getline(new adobe::line_position_t::getline_proc_impl_t(boost::bind(&get_input_line, boost::ref(template_description), _2)));

        include_path_set.insert(include_path_set.begin(), template_path.parent_path());

        binspector_parser_t(template_description,
                            adobe::line_position_t(adobe::name_t(template_path.string().c_str()), getline),
                            include_path_set,
                            boost::bind(&ast_t::set_current_structure, boost::ref(result), _1),
                            boost::bind(&ast_t::add_named_field, boost::ref(result), _1, _2),
                            boost::bind(&ast_t::add_unnamed_field, boost::ref(result), _1),
                            boost::bind(&ast_t::add_typedef, boost::ref(result), _1, _2)).parse();
    }
    catch (const adobe::stream_error_t& error)
    {
        throw std::runtime_error(adobe::format_stream_error(template_description, error));
    }

    // once the parse is done we don't need the main template file anymore.
    template_description.close();

    return result;
}

/**************************************************************************************************/
