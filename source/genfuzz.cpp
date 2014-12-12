/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

// stdc++
#include <iostream>
#include <fstream>
#include <random>

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

struct bitwriter_t
{
    explicit bitwriter_t(const boost::filesystem::path& file) :
        output_m(file)
    { }

    ~bitwriter_t()
    {
        output_m.write(reinterpret_cast<const char*>(&buffer_m[0]), buffer_m.size());
    }

    void write(const node_t& node)
    {
        if (node.bit_count_m % 8)
            throw std::runtime_error("non-byte aligned writing not supported.");

        // It'd be great if we could emplace the whole vector onto the back of the buffer...
        for (const auto& byte : disintegrate_value(node.evaluated_value_m,
                                                   node.bit_count_m,
                                                   node.type_m,
                                                   node.get_flag(atom_is_big_endian_k)))
            buffer_m.push_back(byte);
    }

    std::vector<std::uint8_t>   buffer_m;
    boost::filesystem::ofstream output_m;
};

/**************************************************************************************************/
// I have a sneaking suspicion this will start to look a lot like the analyzer.
struct genfuzz_t
{
    genfuzz_t(const ast_t&                   ast,
              const boost::filesystem::path& output_root,
              std::mt19937_64::result_type   seed = std::mt19937_64::default_seed);

    void generate();

private:
    inspection_branch_t new_branch(inspection_branch_t with_parent);

    void generate_substructure(adobe::name_t       name,
                               inspection_branch_t parent);
    void generate_substructure(const adobe::dictionary_t& field,
                               inspection_branch_t        parent);

    void generate_structure(const structure_type& structure,
                            inspection_branch_t   parent);

    template <typename T>
    T evaluate(const adobe::array_t& expression);

    void write(const node_t& node);

    void fuzz_atom(node_t& node);

    const ast_t&         ast_m;
    std::mt19937_64      rnd_m;

    // generation state and maintenance
    inspection_forest_t         forest_m;
    inspection_branch_t         current_leaf_m;
    typedef_map_t               current_typedef_map_m;

    // file i/o
    boost::filesystem::path     base_output_path_m; // REVISIT: should be passed in
    std::string                 basename_m; // REVISIT: should be passed in
    std::string                 extension_m; // REVISIT: should be passed in
    bitwriter_t                 output_m; // output file
};

/**************************************************************************************************/

genfuzz_t::genfuzz_t(const ast_t&                   ast,
                     const boost::filesystem::path& output_root,
                     std::mt19937_64::result_type   seed) :
    ast_m(ast),
    rnd_m(seed),
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

template <typename T>
T genfuzz_t::evaluate(const adobe::array_t& expression)
{
    static std::ifstream dummy_stream("dummy.bin");
    static bitreader_t   dummy_bitreader(dummy_stream);

    return contextual_evaluation_of<T>(expression,
                                       forest_m.begin(),
                                       current_leaf_m,
                                       dummy_bitreader);    
}

// specialization because double doesn't always implicitly convert to boost::uint64_t
template <>
boost::uint64_t genfuzz_t::evaluate(const adobe::array_t& expression)
{
    return static_cast<boost::uint64_t>(evaluate<double>(expression));
}

/**************************************************************************************************/

void genfuzz_t::write(const node_t& node)
{
    output_m.write(node);
}

/**************************************************************************************************/

void genfuzz_t::fuzz_atom(node_t& node)
{
    std::uint64_t      mask(static_cast<std::uint64_t>(-1) >> (64 - node.bit_count_m));
    std::uint_fast64_t bits(rnd_m());
    std::uint64_t      value(bits & mask);

    // static_cast is tossing bits...
    node.evaluated_value_m.assign(static_cast<double>(value));
    node.evaluated_m = true;
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

        std::cerr << type << " -- " << name << '\n';

        // !!!!! NOTICE !!!!!
        //
        // From this point on we've actually added a node to the genfuzz forest.

        inspection_branch_t sub_branch(new_branch(parent));
        forest_node_t&      branch_data(*sub_branch);

        temp_assignment<inspection_branch_t> node_stack(current_leaf_m, sub_branch);

        branch_data.name_m = name;

        if (type == value_field_type_atom)
        {
            const adobe::array_t& bit_count_expression(value_for<adobe::array_t>(field, key_atom_bit_count_expression));
            const adobe::array_t& is_big_endian_expression(value_for<adobe::array_t>(field, key_atom_is_big_endian_expression));
            bool                  is_big_endian(evaluate<bool>(is_big_endian_expression));

            branch_data.bit_count_m = static_cast<boost::uint64_t>(evaluate<double>(bit_count_expression));
            branch_data.type_m = value_for<atom_base_type_t>(field, key_atom_base_type);
            branch_data.set_flag(atom_is_big_endian_k, is_big_endian);

            const adobe::array_t& atom_invariant_expression(value_for<adobe::array_t>(field, key_field_atom_invariant_expression));

            if (!atom_invariant_expression.empty())
            {
                branch_data.evaluated_m = true;

                std::vector<double> atom_invariant_set(homogeneous_regular_cast<double>(evaluate<adobe::any_regular_t>(atom_invariant_expression)));

                // pick one!
                branch_data.evaluated_value_m.assign(atom_invariant_set[rnd_m() % atom_invariant_set.size()]);
            }
            else
            {
                // make up something!
                fuzz_atom(branch_data);
            }

            write(branch_data);
        }
        else if (type == value_field_type_struct)
        {
            adobe::name_t struct_name(value_for<adobe::name_t>(field, key_named_type_name));
    
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
