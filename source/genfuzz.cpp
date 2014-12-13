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
#include <binspector/string.hpp>

/**************************************************************************************************/

namespace {

/**************************************************************************************************/

enum enum_mode_t
{
    backwrite_k,
    immutable_k
};

/**************************************************************************************************/

std::size_t child_size(const_inspection_branch_t node)
{
    return std::distance(adobe::child_begin(node), adobe::child_end(node));
}

/**************************************************************************************************/

bitreader_t& dummy_bitreader()
{
    static std::ifstream dummy_stream_s("file_that_does_not_exist");
    static bitreader_t   value_s(dummy_stream_s);

    return value_s;
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

struct bitwriter_t
{
    explicit bitwriter_t(const boost::filesystem::path& file) :
        output_m(file)
    { }

    ~bitwriter_t()
    {
        output_m.write(reinterpret_cast<const char*>(&buffer_m[0]), buffer_m.size());
    }

    void write(node_t& node)
    {
        if (node.bit_count_m % 8)
            throw std::runtime_error("non-byte aligned writing not supported.");

        // Record the location of the node's information in the file. For backwriting.
        node.location_m = bytepos(buffer_m.size());

        // It'd be great if we could emplace the whole vector onto the back of the buffer...
        for (const auto& byte : disintegrate_value(node.evaluated_value_m,
                                                   node.bit_count_m,
                                                   node.type_m,
                                                   node.get_flag(atom_is_big_endian_k)))
            buffer_m.push_back(byte);
    }

    void backwrite(const node_t& node)
    {
#if 0
        std::cerr << "backwrite " << node.name_m.c_str()
                  << " to " << serialize(node.evaluated_value_m)
                  << '\n';
#endif
        std::size_t offset(node.location_m.bytes());

        for (const auto& byte : disintegrate_value(node.evaluated_value_m,
                                                   node.bit_count_m,
                                                   node.type_m,
                                                   node.get_flag(atom_is_big_endian_k)))
            buffer_m[offset++] = byte;
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

    void write(node_t& node);
    void backwrite(const node_t& node);

    void fuzz_atom(node_t& node);

    template <typename T>
    T resolve(inspection_branch_t node);

    template <typename T>
    T resolve_expression(const adobe::array_t& expression);

    template <typename T>
    T resolve_expression(const adobe::dictionary_t& field,
                         adobe::name_t              key);

    std::string build_path(const_inspection_branch_t branch)
    {
        return ::build_path(forest_m.begin(), branch);
    }

    const ast_t&         ast_m;
    std::mt19937_64      rnd_m;

    // generation state and maintenance
    inspection_forest_t   forest_m;
    inspection_branch_t   current_leaf_m;
    typedef_map_t         current_typedef_map_m;

    // there are two enumeration modes, depending on whether or not we have a
    // value already picked out, or we need to choose one of the enumerations at
    // random. The former 'regular' case is the same path executed by the
    // analyzer, while the new 'backwriting' mode is unique to the generative
    // fuzzer.
    enum_mode_t           current_enumerated_mode_m;  // regular v. backwriting mode
    adobe::any_regular_t  current_enumerated_value_m; // regular: the value we're looking for
    inspection_branch_t   current_enumerated_atom_m;  // backwriting: the atom to backwrite
    std::size_t           current_enumerated_pick_m;  // backwriting: enumeration option we're choosing
    std::size_t           current_enumerated_index_m; // backwriting: current enum index before to find the pick
    bool                  current_enumerated_found_m; // both: set to true when we've found our pick

    // file i/o
    boost::filesystem::path     base_output_path_m;
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
    current_enumerated_mode_m(immutable_k),
    current_enumerated_pick_m(0),
    current_enumerated_index_m(0),
    current_enumerated_found_m(false),
    base_output_path_m(get_base_output_path(output_root)),
    basename_m("test"),
    extension_m("fixme.txt"),
    output_m(base_output_path_m / (basename_m + "_genfuzz." + extension_m))
{ }

/**************************************************************************************************/

template <typename T>
T genfuzz_t::resolve(inspection_branch_t node)
{
    return finalize_lookup<T>(forest_m.begin(), node, dummy_bitreader(), true);
}

template <typename T>
T genfuzz_t::resolve_expression(const adobe::array_t& expression)
{
    return contextual_evaluation_of<T>(expression, forest_m.begin(), current_leaf_m, dummy_bitreader());
}

// specialization because double doesn't implicitly convert to boost::uint64_t
template <>
boost::uint64_t genfuzz_t::resolve_expression<boost::uint64_t>(const adobe::array_t& expression)
{
    // static_cast is tossing bits...
    return static_cast<boost::uint64_t>(resolve_expression<double>(expression));
}

template <typename T>
T genfuzz_t::resolve_expression(const adobe::dictionary_t& field,
                                adobe::name_t              key)
{
    return resolve_expression<T>(value_for<adobe::array_t>(field, key));
}

/**************************************************************************************************/

void genfuzz_t::generate()
{
    if (!is_directory(base_output_path_m))
        throw std::runtime_error(make_string("Output directory failure: ",
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

void genfuzz_t::write(node_t& node)
{
    output_m.write(node);
}

/**************************************************************************************************/

void genfuzz_t::backwrite(const node_t& node)
{
    output_m.backwrite(node);
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

        if (type == value_field_type_enumerated)
        {
            inspection_branch_t  atom(resolve_expression<inspection_branch_t>(field, key_enumerated_expression));
            enum_mode_t          mode(atom->get_flag(genfuzz_immutable_k) ? immutable_k : backwrite_k);
            adobe::any_regular_t value(resolve<adobe::any_regular_t>(atom));

            temp_assignment<enum_mode_t>          enumerated_mode(current_enumerated_mode_m, mode);
            temp_assignment<adobe::any_regular_t> enumerated_value(current_enumerated_value_m, value);
            temp_assignment<inspection_branch_t>  enumerated_atom(current_enumerated_atom_m, atom);
            temp_assignment<std::size_t>          enumerated_pick(current_enumerated_pick_m, rnd_m() % child_size(current_leaf_m));
            temp_assignment<std::size_t>          enumerated_index(current_enumerated_index_m, 0);
            temp_assignment<bool>                 enumerated_found(current_enumerated_found_m, false);

            if (current_enumerated_mode_m == backwrite_k)
                std::cerr << "picking enum index " << current_enumerated_pick_m << '\n';

            generate_substructure(field, parent);

            if (!current_enumerated_found_m)
                throw std::runtime_error(make_string("value for ",
                                                     build_path(atom),
                                                     " is not enumerated (",
                                                     serialize(value),
                                                     ")"));

            continue;
        }
        else if (type == value_field_type_enumerated_option)
        {
            if (current_enumerated_mode_m == immutable_k)
            {
                adobe::any_regular_t value(resolve_expression<adobe::any_regular_t>(field, key_enumerated_option_expression));

                std::cerr << "comparing " << serialize(value) << " to " << serialize(current_enumerated_value_m) << '\n';

                if (value == current_enumerated_value_m)
                {
                    generate_substructure(field, parent);

                    current_enumerated_found_m = true;
                }
            }
            else // (current_enumerated_mode_m == backwrite_k)
            {
                if (current_enumerated_pick_m == current_enumerated_index_m)
                {
                    // go back into the file in question and modify the bits,
                    // now that we know what they will be.
                    current_enumerated_atom_m->set_flag(genfuzz_immutable_k);
                    current_enumerated_atom_m->evaluated_m = true;
                    current_enumerated_atom_m->evaluated_value_m = resolve_expression<adobe::any_regular_t>(field, key_enumerated_option_expression);

                    backwrite(*current_enumerated_atom_m);

                    generate_substructure(field, parent);

                    current_enumerated_found_m = true;
                }
                else
                {
                    ++current_enumerated_index_m;
                }
            }

            // if we have found our enumerated option we are DONE done; no continue.
            if (current_enumerated_found_m)
                return;

            continue;
        }
        else if (type == value_field_type_enumerated_default)
        {
            generate_substructure(field, parent);

            current_enumerated_found_m = true;

            // Since we have hit the default we are DONE done; no continue.
            return;
        }

        // !!!!! NOTICE !!!!!
        //
        // From this point on we've actually added a node to the genfuzz forest.

        inspection_branch_t sub_branch(new_branch(parent));
        forest_node_t&      branch_data(*sub_branch);

        temp_assignment<inspection_branch_t> node_stack(current_leaf_m, sub_branch);

        branch_data.name_m = name;

        if (type == value_field_type_struct)
            branch_data.set_flag(type_struct_k);
        else if (type == value_field_type_atom)
            branch_data.set_flag(type_atom_k);
        else if (type == value_field_type_const)
            branch_data.set_flag(type_const_k);
        else if (type == value_field_type_skip)
            branch_data.set_flag(type_skip_k);
        else if (type == value_field_type_slot)
            branch_data.set_flag(type_slot_k);
        else
            throw std::runtime_error(make_string("analysis error: unknown type: ",
                                                 type.c_str()));

        if (type == value_field_type_atom)
        {
            const adobe::array_t& bit_count_expression(value_for<adobe::array_t>(field, key_atom_bit_count_expression));
            const adobe::array_t& is_big_endian_expression(value_for<adobe::array_t>(field, key_atom_is_big_endian_expression));
            bool                  is_big_endian(resolve_expression<bool>(is_big_endian_expression));

            branch_data.bit_count_m = static_cast<boost::uint64_t>(resolve_expression<double>(bit_count_expression));
            branch_data.type_m = value_for<atom_base_type_t>(field, key_atom_base_type);
            branch_data.set_flag(atom_is_big_endian_k, is_big_endian);

            const adobe::array_t& atom_invariant_expression(value_for<adobe::array_t>(field, key_field_atom_invariant_expression));

            if (!atom_invariant_expression.empty())
            {
                std::vector<double> atom_invariant_set(homogeneous_regular_cast<double>(resolve_expression<adobe::any_regular_t>(atom_invariant_expression)));

                // pick one!
                branch_data.evaluated_m = true;
                branch_data.evaluated_value_m.assign(atom_invariant_set[rnd_m() % atom_invariant_set.size()]);
                branch_data.set_flag(genfuzz_immutable_k);
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
