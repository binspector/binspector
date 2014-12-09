/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

// stdc++
#include <iostream>
#include <fstream>

// boost
#include <boost/filesystem.hpp>

// asl
#include <adobe/string.hpp>

// application
#include <binspector/analyzer.hpp>
#include <binspector/forest.hpp>

/**************************************************************************************************/

namespace {

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
// I have a sneaking suspicion this will start to look a lot like the analyzer.
struct genfuzz_t
{
    genfuzz_t(const ast_t&                   ast,
              const boost::filesystem::path& output_root);

    void generate();

private:
    inspection_branch_t new_branch(inspection_branch_t with_parent);

    void generate_substructure(adobe::name_t       name,
                               inspection_branch_t parent);
    void generate_substructure(const adobe::dictionary_t& field,
                               inspection_branch_t        parent);

    void generate_structure(const structure_type& structure,
                            inspection_branch_t   parent);

    const ast_t&                ast_m;

    // generation state and maintenance
    inspection_forest_t         forest_m;
    inspection_branch_t         current_leaf_m;
    typedef_map_t               current_typedef_map_m;

    // file i/o
    boost::filesystem::path     base_output_path_m; // REVISIT: should be passed in
    std::string                 basename_m; // REVISIT: should be passed in
    std::string                 extension_m; // REVISIT: should be passed in
    boost::filesystem::ofstream output_m; // summary output
};

/**************************************************************************************************/

genfuzz_t::genfuzz_t(const ast_t&                   ast,
                     const boost::filesystem::path& output_root) :
    ast_m(ast),
    base_output_path_m(get_base_output_path(output_root)),
    basename_m("test"),
    extension_m("fixme.txt"),
    output_m(base_output_path_m / (basename_m + "_genfuzz." + extension_m))
{ }

/**************************************************************************************************/

void genfuzz_t::generate()
{
    if (!is_directory(base_output_path_m))
        throw std::runtime_error(adobe::make_string("Output directory failure: ",
                                                    base_output_path_m.string().c_str()));

    forest_m.clear();
    current_typedef_map_m.clear();

    inspection_branch_t  branch(new_branch(forest_m.begin()));
    forest_node_t&       branch_data(*branch);
    adobe::name_t        starting_struct_name("main");

    branch_data.name_m = value_main;
    branch_data.struct_name_m = starting_struct_name;
    branch_data.set_flag(type_struct_k);

    return generate_substructure(starting_struct_name, branch);
}

/**************************************************************************************************/

inspection_branch_t genfuzz_t::new_branch(inspection_branch_t with_parent)
{
    with_parent.edge() = adobe::forest_trailing_edge;

    return forest_m.insert(with_parent, forest_node_t());
}

/**************************************************************************************************/

void genfuzz_t::generate_substructure(adobe::name_t       name,
                                      inspection_branch_t parent)
{
    generate_structure(ast_m.structure_for(name), parent);
}

/**************************************************************************************************/

void genfuzz_t::generate_substructure(const adobe::dictionary_t& field,
                                      inspection_branch_t        parent)
{
    generate_substructure(get_value(field, key_named_type_name).cast<adobe::name_t>(),
                          parent);
}

/**************************************************************************************************/

void genfuzz_t::generate_structure(const structure_type& structure,
                                   inspection_branch_t   parent)
{
    temp_assignment<inspection_branch_t> node_stack(current_leaf_m, parent);
    save_restore<typedef_map_t>          typedef_map_stack(current_typedef_map_m);
    // bool                                 last_conditional_value(false);

    for (const auto& regular_field : structure)
    {
        adobe::dictionary_t field(regular_field.cast<adobe::dictionary_t>());
        adobe::name_t       type(value_for<adobe::name_t>(field, key_field_type));
        adobe::name_t       name(value_for<adobe::name_t>(field, key_field_name));

        if (type == value_field_type_typedef_atom ||
            type == value_field_type_typedef_named)
        {
            // add the typedef's field details to the typedef map; we're done
            current_typedef_map_m[name] = field;

            continue;
        }

        // typedef resolution, if applicable.
        if (type == value_field_type_named)
        {
            field = typedef_lookup(current_typedef_map_m, field);

            // re-set these values given that the field may have changed.
            type = value_for<adobe::name_t>(field, key_field_type);
            name = value_for<adobe::name_t>(field, key_field_name);
        }

        output_m << type << " -- " << name << '\n';

        if (type == value_field_type_struct)
        {
            inspection_branch_t                  sub_branch(new_branch(parent));
            temp_assignment<inspection_branch_t> node_stack(current_leaf_m, sub_branch);
            forest_node_t&                       branch_data(*sub_branch);
            adobe::name_t                        struct_name(value_for<adobe::name_t>(field, key_named_type_name));
    
            branch_data.name_m = name;
            branch_data.set_flag(type_struct_k);
            branch_data.struct_name_m = struct_name;

            generate_substructure(struct_name, sub_branch);
        }
    }
}

/**************************************************************************************************/

} // namespace

/**************************************************************************************************/

void genfuzz(const ast_t&                   ast,
             const boost::filesystem::path& output_root)
{
    genfuzz_t(ast, output_root).generate();
}

/**************************************************************************************************/
