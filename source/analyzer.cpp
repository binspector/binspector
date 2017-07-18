/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// identity
#include <binspector/analyzer.hpp>

// asl
#include <adobe/algorithm/copy.hpp>
#include <adobe/dictionary_set.hpp>
#include <adobe/implementation/token.hpp>
#include <adobe/string.hpp>

/****************************************************************************************************/

namespace {

/****************************************************************************************************/

template <typename T>
const T& value_for(const adobe::dictionary_t& dict, adobe::name_t key) {
    adobe::dictionary_t::const_iterator found(dict.find(key));

    if (found == dict.end())
        throw std::runtime_error(adobe::make_string("Key ", key.c_str(), " not found"));

    return found->second.cast<T>();
}

/****************************************************************************************************/

template <typename T>
const T& value_for(const adobe::dictionary_t& dict, adobe::name_t key, const T& default_value) {
    adobe::dictionary_t::const_iterator found(dict.find(key));

    return found == dict.end() ? default_value : found->second.cast<T>();
}

/****************************************************************************************************/

void throw_duplicate_field_name(adobe::name_t name) {
    throw std::runtime_error(adobe::make_string("Duplicate field name '", name.c_str(), "'"));
}

bool has_field_named(const adobe::array_t& structure, adobe::name_t field) {
    for (adobe::array_t::const_iterator iter(structure.begin()), last(structure.end());
         iter != last;
         ++iter) {
        const adobe::dictionary_t&          cur_field(iter->cast<adobe::dictionary_t>());
        adobe::dictionary_t::const_iterator field_name(cur_field.find(key_field_name));

        if (field_name != cur_field.end() && field_name->second.cast<adobe::name_t>() == field)
            return true;
    }

    return false;
}

void check_duplicate_field_name(const binspector_analyzer_t::structure_type& current_structure,
                                adobe::name_t                                name) {
    if (has_field_named(current_structure, name))
        throw_duplicate_field_name(name);
}

/****************************************************************************************************/

inline void transfer_field(adobe::dictionary_t&       dst,
                           const adobe::dictionary_t& src,
                           adobe::name_t              key) {
    adobe::dictionary_t::const_iterator found(src.find(key));

    if (found == src.end())
        throw std::runtime_error("Key transfer failed");

    dst[key].assign(found->second);
}

/****************************************************************************************************/
// any_regular's serialization has always been... funny. Gotta find a better
// way to get values serialized than this kind of glue.
inline std::string serialize(const adobe::any_regular_t& x) {
    std::stringstream ugh;

    ugh << x;

    return ugh.str();
}

/****************************************************************************************************/

// return the position of the highest bit set in the value
std::size_t highest_bit_for(boost::uint64_t value) {
    std::size_t result(63);

    while (result) {
        if ((value >> result) & 0x1)
            break;

        --result;
    }

    return result;
}

/****************************************************************************************************/

std::size_t highest_byte_for(boost::uint64_t value) {
    std::size_t highest_bit(highest_bit_for(value));
    std::size_t highest_byte(highest_bit / 8);

    if (highest_bit % 8)
        ++highest_byte;

    return highest_byte;
}

/****************************************************************************************************/

} // namespace

/****************************************************************************************************/
#if 0
#pragma mark -
#endif
/****************************************************************************************************/

binspector_analyzer_t::binspector_analyzer_t(std::istream& binary_file,
                                             std::ostream& output,
                                             std::ostream& error)
    : input_m(binary_file), output_m(output), error_m(error), current_structure_m(0),
      current_sentry_m(invalid_position_k), forest_m(new inspection_forest_t),
      current_enumerated_found_m(false), eof_signalled_m(false), quiet_m(false),
      last_line_number_m(0) {}

/****************************************************************************************************/

void binspector_analyzer_t::set_current_structure(adobe::name_t structure_name) {
    current_structure_m = &structure_map_m[structure_name];
}

/****************************************************************************************************/

void binspector_analyzer_t::add_named_field(adobe::name_t              name,
                                            const adobe::dictionary_t& parameters) {
    if (current_structure_m == 0)
        return;

    structure_type& current_structure(*current_structure_m);
    adobe::name_t   type(value_for<adobe::name_t>(parameters, key_field_type));

    // Signals are declarations that don't produce a field, so the fact there
    // could be a field with the same name as the signal is ok. (We may want to
    // consider doing something similar for invariant names, too, to relax the
    // restriction that their names be unique.)
    if (type != value_field_type_signal)
        check_duplicate_field_name(current_structure, name);

    current_structure.push_back(adobe::any_regular_t(parameters));
}

/****************************************************************************************************/

void binspector_analyzer_t::add_unnamed_field(const adobe::dictionary_t& parameters) {
    if (current_structure_m == 0)
        return;

    structure_type& current_structure(*current_structure_m);

    current_structure.push_back(adobe::any_regular_t(parameters));
}

/****************************************************************************************************/

void binspector_analyzer_t::add_typedef(adobe::name_t /*typedef_name*/,
                                        const adobe::dictionary_t& typedef_parameters) {
    if (current_structure_m == 0)
        return;

    structure_type& current_structure(*current_structure_m);

    // we'll need some kind of check_duplicate_type_name
    // check_duplicate_field_name(current_structure, typedef_name);

    current_structure.push_back(adobe::any_regular_t(typedef_parameters));
}

/****************************************************************************************************/

bool binspector_analyzer_t::jump_into_structure(adobe::name_t       structure_name,
                                                inspection_branch_t parent) {
    bool result = analyze_with_structure(structure_for(structure_name), parent);

#if 0
    if (!adobe::has_children(parent))
    {
        std::cerr << "node with zero kids: " << parent->name_m << '\n';
    }
#endif

    return result;
}

/****************************************************************************************************/

bool binspector_analyzer_t::jump_into_structure(const adobe::dictionary_t& field,
                                                inspection_branch_t        parent) {
    return jump_into_structure(get_value(field, key_named_type_name).cast<adobe::name_t>(), parent);
}

/****************************************************************************************************/

bool binspector_analyzer_t::analyze_binary(const std::string& starting_struct) {
    input_m.seek(bitreader_t::pos_t());
    forest_m->clear();
    current_typedef_map_m.clear();

    inspection_branch_t branch(new_branch(forest_m->begin()));
    forest_node_t&      branch_data(*branch);
    adobe::name_t       starting_struct_name(starting_struct.c_str());

    branch_data.name_m        = value_main;
    branch_data.struct_name_m = starting_struct_name;
    branch_data.set_flag(type_struct_k);

    return jump_into_structure(starting_struct_name, branch);
}

/****************************************************************************************************/

inspection_branch_t binspector_analyzer_t::new_branch(inspection_branch_t with_parent) {
    with_parent.edge() = adobe::forest_trailing_edge;

    return forest_m->insert(with_parent, forest_node_t());
}

/****************************************************************************************************/

template <typename T>
T binspector_analyzer_t::eval_here(const adobe::array_t& expression) {
    restore_point_t restore_point(input_m);

    // "here" being the current location the file format AST.
    return contextual_evaluation_of<T>(expression, forest_m->begin(), current_leaf_m, input_m);
}

// specialization because double doesn't always implicitly convert to boost::uint64_t
template <>
boost::uint64_t binspector_analyzer_t::eval_here(const adobe::array_t& expression) {
    return static_cast<boost::uint64_t>(eval_here<double>(expression));
}

/****************************************************************************************************/

void binspector_analyzer_t::signal_end_of_file() {
    // If this is true then we've reached eof once already and are continuing to
    // try and read the file. As such be a little more draconian: throw.
    if (eof_signalled_m)
        throw std::runtime_error("EOF reached. Consider using the eof slot.");

    eof_signalled_m = true;

    try {
        adobe::array_t expression;

        expression.push_back(adobe::any_regular_t("eof"_name));
        expression.push_back(adobe::any_regular_t(adobe::variable_k));

        inspection_branch_t slot(eval_here<inspection_branch_t>(expression));

        // actually update the slot with a new expression and clear the cache
        slot->expression_m = adobe::array_t(1, adobe::any_regular_t(true));
        slot->evaluated_m  = false;
    } catch (...) {
    }
}

/****************************************************************************************************/

template <typename T>
T binspector_analyzer_t::identifier_lookup(adobe::name_t identifier) {
    adobe::array_t expression;

    expression.push_back(adobe::any_regular_t(identifier));
    expression.push_back(adobe::any_regular_t(adobe::variable_k));

    return eval_here<T>(expression);
}

/****************************************************************************************************/

void binspector_analyzer_t::set_quiet(bool quiet) {
    quiet_m = quiet;
}

/****************************************************************************************************/

bool binspector_analyzer_t::analyze_with_structure(const structure_type& structure,
                                                   inspection_branch_t   parent) try {
    static const adobe::array_t empty_array_k;

    temp_assignment<inspection_branch_t> node_stack(current_leaf_m, parent);
    save_restore<typedef_map_t>          typedef_map_stack(current_typedef_map_m);
    bool                                 last_conditional_value(false);

    for (structure_type::const_iterator iter(structure.begin()), last(structure.end());
         iter != last;
         ++iter) {
        adobe::dictionary_t field(iter->cast<adobe::dictionary_t>());
        adobe::name_t       type(value_for<adobe::name_t>(field, key_field_type));
        adobe::name_t       name(value_for<adobe::name_t>(field, key_field_name));

        // The very first thing we want to do is the typedef resolution. This gives us the ability
        // to assert that the field dictionary is going to be the actual field once the typedef
        // lookup has completed.
        if (type == value_field_type_typedef_atom || type == value_field_type_typedef_named) {
            // add the typedef's field details to the typedef map; we're done
            current_typedef_map_m[name] = field;

            continue;
        }

        // Here we do the typename lookup if necessary and set the field appropriately.
        // If the name lookup is not necessary (i.e., we have found any type other than
        // a named type) then we just use the field pointed to by the current iterator.
        if (type == value_field_type_named) {
            field = typedef_lookup(current_typedef_map_m, field);

            // re-set these values given that the field may have changed.
            type = value_for<adobe::name_t>(field, key_field_type);
            name = value_for<adobe::name_t>(field, key_field_name);
        }

        bool                     has_conditional_type(field.count(key_field_conditional_type) != 0);
        conditional_expression_t conditional_type(
            has_conditional_type ?
                value_for<conditional_expression_t>(field, key_field_conditional_type) :
                none_k);
        bool is_conditional(conditional_type != none_k);

        last_name_m        = name;
        last_filename_m    = value_for<std::string>(field, key_parse_info_filename);
        last_line_number_m = value_for<double>(field, key_parse_info_line_number);

        // if our field is conditional check if condition holds; if not continue past field
        if (is_conditional) {
            if (conditional_type == else_k) {
                // if we've already found our true expression in this block skip this one
                if (last_conditional_value)
                    continue;

                last_conditional_value = true;
            } else // conditional_type == if_k
            {
                const adobe::array_t& if_expression(
                    value_for<adobe::array_t>(field, key_field_if_expression));

                last_conditional_value = eval_here<bool>(if_expression);
            }

            if (last_conditional_value) {
                // jump in to the conditional block but use the same parent as the
                // on that came in - this will add the conditional block's items
                // to the parent, flattening out the conditional.

                if (jump_into_structure(field, parent) == false)
                    return false;
            }

            // we're done with this conditional whether true or false
            continue;
        } else if (type == value_field_type_invariant) {
            const adobe::array_t& expression(
                value_for<adobe::array_t>(field, key_field_assign_expression));
            bool holds(eval_here<bool>(expression));

            if (!holds)
                throw std::runtime_error(
                    adobe::make_string("invariant '", name.c_str(), "' failed to hold."));

            continue;
        } else if (type == value_field_type_enumerated) {
            const adobe::array_t& branch_expression(
                value_for<adobe::array_t>(field, key_enumerated_expression));
            inspection_branch_t  atom(eval_here<inspection_branch_t>(branch_expression));
            adobe::any_regular_t value;

            {
                restore_point_t restore_point(input_m);

                value =
                    finalize_lookup<adobe::any_regular_t>(forest_m->begin(), atom, input_m, true);
            }

            temp_assignment<adobe::any_regular_t> enumerated_value(current_enumerated_value_m,
                                                                   value);
            temp_assignment<bool>           enumerated_found(current_enumerated_found_m, false);
            temp_assignment<adobe::array_t> enumerated_option_set(current_enumerated_option_set_m,
                                                                  adobe::array_t());

            if (jump_into_structure(field, parent) == false)
                return false;

            if (current_enumerated_found_m) {
                if (!atom->option_set_m.empty()) {
                    // This warning may need to be more clear, as its not
                    // last_path_m that is the cause of the warning but the
                    // option_set_m within the atom. (last_path() is the line
                    // in the code that caused the warning.)

                    error_m << "WARNING: " << build_path(atom) << " has multiple enumerate sets!\n";
                }

                // This stores within the enumerated node all the possible options
                // found in the AST. This will give the fuzzer a set of valid options
                // with which it can tweak this value and otherwise wreak smart
                // havoc with the file format.

                atom->option_set_m = current_enumerated_option_set_m;
            } else {
                std::string error;

                // REVISIT (fbrereto) : We have to have a cleaner solution than
                //                      this kind of concatenation...
                error += "value for " + build_path(atom) + " is not enumerated (" +
                         serialize(value) + ")";

                throw std::runtime_error(error);
            }

            continue;
        } else if (type == value_field_type_enumerated_option) {
            const adobe::array_t& expression(
                value_for<adobe::array_t>(field, key_enumerated_option_expression));
            adobe::any_regular_t option_value(eval_here<adobe::any_regular_t>(expression));

            current_enumerated_option_set_m.push_back(option_value);

            if (option_value == current_enumerated_value_m) {
                if (jump_into_structure(field, parent) == false)
                    return false;

                current_enumerated_found_m = true;
            }

            continue;
        } else if (type == value_field_type_enumerated_default) {
            if (!current_enumerated_found_m) {
                if (jump_into_structure(field, parent) == false)
                    return false;

                current_enumerated_found_m = true;
            }

            continue;
        } else if (type == value_field_type_sentry) {
            const adobe::array_t& expression(
                value_for<adobe::array_t>(field, key_sentry_expression));
            adobe::any_regular_t sentry_value(eval_here<adobe::any_regular_t>(expression));
            bitreader_t::pos_t   sentry_position;

            // Values of type double are relative to the current input position;
            // values of type pos_t are absolute.

            if (sentry_value.type_info() == typeid(double))
                sentry_position = input_m.pos() + bytepos(sentry_value.cast<double>());
            else if (sentry_value.type_info() == typeid(bitreader_t::pos_t))
                sentry_position = sentry_value.cast<bitreader_t::pos_t>();
            else
                throw std::runtime_error("Unexpected sentry type");

            {
                temp_assignment<bitreader_t::pos_t> sentry_holder(current_sentry_m,
                                                                  sentry_position);
                temp_assignment<std::string> sentry_name_holder(current_sentry_set_path_m,
                                                                last_path());

                // std::cerr << "! sentry set to " << sentry_position << '\n';

                if (jump_into_structure(field, parent) == false)
                    return false;
            }

            // Now we check to see not if we've gone beyond the sentry
            // but instead haven't reached it (e.g., we were told the
            // size would be 100 bytes but only analyzed 99.) While this
            // isn't necessarily fatal (hence the warning and not an
            // error) it may point to errors in the template definition.
            if (input_m.pos() != sentry_position) {
                error_m << "WARNING: After " << current_sentry_set_path_m
                        << " sentry, read position should be " << sentry_position
                        << " but instead is " << input_m.pos() << ".";

#if 0
                error_m << " Position forced!";

                // In the event the sentry is violated, put the read head
                // where it is expected. This will give us our best chance
                // at being able to continue reading the file without
                // further issue. This means we're trusting the length
                // prefixes in a document more than the actual data that's
                // in there, which is philosophically debatable, I'm sure.
                input_m.seek(sentry_position);
#endif

                error_m << '\n';
            }

            continue;
        } else if (type == value_field_type_notify) {
            // REVISIT (fbrereto) : Refactor and unify this code with value_field_type_summary

            if (quiet_m)
                continue;

            const adobe::array_t& expression(
                value_for<adobe::array_t>(field, key_notify_expression));
            adobe::array_t    argument_set(eval_here<adobe::array_t>(expression));
            std::stringstream result;

            adobe::copy(argument_set, std::ostream_iterator<adobe::any_regular_t>(result));

            output_m << result.str() << '\n';

            continue;
        } else if (type == value_field_type_summary) {
            // REVISIT (fbrereto) : Refactor and unify this code with value_field_type_notify

            if (quiet_m)
                continue;

            const adobe::array_t& expression(
                value_for<adobe::array_t>(field, key_summary_expression));
            adobe::array_t    argument_set(eval_here<adobe::array_t>(expression));
            std::stringstream result;

            // REVISIT (fbrereto) : I would like to be able to specify a serialization routien for
            //                      inspection_branch_t at this point, so I don't have to do this
            //                      kind of manual looping, however changes will need to go into ASL
            //                      to make that happen, so this is a bit of a hack.
            for (const auto& entry : argument_set) {
                if (entry.type_info() == typeid(inspection_branch_t)) {
                    result << entry.cast<inspection_branch_t>()->summary_m;
                } else {
                    result << entry;
                }
            }

            parent->summary_m = result.str();

            continue;
        } else if (type == value_field_type_die) {
            const adobe::array_t& expression(value_for<adobe::array_t>(field, key_die_expression));
            adobe::array_t        argument_set(eval_here<adobe::array_t>(expression));
            std::stringstream     result;

            result << "die: ";

            adobe::copy(argument_set, std::ostream_iterator<adobe::any_regular_t>(result));

            throw std::runtime_error(result.str());
        } else if (type == value_field_type_signal) {
            inspection_branch_t slot(identifier_lookup<inspection_branch_t>(name));

            // actually update the slot with a new expression and clear the cache
            slot->expression_m = value_for<adobe::array_t>(field, key_field_assign_expression);
            slot->evaluated_m  = false;

            continue;
        }

        // !!!!! NOTICE !!!!!
        //
        // From this point on we've actually added a node to the analysis forest.
        // So if you want to do anything during analysis without affecting the
        // forest, do it above this comment. (Also, make sure this comment stays
        // with the sub_branch creation so we have something obvious to demarcate
        // when a node is added to the forest.)

        inspection_branch_t sub_branch(new_branch(parent));
        forest_node_t&      branch_data(*sub_branch);

        temp_assignment<inspection_branch_t> node_stack(current_leaf_m, sub_branch);

        try {
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
                throw std::runtime_error(
                    adobe::make_string("analysis error: unknown type: ", type.c_str()));

            if (type == value_field_type_const) {
                branch_data.expression_m = value_for<adobe::array_t>(field, key_const_expression);
                branch_data.no_print_m   = value_for<bool>(field, key_const_no_print);

                continue;
            } else if (type == value_field_type_skip) {
                // skip is different in that its parameter is unit BYTES not bits
                const adobe::array_t& skip_expression(
                    value_for<adobe::array_t>(field, key_skip_expression));
                boost::uint64_t byte_count(
                    static_cast<boost::uint64_t>(eval_here<double>(skip_expression)));

                branch_data.start_offset_m = input_m.pos();

                if (parent->start_offset_m == invalid_position_k)
                    parent->start_offset_m = input_m.pos();

                // skip the bytes here
                branch_data.bit_count_m = byte_count * 8;
                branch_data.location_m  = make_location(branch_data.bit_count_m);

                branch_data.end_offset_m = input_m.pos() - inspection_byte_k;
                parent->end_offset_m     = branch_data.end_offset_m;

                continue;
            } else if (type == value_field_type_slot) {
                branch_data.expression_m =
                    value_for<adobe::array_t>(field, key_field_assign_expression);

                continue;
            }

            field_size_t field_size_type(value_for<field_size_t>(field, key_field_size_type));
            bool         has_size_expression(field_size_type != field_size_none_k);
            const adobe::array_t& field_size_expression(
                value_for<adobe::array_t>(field, key_field_size_expression));

            if (has_size_expression) {
                branch_data.set_flag(is_array_root_k);
                branch_data.cardinal_m = 0; // loops will overwrite
                branch_data.shuffle_m  = value_for<bool>(field, key_field_shuffle);
            }

            inspection_position_t position_save(input_m.pos());
            const adobe::array_t& offset_expression(
                value_for<adobe::array_t>(field, key_field_offset_expression));
            bool remote_position(!offset_expression.empty());

            if (remote_position) {
                // if our field's data is not at the next immediate offset,
                // temporarily set the position marker to that offset location
                // and restore it later.
                adobe::any_regular_t offset_value(
                    eval_here<adobe::any_regular_t>(offset_expression));
                inspection_position_t offset;

                if (offset_value.type_info() == typeid(double))
                    offset = bytepos(offset_value.cast<double>());
                else if (offset_value.type_info() == typeid(inspection_position_t))
                    offset = offset_value.cast<inspection_position_t>();
                else
                    throw std::runtime_error("Expected a position or a double for the offset");

                input_m.seek(offset);
            } else {
                // If it's not a remote position we want to cache the start
                // offset for this node in its parent, but only if it hasn't
                // already been set.
                if (parent->start_offset_m == invalid_position_k)
                    parent->start_offset_m = input_m.pos();
            }

            if (type == value_field_type_struct) {
                adobe::name_t struct_name(value_for<adobe::name_t>(field, key_named_type_name));

                branch_data.struct_name_m = struct_name;

                if (has_size_expression) {
                    // a loop means branch_data is an array root, so we must
                    // update the start and end offset for it.
                    branch_data.start_offset_m = input_m.pos();
                    branch_data.end_offset_m   = input_m.pos();

                    if (field_size_type == field_size_while_k) {
                        while (eval_here<bool>(field_size_expression)) {
                            inspection_branch_t array_element_branch(new_branch(sub_branch));
                            forest_node_t&      array_element_data(*array_element_branch);

                            array_element_data.set_flag(is_array_element_k);
                            array_element_data.cardinal_m = branch_data.cardinal_m++;

                            if (jump_into_structure(struct_name, array_element_branch) == false)
                                return false;

                            // We keep the end offset up to date because it
                            // may be used by the while predicate
                            branch_data.end_offset_m = input_m.pos() - inspection_byte_k;
                        }
                    } else if (field_size_type == field_size_integer_k) {
                        double size_count_double(eval_here<double>(field_size_expression));

                        if (size_count_double < 0)
                            throw std::runtime_error("Negative bounds size for array");

                        std::size_t size_count(static_cast<std::size_t>(size_count_double));

                        while (branch_data.cardinal_m != size_count) {
                            inspection_branch_t array_element_branch(new_branch(sub_branch));
                            forest_node_t&      array_element_data(*array_element_branch);

                            array_element_data.set_flag(is_array_element_k);
                            array_element_data.cardinal_m = branch_data.cardinal_m++;

                            if (jump_into_structure(struct_name, array_element_branch) == false)
                                return false;
                        }
                    } else if (field_size_type == field_size_terminator_k) {
                        throw std::runtime_error(
                            "Structure size expression: terminator not allowed");
                    } else if (field_size_type == field_size_delimiter_k) {
                        boost::uint64_t delimiter(
                            eval_here<boost::uint64_t>(field_size_expression));
                        boost::uint64_t delimiter_byte_count(
                            std::max<std::size_t>(1, highest_byte_for(delimiter)));

                        while (true) {
                            boost::uint64_t delimiter_peek(0);

                            {
                                restore_point_t      restore_point(input_m);
                                rawbytes_t           raw(input_m.read(delimiter_byte_count));
                                adobe::any_regular_t peeked_any(
                                    evaluate(raw, delimiter_byte_count * 8, atom_unsigned_k, true));
                                delimiter_peek =
                                    static_cast<boost::uint64_t>(peeked_any.cast<double>());
                            }

                            if (delimiter_peek == delimiter)
                                break;

                            inspection_branch_t array_element_branch(new_branch(sub_branch));
                            forest_node_t&      array_element_data(*array_element_branch);

                            array_element_data.set_flag(is_array_element_k);
                            array_element_data.cardinal_m = branch_data.cardinal_m++;

                            if (jump_into_structure(struct_name, array_element_branch) == false)
                                return false;
                        }
                    } else {
                        throw std::runtime_error("Unknown structure size expression type");
                    }

                    // One final update to the root's end offset should does the trick
                    branch_data.end_offset_m = input_m.pos() - inspection_byte_k;
                } else // singleton
                {
                    if (jump_into_structure(struct_name, sub_branch) == false)
                        return false;
                }
            } else if (type == value_field_type_atom) {
                const adobe::array_t& bit_count_expression(
                    value_for<adobe::array_t>(field, key_atom_bit_count_expression));
                const adobe::array_t& is_big_endian_expression(
                    value_for<adobe::array_t>(field, key_atom_is_big_endian_expression));
                bool is_big_endian(eval_here<bool>(is_big_endian_expression));

                branch_data.bit_count_m =
                    static_cast<boost::uint64_t>(eval_here<double>(bit_count_expression));
                branch_data.type_m = value_for<atom_base_type_t>(field, key_atom_base_type);
                branch_data.set_flag(atom_is_big_endian_k, is_big_endian);

                if (has_size_expression) {
                    // a loop means branch_data is an array root, so we must
                    // update the start and end offset for it.
                    branch_data.start_offset_m = input_m.pos();
                    branch_data.end_offset_m   = input_m.pos();

                    if (field_size_type == field_size_delimiter_k) {
                        /*
                            There is an issue here this code addresses which makes it more complicated,
                            and I'm not sure if it's necessary but I'm keeping it in. The problem is that
                            the delimiter you are interested in may not exist in the file aligned to the
                            current read boundary you have... for example a 16-bit delimiter cannot be
                            found by simply reading 16 bits at a time and then comparing because you
                            might read the two halves of the 16 bit value over the course of two
                            consecutive reads and fail both times. As such we have to read in the data
                            and push it into a "sliding window" that is the size of the delimiter we
                            are looking for and do the comparisons that way. It's much slower and the
                            language might benefit from the user being able to guarantee the alignment
                            of the values we are looking for, in which case we can improve the
                            performance here.
                        */
                        boost::uint64_t delimiter(
                            eval_here<boost::uint64_t>(field_size_expression));
                        boost::uint64_t delimiter_byte_count(
                            std::max<std::size_t>(1, highest_byte_for(delimiter)));
                        std::size_t delimiter_bit_count(
                            static_cast<std::size_t>(delimiter_byte_count * 8));
                        boost::uint64_t running_count(0);
                        boost::uint64_t running_stop_value(0);
                        boost::uint64_t running_stop_value_mask(
                            delimiter_bit_count == 8 ? 0xff :
                                                       delimiter_bit_count == 16 ?
                                                       0xffff :
                                                       delimiter_bit_count == 32 ? 0xffffffff : -1);

                        {
                            // because the while loop actually advances the read
                            // pointer we want to save it off here and restore it
                            // at the end of the delimiter-finding loop.
                            restore_point_t restore_point(input_m);

                            while (true) {
                                rawbytes_t raw(input_m.read(1));

                                if (input_m.fail())
                                    throw std::runtime_error("Delimiter not found: eof reached");

                                unsigned char c(raw[0]);

                                running_stop_value =
                                    (running_stop_value << 8 | c) & running_stop_value_mask;

                                if (running_stop_value != delimiter) {
                                    ++running_count;
                                } else {
                                    // we back up by the size of the window
                                    running_count -= bytesize(delimiter_bit_count) - 1;

                                    break;
                                }
                            }
                        }

                        for (boost::uint64_t i(0); i < running_count; ++i) {
                            inspection_branch_t array_element_branch(new_branch(sub_branch));
                            forest_node_t&      array_element_data(*array_element_branch);

                            array_element_data.set_flag(is_array_element_k);
                            array_element_data.cardinal_m = branch_data.cardinal_m++;
                            array_element_data.location_m = make_location(branch_data.bit_count_m);
                        }
                    } else if (field_size_type == field_size_terminator_k) {
                        boost::uint64_t terminator(
                            eval_here<boost::uint64_t>(field_size_expression));
                        boost::uint8_t read_size(
                            static_cast<boost::uint8_t>(bytesize(branch_data.bit_count_m)));
                        boost::uint8_t  read_size_leftovers(bitsize(branch_data.bit_count_m));
                        boost::uint64_t running_count(0);
                        boost::uint64_t terminator_mask(
                            read_size == 1 ?
                                0xff :
                                read_size == 2 ? 0xffff : read_size == 4 ? 0xffffffff : -1);

                        if (read_size_leftovers)
                            throw std::runtime_error(
                                "Use of non-byte-aligned fields with a terminator not supported.");

                        if (read_size > 8)
                            throw std::runtime_error("Use of terminators > 64 bits not supported.");

                        {
                            // because the while loop actually advances the read
                            // pointer we want to save it off here and restore it
                            // at the end of the terminator-finding loop.
                            restore_point_t restore_point(input_m);

                            while (true) {
                                rawbytes_t raw(input_m.read(read_size));

                                if (input_m.fail())
                                    throw std::runtime_error("Terminator not found: eof reached");

                                boost::uint64_t* termptr(
                                    reinterpret_cast<boost::uint64_t*>(&raw[0]));
                                boost::uint64_t buffer(*termptr & terminator_mask);

                                ++running_count;

                                if (buffer == terminator)
                                    break;
                            }
                        }

                        for (boost::uint64_t i(0); i < running_count; ++i) {
                            inspection_branch_t array_element_branch(new_branch(sub_branch));
                            forest_node_t&      array_element_data(*array_element_branch);

                            array_element_data.set_flag(is_array_element_k);
                            array_element_data.cardinal_m = branch_data.cardinal_m++;
                            array_element_data.location_m = make_location(branch_data.bit_count_m);
                        }
                    } else if (field_size_type == field_size_while_k) {
                        while (eval_here<bool>(field_size_expression)) {
                            inspection_branch_t array_element_branch(new_branch(sub_branch));
                            forest_node_t&      array_element_data(*array_element_branch);

                            array_element_data.set_flag(is_array_element_k);
                            array_element_data.cardinal_m = branch_data.cardinal_m++;
                            array_element_data.location_m = make_location(branch_data.bit_count_m);
                        }
                    } else if (field_size_type == field_size_integer_k) {
                        double size_count_double(eval_here<double>(field_size_expression));

                        if (size_count_double < 0)
                            throw std::runtime_error("Negative bounds size for array");

                        std::size_t size_count(static_cast<std::size_t>(size_count_double));

                        while (branch_data.cardinal_m != size_count) {
                            inspection_branch_t array_element_branch(new_branch(sub_branch));
                            forest_node_t&      array_element_data(*array_element_branch);

                            array_element_data.set_flag(is_array_element_k);
                            array_element_data.cardinal_m = branch_data.cardinal_m++;
                            array_element_data.location_m = make_location(branch_data.bit_count_m);
                        }
                    } else {
                        throw std::runtime_error("Unknown atom size expression type");
                    }

                    // One final update to the root's end offset should does the trick
                    branch_data.end_offset_m = input_m.pos() - inspection_byte_k;
                } else // singleton
                {
                    branch_data.location_m = make_location(branch_data.bit_count_m);
                }
            } else {
                error_m << "WARNING: I'm not sure what I'm looking at (" << type << ")...\n";
            }

            if (remote_position) {
                // restore the position marker if it was tweaked to read this field
                input_m.seek(position_save);
            } else {
                // If it's not a remote position we want to cache the end
                // offset for this node in its parent. Because the nodes
                // are in increasing order in the file we overwrite the
                // end offset with whatever the most current value is.
                parent->end_offset_m = input_m.pos() - inspection_byte_k;
            }
        } catch (const std::out_of_range&) {
            // eliminate the node that caused the eof; it is invalid.
            forest_m->erase(sub_branch);

            // rethrow; eof signalling handled elsewhere.
            throw;
        }
    }

    return true;
} catch (const std::out_of_range&) {
    // signal eof slot if it is present
    signal_end_of_file();

    return true;
} catch (const std::exception& error) {
    error_m << "error: " << error.what() << '\n';

    if (current_leaf_m != inspection_branch_t())
        error_m << "while analyzing: " << last_path() << '.' << last_name_m << '\n';

    error_m << "in file: " << last_filename_m << ":" << last_line_number_m << '\n';

    return false;
}

/****************************************************************************************************/

const binspector_analyzer_t::structure_type& binspector_analyzer_t::structure_for(
    adobe::name_t structure_name) {
    structure_map_t::const_iterator structure(structure_map_m.find(structure_name));

    if (structure == structure_map_m.end())
        throw std::runtime_error(
            adobe::make_string("Could not find structure '", structure_name.c_str(), "'"));

    return structure->second;
}

/****************************************************************************************************/

adobe::dictionary_t typedef_lookup(const binspector_analyzer_t::typedef_map_t& typedef_map,
                                   const adobe::dictionary_t&                  src_field) {
    adobe::name_t type(value_for<adobe::name_t>(src_field, key_named_type_name));
    binspector_analyzer_t::typedef_map_t::const_iterator found(typedef_map.find(type));
    adobe::dictionary_t                                  result(src_field);

    if (found == typedef_map.end()) {
        // found a top level identity that is neither an atom nor another possible
        // typedef. At this point we either have a name of a structure or we have
        // something mistyped by the user.

        result[key_field_type].assign(value_field_type_struct);
        result[key_named_type_name].assign(type);
    } else {
        const adobe::dictionary_t& result_field(found->second);

        type = value_for<adobe::name_t>(result_field, key_field_type);

        if (type == value_field_type_typedef_atom) {
            // we found an atom typedef; we're done.
            transfer_field(result, result_field, key_atom_base_type);
            transfer_field(result, result_field, key_atom_bit_count_expression);
            transfer_field(result, result_field, key_atom_is_big_endian_expression);

            result[key_field_type].assign(value_field_type_atom);
        } else if (type == value_field_type_typedef_named) {
            transfer_field(result, result_field, key_named_type_name);

            result = typedef_lookup(typedef_map, result);
        } else {
            throw std::runtime_error(adobe::make_string(
                "Typedef lookup failure: don't know what to do with entries of type ",
                type.c_str()));
        }
    }

    return result;
}

/****************************************************************************************************/
