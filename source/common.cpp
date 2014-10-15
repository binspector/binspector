/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// identity
#include <binspector/common.hpp>

// boost
#include <boost/lexical_cast.hpp>
//#include <boost/timer/timer.hpp>

// asl
#include <adobe/implementation/expression_parser.hpp>
#include <adobe/implementation/token.hpp>
#include <adobe/virtual_machine.hpp>

/****************************************************************************************************/

namespace {

/****************************************************************************************************/

template <typename T>
inline adobe::any_regular_t convert_raw(const rawbytes_t& raw)
{
    T value(*reinterpret_cast<const T*>(&raw[0]));

    return adobe::any_regular_t(static_cast<double>(value));
}

inline adobe::any_regular_t convert_raw(const rawbytes_t& raw,
                                        std::size_t       bit_count,
                                        atom_base_type_t  base_type)
{
    if (base_type == atom_unknown_k)
    {
        throw std::runtime_error("convert_raw: unknown atom base type");
    }
    else if (base_type == atom_float_k)
    {
        BOOST_STATIC_ASSERT((sizeof(float) == 4));
        BOOST_STATIC_ASSERT((sizeof(double) == 8));

        if (bit_count == 32)
            return convert_raw<float>(raw);
        else if (bit_count == 64)
            return convert_raw<double>(raw);
        else
            throw std::runtime_error("convert_raw: float atom of specified bit count not supported.");
    }
    else if (bit_count <= 8)
    {
        if (base_type == atom_signed_k)
            return convert_raw<boost::int8_t>(raw);
        else if (base_type == atom_unsigned_k)
            return convert_raw<boost::uint8_t>(raw);
    }
    else if (bit_count <= 16)
    {
        if (base_type == atom_signed_k)
            return convert_raw<boost::int16_t>(raw);
        else if (base_type == atom_unsigned_k)
            return convert_raw<boost::uint16_t>(raw);
    }
    else if (bit_count <= 32)
    {
        if (base_type == atom_signed_k)
            return convert_raw<boost::int32_t>(raw);
        else if (base_type == atom_unsigned_k)
            return convert_raw<boost::uint32_t>(raw);
    }
    else if (bit_count <= 64)
    {
        if (base_type == atom_signed_k)
            return convert_raw<boost::int64_t>(raw);
        else if (base_type == atom_unsigned_k)
            return convert_raw<boost::uint64_t>(raw);
    }

    throw std::runtime_error("convert_raw: invalid bit count");
}

/****************************************************************************************************/
#if 0
#pragma mark -
#endif
/****************************************************************************************************/

class evaluation_error_t : public std::runtime_error
{
public:
    explicit evaluation_error_t(const std::string& error) :
        std::runtime_error(error)
    { }

    virtual ~evaluation_error_t() throw()
    { }
};

/****************************************************************************************************/

struct contextual_evaluation_engine_t
{
    contextual_evaluation_engine_t(const adobe::array_t& expression,
                                   inspection_branch_t   main_branch,
                                   inspection_branch_t   current_node,
                                   bitreader_t&          input);

    adobe::any_regular_t evaluate(bool finalize = true);

private:
    // evaluation vm custom callbacks
    adobe::any_regular_t named_index_lookup(const adobe::any_regular_t& value,
                                            adobe::name_t               name,
                                            bool                        throwing);
    adobe::any_regular_t numeric_index_lookup(const adobe::any_regular_t& value,
                                              std::size_t                 index);
    adobe::any_regular_t stack_variable_lookup(adobe::name_t name);
    adobe::any_regular_t array_function_lookup(adobe::name_t         name,
                                               const adobe::array_t& parameter_set);

    // helpers
    inspection_branch_t  regular_to_branch(const adobe::any_regular_t& name_or_branch);

    const adobe::array_t& expression_m;
    inspection_branch_t   main_branch_m;
    inspection_branch_t   current_node_m;
    bitreader_t&          input_m;
    bool                  finalize_m;
};

/****************************************************************************************************/

contextual_evaluation_engine_t::contextual_evaluation_engine_t(const adobe::array_t& expression,
                                                               inspection_branch_t   main_branch,
                                                               inspection_branch_t   current_node,
                                                               bitreader_t&          input) :
    expression_m(expression),
    main_branch_m(main_branch),
    current_node_m(current_node),
    input_m(input)
{ }

/****************************************************************************************************/
#if 0
template <typename T, typename U>
void bad_cast_details(const adobe::bad_cast& error, const std::string& details)
{
    static const adobe::bad_cast test(typeid(T), typeid(U));
    static const std::string     test_string(test.what());

    if (std::string(error.what()) == test_string)
        throw std::runtime_error(details);
}
#endif
/****************************************************************************************************/

adobe::any_regular_t contextual_evaluation_engine_t::evaluate(bool finalize)
try
{
    adobe::virtual_machine_t vm;

    finalize_m = finalize;

    vm.set_variable_lookup(std::bind(&contextual_evaluation_engine_t::stack_variable_lookup,
                                     std::ref(*this),
                                     std::placeholders::_1));
    vm.set_named_index_lookup(std::bind(&contextual_evaluation_engine_t::named_index_lookup,
                                        std::ref(*this),
                                        std::placeholders::_1,
                                        std::placeholders::_2,
                                        true));
    vm.set_numeric_index_lookup(std::bind(&contextual_evaluation_engine_t::numeric_index_lookup,
                                          std::ref(*this),
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    vm.set_array_function_lookup(std::bind(&contextual_evaluation_engine_t::array_function_lookup,
                                           std::ref(*this),
                                           std::placeholders::_1,
                                           std::placeholders::_2));

    vm.evaluate(expression_m);

    adobe::any_regular_t result(vm.back());

    vm.pop_back();

    return result;
}
#if 0
catch (const adobe::bad_cast& error)
{
    // If in the course of eval we get a bad cast from double to name we can
    // deduce the user might have forgotten an @ symbol when appropriate, e.g.,
    //     str(my_field) instead of str(@my_field)
    // Another possible error is when a user tries to fetch the subfield of
    // an address, which is not allowed:
    //     @field.subfield
    // Another possible error is when an address is passed when a value is
    // expected, a la
    //     byte(@field), which expects an offset instead of an address

    std::string path(build_path(main_branch_m, current_node_m));

    bad_cast_details<double, adobe::name_t>(error, "Address failure -- possible '@' missing in " + path);
    bad_cast_details<adobe::name_t, inspection_branch_t>(error, "Address failure -- possible '@' extra in " + path);
    bad_cast_details<inspection_branch_t, double>(error, "Address failure -- possible '@' extra in " + path);

    throw error;
}
#endif
catch (const std::out_of_range& error)
{
    // we'll get this from the bitreader when we hit the end of the file.
    // pass it up -- it's handled elsewhere.

    throw;
}
catch (const evaluation_error_t& error)
{
    // we'll get this from the contexutal evaluation engine when something
    // goes wrong. It's already formatted, to the std::exception decoration
    // below isn't necessary. Pass it up.

    throw;
}
catch (const std::exception& error)
{
    std::string details(error.what());

    // REVISIT (fbrereto) : String concatenation in this routine

    if (dynamic_cast<const std::bad_cast*>(&error) != NULL)
        details += " -- do you need to use ptoi or itop?";

    if (current_node_m.equal_node(inspection_branch_t()))
        details += "\nin field: <<unknown>>";
    else
        details += "\nin field: " + build_path(main_branch_m, current_node_m);

    throw std::runtime_error(details);
}

/****************************************************************************************************/

adobe::any_regular_t contextual_evaluation_engine_t::named_index_lookup(const adobe::any_regular_t& value,
                                                                        adobe::name_t               name,
                                                                        bool                        throwing)
{
    inspection_branch_t branch(value.cast<inspection_branch_t>());

    if (!branch.equal_node(inspection_branch_t()))
    {
        inspection_forest_t::child_iterator iter(adobe::child_begin(branch));
        inspection_forest_t::child_iterator last(adobe::child_end(branch));

        for (; iter != last; ++iter)
            if (iter->name_m == name)
                return finalize_lookup<adobe::any_regular_t>(main_branch_m,
                                                             inspection_branch_t(iter.base()),
                                                             input_m,
                                                             finalize_m);
    }

    if (throwing)
        throw std::runtime_error(adobe::make_string("Subfield '",
                                                    name.c_str(),
                                                    "' not found."));

    return adobe::any_regular_t();
}

/****************************************************************************************************/

adobe::any_regular_t contextual_evaluation_engine_t::numeric_index_lookup(const adobe::any_regular_t& value,
                                                                          std::size_t                 index)
{
    inspection_branch_t branch(value.cast<inspection_branch_t>());

    if (!adobe::has_children(branch))
        throw std::range_error(adobe::make_string("Array '",
                                                  branch->name_m.c_str(),
                                                  "' is empty"));

    inspection_forest_t::child_iterator iter(adobe::child_begin(branch));
    inspection_forest_t::child_iterator last(adobe::child_end(branch));
    std::size_t                         i(0);

    while (true)
    {
        if (i == index)
            return finalize_lookup<adobe::any_regular_t>(main_branch_m,
                                                         inspection_branch_t(iter.base()),
                                                         input_m,
                                                         finalize_m);

        if (++iter == last)
        {
            std::stringstream error;
            error << "Array index " << index << " out of range [ 0 .. " << i << " ] for array '" << branch->name_m << "'";
            throw std::range_error(error.str());
        }

        ++i;
    }

    return adobe::any_regular_t();
}

/****************************************************************************************************/

adobe::any_regular_t contextual_evaluation_engine_t::stack_variable_lookup(adobe::name_t name)
{
    inspection_branch_t current_node(current_node_m);
    bool                bad_node(current_node.equal_node(inspection_branch_t()));

    if (name == value_main)
        return adobe::any_regular_t(main_branch_m);
    else if (name == value_this)
        return adobe::any_regular_t(current_node);

    while (!bad_node)
    {
        adobe::any_regular_t subfield(named_index_lookup(adobe::any_regular_t(current_node),
                                                         name,
                                                         false));

        // named_index_lookup handles finalization
        if (subfield != adobe::any_regular_t())
            return subfield;

        current_node.edge() = adobe::forest_leading_edge;

        if (current_node == main_branch_m)
            break;

        current_node = adobe::find_parent(current_node);

        bad_node = current_node.equal_node(inspection_branch_t());
    }

    std::string error;

    // REVISIT (fbrereto) : String concatenation in this routine.
    if (bad_node)
        error += adobe::make_string("Subfield '",
                                    name.c_str(),
                                    "' not found at or above unknown node");
    else
        error += adobe::make_string("Subfield '",
                                    name.c_str(),
                                    "' not found at or above ") +
                 build_path(main_branch_m, current_node_m);

    throw std::runtime_error(error);
}

/****************************************************************************************************/

inspection_branch_t contextual_evaluation_engine_t::regular_to_branch(const adobe::any_regular_t& name_or_branch)
{
    const std::type_info& type(name_or_branch.type_info());

    if (type == typeid(inspection_branch_t))
    {
        return name_or_branch.cast<inspection_branch_t>();
    }
    else if (type == typeid(adobe::name_t))
    {
        // Otherwise, converts a user-specified name-as-path to an inspection branch

        std::stringstream      input(name_or_branch.cast<adobe::name_t>().c_str());
        adobe::line_position_t position(__FILE__, __LINE__);
        adobe::array_t         expression;

        adobe::expression_parser(input, position).require_expression(expression);

        return contextual_evaluation_of<inspection_branch_t>(expression,
                                                             main_branch_m,
                                                             current_node_m,
                                                             input_m);
    }

    throw std::runtime_error(adobe::make_string("Expected ref(@field) or @field, but was passed ",
                                                type.name()));
}

/****************************************************************************************************/

adobe::any_regular_t contextual_evaluation_engine_t::array_function_lookup(adobe::name_t         name,
                                                                           const adobe::array_t& parameter_set)
{
    CONSTANT_VALUE(byte);
    CONSTANT_VALUE(card);
    CONSTANT_VALUE(endof);
    CONSTANT_VALUE(indexof);
    CONSTANT_VALUE(path);
    CONSTANT_VALUE(peek);
    CONSTANT_VALUE(print);
    CONSTANT_VALUE(sizeof);
    CONSTANT_VALUE(startof);
    CONSTANT_VALUE(str);
    CONSTANT_VALUE(strcat);
    CONSTANT_VALUE(summaryof);
    CONSTANT_VALUE(fcc);
    CONSTANT_VALUE(ptoi);
    CONSTANT_VALUE(padd);
    CONSTANT_VALUE(psub);
    CONSTANT_VALUE(itop);
    CONSTANT_VALUE(gtell);

    if (name == value_sizeof)
    {
        if (parameter_set.empty())
            throw std::runtime_error("sizeof(): @field_name expected");

        inspection_position_t start_offset;
        inspection_position_t end_offset(invalid_position_k);

        if (parameter_set.size() == 1)
        {
            inspection_branch_t leaf(regular_to_branch(parameter_set[0]));
            
            start_offset = starting_offset_for(leaf);
            end_offset = ending_offset_for(leaf);
        }
        else if (parameter_set.size() == 2)
        {
            inspection_branch_t leaf1(regular_to_branch(parameter_set[0]));
            inspection_branch_t leaf2(regular_to_branch(parameter_set[1]));
            
            start_offset = starting_offset_for(leaf1);
            end_offset = ending_offset_for(leaf2);
        }
        else
        {
            throw std::runtime_error("sizeof(): too many parameters");
        }

        inspection_position_t size(end_offset - start_offset + inspection_byte_k);

        return adobe::any_regular_t(static_cast<double>(size.bytes()));
    }
    else if (name == value_startof)
    {
        if (parameter_set.empty())
            throw std::runtime_error("startof(): @field_name expected");

        inspection_branch_t   leaf(regular_to_branch(parameter_set[0]));
        inspection_position_t start_offset(starting_offset_for(leaf));

        return adobe::any_regular_t(start_offset);
    }
    else if (name == value_endof)
    {
        if (parameter_set.empty())
            throw std::runtime_error("endof(): @field_name expected");

        inspection_branch_t   leaf(regular_to_branch(parameter_set[0]));
        inspection_position_t end_offset(ending_offset_for(leaf));

        return adobe::any_regular_t(end_offset);
    }
    else if (name == value_byte)
    {
        if (parameter_set.empty())
            throw std::runtime_error("byte(): offset expected");

        const adobe::any_regular_t& argument(parameter_set[0]);
        inspection_position_t       offset;
        
        if (argument.type_info() == typeid(double))
            offset = bytepos(argument.cast<double>());
        else // argument.type_info() == inspection_position_t
            offset = argument.cast<inspection_position_t>();

        input_m.seek(offset);

        rawbytes_t buffer(input_m.read(1));

        return adobe::any_regular_t(static_cast<double>(buffer[0]));
    }
    else if (name == value_peek)
    {
        std::size_t byte_count(1);

        if (!parameter_set.empty())
            byte_count = static_cast<std::size_t>(parameter_set[0].cast<double>());

        rawbytes_t buffer;

        {
        restore_point_t restore(input_m);

        buffer = input_m.read(byte_count);
        }

        return byte_count > 1 ?
                   adobe::any_regular_t(std::string(buffer.begin(), buffer.end())) :
                   adobe::any_regular_t(static_cast<double>(buffer[0]));
    }
    else if (name == value_card)
    {
        if (parameter_set.empty())
            throw std::runtime_error("card(): @field_name expected");

        inspection_branch_t array(regular_to_branch(parameter_set[0]));

        if (!array->get_flag(is_array_root_k))
            throw std::runtime_error("card(): field is not an array");

        return adobe::any_regular_t(static_cast<double>(node_property(array, ARRAY_ROOT_PROPERTY_SIZE)));
    }
    else if (name == value_print)
    {
        // should be a printf-like behavior, to be able to output numbers etc.

        if (parameter_set.empty())
            throw std::runtime_error("print(): argument required");

        std::string result;

        for (adobe::array_t::const_iterator iter(parameter_set.begin()), last(parameter_set.end()); iter != last; ++iter)
        {
            const std::type_info& type(iter->type_info());

            if (type == typeid(double))
                result += boost::lexical_cast<std::string>(iter->cast<double>());
            else if (type == typeid(std::string))
                result += iter->cast<std::string>();
            else
                result += type.name();
        }

        return adobe::any_regular_t(result);
    }
    else if (name == value_strcat)
    {
        if (parameter_set.empty())
            throw std::runtime_error("strcat(): argument required");

        std::string result;

        for (adobe::array_t::const_iterator iter(parameter_set.begin()), last(parameter_set.end()); iter != last; ++iter)
            result += iter->cast<std::string>();

        return adobe::any_regular_t(result);
    }
    else if (name == value_summaryof)
    {
        if (parameter_set.size() != 1)
            throw std::runtime_error("summaryof(): takes one argument");

        return adobe::any_regular_t(regular_to_branch(parameter_set[0])->summary_m);
    }
    else if (name == value_str)
    {
        if (parameter_set.empty())
            throw std::runtime_error("str(): @field_name expected");

        inspection_branch_t   leaf(regular_to_branch(parameter_set[0]));
        inspection_position_t start_offset(starting_offset_for(leaf));
        inspection_position_t end_offset(ending_offset_for(leaf));
        inspection_position_t size(end_offset - start_offset + inspection_byte_k);

        if (node_property(leaf, NODE_PROPERTY_IS_CONST))
            throw std::runtime_error("str(): cannot take the string of a const");

        input_m.seek(start_offset);

        rawbytes_t str(input_m.read(size.bytes()));

        // This is a workaround I'm still not sure about; in the cases when we
        // obtain an array with the terminator: construct the terminator is
        // included. In the case of a null-terminated c-string we consider it
        // part of the array but do not want it included in the string_t. As
        // such we have a general exception case here where if the final
        // character of the string is 0 it is excluded.

        std::size_t strsize(str.size());

        if (strsize && str[strsize - 1] == 0)
            str.resize(strsize - 1);

        // If we have an atom that isn't an array root and it's little endian,
        // we need to reverse the contents.
        if (node_property(leaf, NODE_PROPERTY_IS_ATOM) &&
            !node_property(leaf, ATOM_PROPERTY_IS_BIG_ENDIAN) &&
            !leaf->get_flag(is_array_root_k))
        {
            adobe::reverse(str);
        }

        return adobe::any_regular_t(std::string(str.begin(), str.end()));
    }
    else if (name == value_path)
    {
        adobe::any_regular_t path("this"_name);

        if (!parameter_set.empty())
            path = parameter_set[0];

        inspection_branch_t leaf(regular_to_branch(path));

        return adobe::any_regular_t(std::string(build_path(main_branch_m, leaf)));
    }
    else if (name == value_indexof)
    {
        adobe::any_regular_t path("this"_name);

        if (!parameter_set.empty())
            path = parameter_set[0];

        inspection_branch_t leaf(regular_to_branch(path));

        if (!leaf->get_flag(is_array_element_k))
        {
            adobe::name_t     name(node_property(leaf, NODE_PROPERTY_NAME));
            std::stringstream error;
            error << "indexof(): field '" << name << "' is not an array element";
            throw std::runtime_error(error.str());
        }

        return adobe::any_regular_t(static_cast<double>(node_value(leaf, ARRAY_ELEMENT_VALUE_INDEX)));
    }
    else if (name == value_fcc)
    {
        // converts an N character-code (commonly a four character code) to its
        // integer equivalent.

        if (parameter_set.empty())
            throw std::runtime_error("fcc(): character code expected");

        std::string fcc(parameter_set[0].cast<std::string>());
        std::size_t length(fcc.size());

        if (length > 4)
            throw std::runtime_error("fcc(): character code too long.");

        boost::uint32_t value(0);

        for (std::size_t i(0); i < length; ++i)
            value = (value << 8) | fcc[i];

        return adobe::any_regular_t(value);
    }
    else if (name == value_ptoi)
    {
        // converts the bytes portion of a pos_t to a double

        if (parameter_set.empty())
            throw std::runtime_error("ptoi(): position required");

        const adobe::any_regular_t& argument(parameter_set[0]);

        if (argument.type_info() != typeid(inspection_position_t))
            throw std::runtime_error("ptoi(): bad parameter type (expects a position)");

        // NOTE (fbrereto) : 64->32 truncation!
        boost::uint32_t value(argument.cast<inspection_position_t>().bytes());

        return adobe::any_regular_t(value);
    }
    else if (name == value_itop)
    {
        // converts the bytes portion of a pos_t to a double

        if (parameter_set.empty())
            throw std::runtime_error("itop(): position required");

        const adobe::any_regular_t& argument(parameter_set[0]);

        if (argument.type_info() != typeid(double))
            throw std::runtime_error("itop(): bad parameter type (expects an integer)");

        inspection_position_t value(bytepos(argument.cast<double>()));

        return adobe::any_regular_t(value);
    }
    else if (name == value_padd)
    {
        // adds N values (position and/or double) and returns a position

        if (parameter_set.empty())
            throw std::runtime_error("padd(): argument required");

        inspection_position_t result;

        for (adobe::array_t::const_iterator iter(parameter_set.begin()), last(parameter_set.end()); iter != last; ++iter)
        {
            if (iter->type_info() == typeid(double))
                result += bytepos(iter->cast<double>());
            else if (iter->type_info() == typeid(inspection_position_t))
                result += iter->cast<inspection_position_t>();
            else
                throw std::runtime_error("padd() : type of argument must be position or double");
        }

        return adobe::any_regular_t(result);
    }
    else if (name == value_psub)
    {
        // subs 2 values (position and/or double) and returns a position

        if (parameter_set.size() != 2)
            throw std::runtime_error("psub(): exactly 2 arguments required");

        inspection_position_t       result;
        const adobe::any_regular_t& op1(parameter_set[0]);
        const adobe::any_regular_t& op2(parameter_set[1]);

        if (op1.type_info() == typeid(double))
            result += bytepos(op1.cast<double>());
        else if (op1.type_info() == typeid(inspection_position_t))
            result += op1.cast<inspection_position_t>();
        else
            throw std::runtime_error("psub() : first argument must be position or double");

        if (op2.type_info() == typeid(double))
            result -= bytepos(op2.cast<double>());
        else if (op2.type_info() == typeid(inspection_position_t))
            result -= op2.cast<inspection_position_t>();
        else
            throw std::runtime_error("psub() : second argument must be position or double");

        return adobe::any_regular_t(result);
    }
    else if (name == value_gtell)
    {
        // returns the current read head position
        return adobe::any_regular_t(input_m.pos());
    }

    throw std::runtime_error(adobe::make_string("Function '",
                                                name.c_str(),
                                                "' not found"));
}

/****************************************************************************************************/

} // namespace

/****************************************************************************************************/
#if 0
#pragma mark -
#endif

/****************************************************************************************************/

adobe::any_regular_t evaluate(const rawbytes_t& raw,
                              boost::uint64_t   bit_count,
                              atom_base_type_t  base_type,
                              bool              is_big_endian)
{
    rawbytes_t  byte_set(raw);

#if __LITTLE_ENDIAN__ || defined(_M_IX86) || defined(_WIN32)
    if (is_big_endian)
        std::reverse(byte_set.begin(), byte_set.end());
#endif
#if __BIG_ENDIAN__
    if (!is_big_endian)
        std::reverse(byte_set.begin(), byte_set.end());
#endif

    return convert_raw(byte_set, bit_count, base_type);
}

/****************************************************************************************************/

adobe::any_regular_t fetch_and_evaluate(bitreader_t&                 input,
                                        const inspection_position_t& location,
                                        boost::uint64_t              bit_count,
                                        atom_base_type_t             base_type,
                                        bool                         is_big_endian)
{
    return evaluate(input.read_bits(location, bit_count), bit_count, base_type, is_big_endian);
}

/****************************************************************************************************/

template <>
adobe::any_regular_t finalize_lookup(inspection_branch_t root,
                                     inspection_branch_t branch,
                                     bitreader_t&        input,
                                     bool                finalize)
{
    // converts our branch into its value when its an atom or a const.
    // can be bypassed by passing false to evaluate(), in which case
    // the evaluated result is always an inspection branch.

    adobe::any_regular_t result(branch);

    ++branch->use_count_m;

    // really slow, but good for debugging
    // std::cerr << "branch " << build_path(main_branch_m, branch) << " use count now " << branch->use_count_m <<'\n';

    if (!finalize)
    {
        // do nothing
    }
    else if (branch->get_flag(is_array_root_k) ||
             node_property(branch, NODE_PROPERTY_IS_STRUCT))
    {
        // do nothing
    }
    else if (node_property(branch, NODE_PROPERTY_IS_ATOM))
    {
        atom_base_type_t      base_type(node_property(branch, ATOM_PROPERTY_BASE_TYPE));
        bool                  is_big_endian(node_property(branch, ATOM_PROPERTY_IS_BIG_ENDIAN));
        boost::uint64_t       bit_count(node_property(branch, ATOM_PROPERTY_BIT_COUNT));
        inspection_position_t position(node_value(branch, ATOM_VALUE_LOCATION));

        // REVISIT (fbrereto) : Is there any reason why we wouldn't want to cache
        //                      the value of an atom like we do a const or slot?

        result = fetch_and_evaluate(input, position, bit_count, base_type, is_big_endian);
    }
    else if (node_property(branch, NODE_PROPERTY_IS_CONST) ||
             node_property(branch, NODE_PROPERTY_IS_SLOT))
    {
        // We handle slots and consts the same way; at the time a signal is fired for the slot
        // a new expression is pushed into the slot's expression and the cache is invalidated.

        if (node_value(branch, CONST_VALUE_IS_EVALUATED) == false)
        {
            try
            {
                const adobe::array_t& const_expression(node_value(branch, CONST_VALUE_EXPRESSION));

                // Note (fbrereto): Evaluate at the point of the const declaration,
                //                  not at the current branch.
                branch->evaluated_value_m = contextual_evaluation_of<adobe::any_regular_t>(const_expression,
                                                                                           root,
                                                                                           adobe::find_parent(branch),
                                                                                           input);
                branch->evaluated_m = true;
            }
            catch (const std::exception& error)
            {
                // This pads out the current error with the location of the
                // expression being evaluated, hopefully giving the user a
                // cascade of lines directing them from the location of the
                // evaluation error to the line in the template that caused it.

                std::string details(error.what());

                // REVISIT (fbrereto) : String concatenation in this block

                details += adobe::make_string(", while evaluating constant '",
                                              node_property(branch, NODE_PROPERTY_NAME).c_str(),
                                              "'");

                if (branch.equal_node(inspection_branch_t()))
                    details += "\nin field: <<unknown>>";
                else
                    details += "\nin field: " + build_path(root, branch);

                throw evaluation_error_t(details);
            }
        }

        result = node_value(branch, CONST_VALUE_EVALUATED_VALUE);
    }

    return result;
}

/****************************************************************************************************/

template <>
adobe::any_regular_t contextual_evaluation_of(const adobe::array_t& expression,
                                              inspection_branch_t   main_branch,
                                              inspection_branch_t   current_node,
                                              bitreader_t&          input)
{
    return contextual_evaluation_engine_t(expression, main_branch, current_node, input).evaluate();
}

template <>
inspection_branch_t contextual_evaluation_of(const adobe::array_t& expression,
                                             inspection_branch_t   main_branch,
                                             inspection_branch_t   current_node,
                                             bitreader_t&          input)
{
    return contextual_evaluation_engine_t(expression, main_branch, current_node, input).evaluate(false).cast<inspection_branch_t>();
}

/****************************************************************************************************/

inspection_position_t starting_offset_for(inspection_branch_t branch)
{
    bool is_struct(node_property(branch, type_struct_k));
    bool is_array_root(branch->get_flag(is_array_root_k));

    if (is_struct || is_array_root)
        return branch->start_offset_m;

    return node_value(branch, ATOM_VALUE_LOCATION);
}

/****************************************************************************************************/

inspection_position_t ending_offset_for(inspection_branch_t branch)
{
    bool is_struct(node_property(branch, type_struct_k));
    bool is_array_root(branch->get_flag(is_array_root_k));

    if (is_struct || is_array_root)
        return branch->end_offset_m;

    inspection_position_t position(node_value(branch, ATOM_VALUE_LOCATION));
    boost::uint64_t       bit_count(node_property(branch, ATOM_PROPERTY_BIT_COUNT));

    return position + bitpos(bit_count) - inspection_byte_k;
}

/****************************************************************************************************/

std::string build_path(const_inspection_branch_t main, const_inspection_branch_t current)
{
#if !BOOST_WINDOWS
    using std::isalpha;
#endif

    std::vector<std::string> result_set;

    result_set.reserve(10);

    while (true)
    {
        std::string path_segment;
        bool        is_array_element(current->get_flag(is_array_element_k));

        if (is_array_element)
        {
            boost::uint64_t index(node_value(current, ARRAY_ELEMENT_VALUE_INDEX));

            // REVISIT (fbrereto) : String concatenation here.
            path_segment += "[" + boost::lexical_cast<std::string>(index) + "]";
        }
        else
        {
            adobe::name_t name(node_property(current, NODE_PROPERTY_NAME));

            if (name)
                path_segment = name.c_str();
        }

        if (!path_segment.empty())
            result_set.push_back(path_segment);

        current.edge() = adobe::forest_leading_edge;

        if (current == main)
            break;

        current = adobe::find_parent(current);
    }

    std::string result;
    bool        first(true);

    for (std::vector<std::string>::reverse_iterator iter(result_set.rbegin()), last(result_set.rend()); iter != last; ++iter)
    {
        if (!first && isalpha((*iter)[0]))
            result += ".";

        result += *iter;
        
        first = false;
    }

    return result;
}

/****************************************************************************************************/

attack_vector_set_t build_attack_vector_set(const inspection_forest_t& forest)
{
    const_inspection_branch_t                    root(forest.begin());
    inspection_forest_t::const_preorder_iterator iter(root);
    inspection_forest_t::const_preorder_iterator last(forest.end());
    std::size_t                                  node_count(0);
    attack_vector_set_t                          result;

    for (; iter != last; ++iter, ++node_count)
    {
        const_inspection_branch_t iter_base(iter.base());
        std::size_t               use_count(iter_base->use_count_m);
        bool                      used_atom(use_count != 0 && iter_base->bit_count_m != 0);
        bool                      shuffle_array(iter_base->get_flag(is_array_root_k) &&
                                                iter_base->cardinal_m != 0 &&
                                                iter_base->shuffle_m);

        if (!used_atom && !shuffle_array)
            continue;

        // takes time to build this - only do so when needed.
        std::string path(build_path(root, iter_base));

        if (used_atom)
        {
            result.push_back(attack_vector_t(attack_vector_t::type_atom_usage_k,
                                             path,
                                             iter_base,
                                             use_count));
        }
        else if (shuffle_array)
        {
            result.push_back(attack_vector_t(attack_vector_t::type_array_shuffle_k,
                                             path,
                                             iter_base));
        }
    }

    adobe::sort(result);

    std::cerr << "Scanned " << node_count << " nodes\n";

    return result;
}

/****************************************************************************************************/
