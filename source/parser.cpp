/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

// identity
#include <binspector/parser.hpp>

// boost
#include <boost/filesystem/fstream.hpp>

// asl
#include <adobe/algorithm/find.hpp>
#include <adobe/algorithm/sorted.hpp>
#include <adobe/implementation/token.hpp>
#include <adobe/array.hpp>
#include <adobe/string.hpp>

// application
#include <binspector/string.hpp>

/**************************************************************************************************/

namespace {

/**************************************************************************************************/
#if 0
#pragma mark -
#endif
/*************************************************************************************************/

CONSTANT_KEY(big);
CONSTANT_KEY(const);
CONSTANT_KEY(default);
CONSTANT_KEY(delimiter);
CONSTANT_KEY(die);
CONSTANT_KEY(else);
CONSTANT_KEY(enumerate);
CONSTANT_KEY(float);
CONSTANT_KEY(if);
CONSTANT_KEY(include);
CONSTANT_KEY(invariant);
CONSTANT_KEY(invis);
CONSTANT_KEY(little);
CONSTANT_KEY(noprint);
CONSTANT_KEY(notify);
CONSTANT_KEY(sentry);
CONSTANT_KEY(shuffle);
CONSTANT_KEY(signal);
CONSTANT_KEY(signed);
CONSTANT_KEY(skip);
CONSTANT_KEY(slot);
CONSTANT_KEY(struct);
CONSTANT_KEY(summary);
CONSTANT_KEY(terminator);
CONSTANT_KEY(typedef);
CONSTANT_KEY(unsigned);
CONSTANT_KEY(while);

adobe::name_t keyword_table[] =
{
    key_big,
    key_const,
    key_default,
    key_delimiter,
    key_die,
    key_else,
    key_enumerate,
    key_float,
    key_if,
    key_include,
    key_invariant,
    key_invis,
    key_little,
    key_noprint,
    key_notify,
    key_sentry,
    key_shuffle,
    key_signal,
    key_signed,
    key_skip,
    key_slot,
    key_struct,
    key_summary,
    key_terminator,
    key_typedef,
    key_unsigned,
    key_while,
};

/*************************************************************************************************/

bool keyword_lookup(const adobe::name_t& name)
{
#ifndef NDEBUG
    static bool inited_s = false;
    if (!inited_s)
    {
        assert(adobe::is_sorted(keyword_table));
        inited_s = true;
    }
#endif

    return binary_search(keyword_table, name, adobe::less(), adobe::constructor<adobe::name_t>()) != boost::end(keyword_table);
}

/**************************************************************************************************/

template <typename T>
struct temp_assign_and_call
{
    template <typename U>
    temp_assign_and_call(T& variable, const T& value, U callback) :
        variable_m(variable),
        old_value_m(variable),
        proc_m(callback)
    {
        variable_m = value;

        call();
    }

    ~temp_assign_and_call()
    {
        variable_m = old_value_m;

        call();
    }

private:
    void call()
    {
        if (proc_m)
            proc_m(variable_m);
    }

    T&                             variable_m;
    T                              old_value_m;
    std::function<void (const T&)> proc_m;
};

/**************************************************************************************************/

} // namespace

/**************************************************************************************************/

std::string get_input_line(std::istream& stream, std::streampos position)
{
    static std::vector<char> line_buffer(1024, 0);

    stream.clear();
    stream.seekg(position);
    stream.getline(&line_buffer[0], line_buffer.size());

    return std::string(&line_buffer[0], &line_buffer[static_cast<std::size_t>(stream.gcount())]);
}

/**************************************************************************************************/

binspector_parser_t::binspector_parser_t(std::istream&                   in,
                                         const adobe::line_position_t&   position,
                                         const include_directory_set_t&  include_directory_set,
                                         const set_structure_proc_t&     set_structure_proc,
                                         const add_field_proc_t&         add_field_proc,
                                         const add_unnamed_field_proc_t& add_unnamed_field_proc,
                                         const add_typedef_proc_t&       add_typedef_proc,
                                         const included_file_set_t&      included_file_set) :
    adobe::expression_parser(in, position),
    include_directory_set_m(include_directory_set),
    included_file_set_m(included_file_set),
    set_structure_proc_m(set_structure_proc),
    add_field_proc_m(add_field_proc),
    add_unnamed_field_proc_m(add_unnamed_field_proc),
    add_typedef_proc_m(add_typedef_proc)
{
    if (!set_structure_proc_m)
        throw std::runtime_error("A set structure callback is required");

    if (!add_field_proc_m)
        throw std::runtime_error("An add field callback is required");

    if (!add_unnamed_field_proc_m)
        throw std::runtime_error("An add unnamed field callback is required");

    if (!add_typedef_proc_m)
        throw std::runtime_error("An add typedef callback is required");

    set_keyword_extension_lookup(std::bind(&keyword_lookup,
                                           std::placeholders::_1));

    set_comment_bypass(true);
}

/**************************************************************************************************/

void binspector_parser_t::parse()
{
    try
    {
        if (!is_struct_set())
            throw_exception("Format description must not be empty");

        require_token(adobe::eof_k);
    }
    catch (const adobe::stream_error_t& error)
    {
        // Necessary to keep stream_error_t from being caught by the next catch
        throw error;
    }
    catch (const std::exception& error)
    {
        putback();

        throw adobe::stream_error_t(error, next_position());
    }
}

/**************************************************************************************************/

bool binspector_parser_t::is_struct_set()
{
    bool result = false;

    while (is_struct() || is_pp_statement())
        result = true;

    return result;
}

/**************************************************************************************************/

bool binspector_parser_t::is_struct()
{
    if (!is_keyword(key_struct))
        return false;

    adobe::name_t structure_name;

    require_identifier(structure_name);

    require_token(adobe::open_brace_k);

    temp_assign_and_call<adobe::name_t> tmp(current_struct_m, structure_name, set_structure_proc_m);

    is_statement_set();

    require_token(adobe::close_brace_k);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_statement_set()
{
    bool result = false;

    while (is_scope_or_statement())
        result = true;

    return result;
}

/**************************************************************************************************/

bool binspector_parser_t::is_scope_or_statement()
{
    return is_conditional_scope() ||
           is_enum_scope() ||
           is_sentry_scope() ||
           is_statement();
}

/**************************************************************************************************/

bool binspector_parser_t::is_conditional_scope()
{
    if (!is_if_scope())
        return false;

    is_else_scope();

    return true;
}

/**************************************************************************************************/

void binspector_parser_t::insert_parser_metadata(adobe::dictionary_t& parameters)
{
    const adobe::line_position_t& line_position(next_position());

    parameters[key_parse_info_filename].assign(line_position.stream_name());
    parameters[key_parse_info_line_number].assign(line_position.line_number_m);
}

/**************************************************************************************************/

adobe::name_t binspector_parser_t::make_lambda_identifier(const char* type, std::size_t id)
{
    return adobe::name_t(make_string(current_struct_m.c_str(),
                                     ":",
                                     type,
                                     "_",
                                     boost::lexical_cast<std::string>(id)).c_str());
}

/**************************************************************************************************/

bool binspector_parser_t::is_if_scope()
{
    if (!is_keyword(key_if))
        return false;

    adobe::array_t expression;

    require_token(adobe::open_parenthesis_k);

    require_expression(expression);

    require_token(adobe::close_parenthesis_k);

    static std::size_t          uid_s(0);
    static const adobe::array_t empty_array_k;

    adobe::name_t       identifier(make_lambda_identifier("if", ++uid_s));
    adobe::dictionary_t parameters;

    parameters[key_field_if_expression].assign(expression);
    parameters[key_field_conditional_type].assign(conditional_expression_t(if_k));
    parameters[key_field_name].assign(identifier);
    parameters[key_field_size_expression].assign(empty_array_k);
    parameters[key_field_offset_expression].assign(empty_array_k);
    parameters[key_field_type].assign(value_field_type_struct);
    parameters[key_named_type_name].assign(identifier);

    insert_parser_metadata(parameters);
    add_field_proc_m(identifier, parameters);

    require_scope_content(identifier);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_else_scope()
{
    if (!is_keyword(key_else))
        return false;

    static std::size_t          uid_s(0);
    static const adobe::array_t empty_array_k;

    adobe::name_t       identifier(make_lambda_identifier("else", ++uid_s));
    adobe::dictionary_t parameters;

    parameters[key_field_if_expression].assign(empty_array_k);
    parameters[key_field_conditional_type].assign(conditional_expression_t(else_k));
    parameters[key_field_name].assign(identifier);
    parameters[key_field_size_expression].assign(empty_array_k);
    parameters[key_field_offset_expression].assign(empty_array_k);
    parameters[key_field_type].assign(value_field_type_struct);
    parameters[key_named_type_name].assign(identifier);

    insert_parser_metadata(parameters);
    add_field_proc_m(identifier, parameters);

    require_scope_content(identifier);

    return true;
}

/**************************************************************************************************/

void binspector_parser_t::require_scope_content(adobe::name_t scope_name)
{
    temp_assign_and_call<adobe::name_t> tmp(current_struct_m, scope_name, set_structure_proc_m);

    if (is_token(adobe::open_brace_k))
    {
        is_statement_set();

        require_token(adobe::close_brace_k);
    }
    else if (!is_scope_or_statement())
    {
        throw_exception("Expected scope or statement");
    }
}

/**************************************************************************************************/

bool binspector_parser_t::is_enum_scope()
{
    if (!is_keyword(key_enumerate))
        return false;

    require_token(adobe::open_parenthesis_k);

    adobe::array_t expression;
    require_expression(expression);

    require_token(adobe::close_parenthesis_k);

    static std::size_t          uid_s(0);
    static const adobe::array_t empty_array_k;

    adobe::name_t       identifier(make_lambda_identifier("enumerate", ++uid_s));
    adobe::dictionary_t parameters;

    parameters[key_enumerated_expression].assign(expression);
    parameters[key_field_name].assign(identifier);
    parameters[key_field_size_expression].assign(empty_array_k);
    parameters[key_field_offset_expression].assign(empty_array_k);
    parameters[key_field_type].assign(value_field_type_enumerated);
    parameters[key_named_type_name].assign(identifier);

    insert_parser_metadata(parameters);
    add_field_proc_m(identifier, parameters);

    require_enum_content(identifier);

    return true;
}

/**************************************************************************************************/

void binspector_parser_t::require_enum_content(adobe::name_t scope_name)
{
    temp_assign_and_call<adobe::name_t> tmp(current_struct_m, scope_name, set_structure_proc_m);

    if (!is_enum_entry_map() && !is_enum_entry_list())
        throw_exception("Expected an enumerate list or map, but found neither");
}

/**************************************************************************************************/

bool binspector_parser_t::is_enum_entry_list()
{
    if (!is_token(adobe::open_bracket_k))
        return false;

    if (!is_enum_list_item_set())
        throw_exception("Enumerate list must have at least one option");

    require_token(adobe::close_bracket_k);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_enum_list_item_set()
{
    if (!is_enum_list_item())
        return false;

    while (is_token(adobe::comma_k))
        if (!is_enum_list_item())
            throw_exception("Expected an enumerate list item after the comma");

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_enum_list_item()
{
    adobe::array_t expression;

    if (!is_expression(expression))
        return false;

    static std::size_t          uid_s(0);
    static const adobe::array_t empty_array_k;

    adobe::name_t       identifier(make_lambda_identifier("list_option", ++uid_s));
    adobe::dictionary_t parameters;

    parameters[key_enumerated_option_expression].assign(expression);
    parameters[key_field_name].assign(identifier);
    parameters[key_field_size_expression].assign(empty_array_k);
    parameters[key_field_offset_expression].assign(empty_array_k);
    parameters[key_field_type].assign(value_field_type_enumerated_option);
    parameters[key_named_type_name].assign(identifier);

    insert_parser_metadata(parameters);
    add_field_proc_m(identifier, parameters);

    // This call adds an empty structure to the structure map. Remember the list
    // is intended to be syntactic sugar, so this kind of "hack" is OK.
    temp_assign_and_call<adobe::name_t> tmp(current_struct_m,
                                            identifier,
                                            set_structure_proc_m);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_enum_entry_map()
{
    if (!is_token(adobe::open_brace_k))
        return false;

    if (!is_enum_map_item_set())
        throw_exception("Enumerate map must have at least one option");

    // must be last in the enumerate construct!
    is_enum_map_default();

    require_token(adobe::close_brace_k);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_enum_map_item_set()
{
    bool result = false;

    while (is_enum_map_item())
        result = true;

    return result;
}

/**************************************************************************************************/

bool binspector_parser_t::is_enum_map_item()
{
    adobe::array_t expression;

    if (!is_expression(expression))
        return false;

    require_token(adobe::colon_k);

    static std::size_t          uid_s(0);
    static const adobe::array_t empty_array_k;

    adobe::name_t       identifier(make_lambda_identifier("map_option", ++uid_s));
    adobe::dictionary_t parameters;

    parameters[key_enumerated_option_expression].assign(expression);
    parameters[key_field_name].assign(identifier);
    parameters[key_field_size_expression].assign(empty_array_k);
    parameters[key_field_offset_expression].assign(empty_array_k);
    parameters[key_field_type].assign(value_field_type_enumerated_option);
    parameters[key_named_type_name].assign(identifier);

    insert_parser_metadata(parameters);
    add_field_proc_m(identifier, parameters);

    require_scope_content(identifier);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_enum_map_default()
{
    if (!is_keyword(key_default))
        return false;

    require_token(adobe::colon_k);

    static std::size_t          uid_s(0);
    static const adobe::array_t empty_array_k;

    adobe::name_t       identifier(make_lambda_identifier("default", ++uid_s));
    adobe::dictionary_t parameters;

    parameters[key_field_name].assign(identifier);
    parameters[key_field_size_expression].assign(empty_array_k);
    parameters[key_field_offset_expression].assign(empty_array_k);
    parameters[key_field_type].assign(value_field_type_enumerated_default);
    parameters[key_named_type_name].assign(identifier);

    insert_parser_metadata(parameters);
    add_field_proc_m(identifier, parameters);

    require_scope_content(identifier);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_sentry_scope()
{
    if (!is_keyword(key_sentry))
        return false;

    require_token(adobe::open_parenthesis_k);

    adobe::array_t expression;
    require_expression(expression);

    require_token(adobe::close_parenthesis_k);

    static std::size_t          uid_s(0);
    static const adobe::array_t empty_array_k;

    adobe::name_t       identifier(make_lambda_identifier("sentry", ++uid_s));
    adobe::dictionary_t parameters;

    parameters[key_sentry_expression].assign(expression);
    parameters[key_field_name].assign(identifier);
    parameters[key_field_size_expression].assign(empty_array_k);
    parameters[key_field_offset_expression].assign(empty_array_k);
    parameters[key_field_type].assign(value_field_type_sentry);
    parameters[key_named_type_name].assign(identifier);

    insert_parser_metadata(parameters);
    add_field_proc_m(identifier, parameters);

    require_scope_content(identifier);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_statement()
{
    bool success(is_typedef() || is_unnamed_statement() || is_named_statement());

    if (success)
        require_token(adobe::semicolon_k);

    return success;
}

/**************************************************************************************************/

bool binspector_parser_t::is_unnamed_statement()
{
    return is_notify() || is_summary() || is_die();
}

/**************************************************************************************************/

bool binspector_parser_t::is_named_statement()
{
    return is_invariant() ||
           is_constant()  ||
           is_skip()      ||
           is_slot()      ||
           is_signal()    ||
           is_field(); // field should be last because atoms only
                       // require an expression which most everything
                       // falls into; the more explicit stuff should 
                       // come first.
}

/**************************************************************************************************/

bool binspector_parser_t::is_field_type(adobe::name_t&    named_field_identifier,
                                        atom_base_type_t& atom_base_type,
                                        adobe::array_t&   bit_count_expression,
                                        adobe::array_t&   is_big_endian_expression)
{
    if (is_named_field(named_field_identifier))
        return true;

    if (is_atom_field(atom_base_type,
                      bit_count_expression,
                      is_big_endian_expression))
        return true;

    return false;
}

/**************************************************************************************************/

bool binspector_parser_t::is_named_field(adobe::name_t& named_field_identifier)
{
    return is_identifier(named_field_identifier);
}

/**************************************************************************************************/

bool binspector_parser_t::is_atom_field(atom_base_type_t& atom_base_type,
                                        adobe::array_t&   bit_count_expression,
                                        adobe::array_t&   is_big_endian_expression)
{
    static const adobe::array_t is_little_endian_expression_k(1, adobe::any_regular_t(false));
    static const adobe::array_t is_big_endian_expression_k(1, adobe::any_regular_t(true));

    if (is_keyword(key_signed))
        atom_base_type = atom_signed_k;
    else if (is_keyword(key_unsigned))
        atom_base_type = atom_unsigned_k;
    else if (is_keyword(key_float))
        atom_base_type = atom_float_k;
    else
        return false;

    require_expression(bit_count_expression);

    if (is_keyword(key_little))
        is_big_endian_expression = is_little_endian_expression_k;
    else if (is_keyword(key_big))
        is_big_endian_expression = is_big_endian_expression_k;
    else
        require_expression(is_big_endian_expression);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_typedef()
{
    adobe::name_t    named_field_identifier;
    atom_base_type_t atom_type(atom_unknown_k);
    adobe::array_t   bit_count_expression;
    adobe::array_t   is_big_endian_expression;
    adobe::name_t    new_type_identifier;

    if (!is_keyword(key_typedef))
        return false;

    if (!is_field_type(named_field_identifier,
                       atom_type,
                       bit_count_expression,
                       is_big_endian_expression))
        throw_exception("Field type expected");

    require_identifier(new_type_identifier);

    adobe::dictionary_t parameters;

    parameters[key_field_name].assign(new_type_identifier);
    parameters[key_field_size_type].assign(field_size_none_k); // intentionally fixed
    parameters[key_field_size_expression].assign(adobe::array_t()); // intentionally fixed
    parameters[key_field_offset_expression].assign(adobe::array_t()); // intentionally fixed

    if (named_field_identifier != adobe::name_t())
    {
        parameters[key_field_type].assign(value_field_type_typedef_named);
        parameters[key_named_type_name].assign(named_field_identifier);
    }
    else
    {
        parameters[key_field_type].assign(value_field_type_typedef_atom);
        parameters[key_atom_base_type].assign(atom_type);
        parameters[key_atom_bit_count_expression].assign(bit_count_expression);
        parameters[key_atom_is_big_endian_expression].assign(is_big_endian_expression);
    }

    insert_parser_metadata(parameters);
    add_typedef_proc_m(new_type_identifier, parameters);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_invariant()
{
    return is_simple_assign_field(key_invariant, value_field_type_invariant);
}

/**************************************************************************************************/

bool binspector_parser_t::is_constant()
{
    bool is_const(is_keyword(key_const));
    bool is_invis(!is_const && is_keyword(key_invis));

    if (!is_const && !is_invis)
        return false;

    adobe::name_t name;

    require_identifier(name);

    require_token(adobe::assign_k);

    adobe::array_t expression;

    require_expression(expression);

    // REVISIT (fbrereto) :
    // I should generalize these into "decorators", add them to the grammar and
    // reduce is_constant into a forwarding of is_simple_assign_field
    bool noprint(false);

    if (is_keyword(key_noprint) || is_invis)
        noprint = true;

    adobe::dictionary_t parameters;

    parameters[key_field_type].assign(value_field_type_const);
    parameters[key_field_name].assign(name);
    parameters[key_const_expression].assign(expression);
    parameters[key_const_no_print].assign(noprint);

    insert_parser_metadata(parameters);
    add_field_proc_m(name, parameters);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_notify()
{
    if (!is_keyword(key_notify))
        return false;

    adobe::array_t argument_list_expression;

    if (is_argument_list(argument_list_expression) == false)
        throw_exception("argument list required");

    adobe::dictionary_t parameters;

    parameters[key_field_type].assign(value_field_type_notify);
    parameters[key_field_name].assign(value_field_type_notify);
    parameters[key_notify_expression].assign(argument_list_expression);

    insert_parser_metadata(parameters);
    add_unnamed_field_proc_m(parameters);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_skip()
{
    if (!is_keyword(key_skip))
        return false;

    adobe::name_t name;

    require_identifier(name);

    require_token(adobe::open_bracket_k);

    adobe::array_t expression;

    require_expression(expression);

    require_token(adobe::close_bracket_k);

    adobe::dictionary_t parameters;

    parameters[key_field_type].assign(value_field_type_skip);
    parameters[key_field_name].assign(name);
    parameters[key_skip_expression].assign(expression);

    insert_parser_metadata(parameters);
    add_field_proc_m(name, parameters);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_field()
{
    adobe::name_t    named_field_identifier;
    atom_base_type_t atom_type(atom_unknown_k);
    adobe::array_t   bit_count_expression;
    adobe::array_t   is_big_endian_expression;
    bool             named_field(is_named_field(named_field_identifier));
    bool             atom_field(!named_field &&
                                is_atom_field(atom_type,
                                              bit_count_expression,
                                              is_big_endian_expression));

    if (!named_field && !atom_field)
        return false;

    adobe::name_t field_identifier;

    require_identifier(field_identifier);

    adobe::array_t field_size_expression;
    field_size_t   field_size_type(field_size_none_k);
    adobe::array_t offset_expression;
    adobe::array_t atom_invariant_expression;
    adobe::array_t callback_expression;
    bool           shuffleable(false);

    is_field_size(field_size_type, field_size_expression, shuffleable); // optional
    is_offset(offset_expression); // optional
    is_atom_invariant(atom_invariant_expression); // optional

    try
    {
        static const adobe::array_t empty_array_k;
        adobe::dictionary_t         parameters;

        parameters[key_field_name].assign(field_identifier);
        parameters[key_field_size_type].assign(field_size_type);
        parameters[key_field_size_expression].assign(field_size_expression);
        parameters[key_field_offset_expression].assign(offset_expression);
        parameters[key_field_atom_invariant_expression].assign(atom_invariant_expression);
        parameters[key_field_shuffle].assign(shuffleable);

        // add the field to the current structure description
        if (named_field)
        {
            parameters[key_field_type].assign(value_field_type_named);
            parameters[key_named_type_name].assign(named_field_identifier);
        }
        else
        {
            parameters[key_field_type].assign(value_field_type_atom);
            parameters[key_atom_base_type].assign(atom_type);
            parameters[key_atom_bit_count_expression].assign(bit_count_expression);
            parameters[key_atom_is_big_endian_expression].assign(is_big_endian_expression);
        }

        insert_parser_metadata(parameters);
        add_field_proc_m(field_identifier, parameters);
    }
    catch (const std::exception& error)
    {
        putback();

        throw adobe::stream_error_t(error, next_position());
    }

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_field_size(field_size_t&   field_size_type,
                                        adobe::array_t& field_size_expression,
                                        bool&           shuffleable)
{
    if (!is_token(adobe::open_bracket_k))
        return false;

    if (is_keyword(key_while))
    {
        field_size_type = field_size_while_k;

        require_token(adobe::colon_k);
    }
    else if (is_keyword(key_terminator))
    {
        field_size_type = field_size_terminator_k;

        require_token(adobe::colon_k);
    }
    else if (is_keyword(key_delimiter))
    {
        field_size_type = field_size_delimiter_k;

        require_token(adobe::colon_k);
    }
    else
    {
        field_size_type = field_size_integer_k;
    }

    require_expression(field_size_expression);

    require_token(adobe::close_bracket_k);

    shuffleable = is_keyword(key_shuffle);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_slot()
{
    return is_simple_assign_field(key_slot, value_field_type_slot);
}

/**************************************************************************************************/

bool binspector_parser_t::is_signal()
{
    return is_simple_assign_field(key_signal, value_field_type_signal);
}

/**************************************************************************************************/

bool binspector_parser_t::is_offset(adobe::array_t& offset_expression)
{
    if (!is_token(adobe::at_k))
        return false;

    require_expression(offset_expression);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_atom_invariant(adobe::array_t& atom_invariant_expression)
{
    if (!is_token(adobe::is_k))
        return false;

    require_expression(atom_invariant_expression);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_summary()
{
    if (!is_keyword(key_summary))
        return false;

    adobe::array_t argument_list_expression;

    if (is_argument_list(argument_list_expression) == false)
        throw_exception("argument list required");

    adobe::dictionary_t parameters;

    parameters[key_field_type].assign(value_field_type_summary);
    parameters[key_field_name].assign(value_field_type_summary);
    parameters[key_summary_expression].assign(argument_list_expression);

    insert_parser_metadata(parameters);
    add_unnamed_field_proc_m(parameters);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_die()
{
    if (!is_keyword(key_die))
        return false;

    adobe::array_t argument_list_expression;

    if (is_argument_list(argument_list_expression) == false)
        throw_exception("argument list required");

    adobe::dictionary_t parameters;

    parameters[key_field_type].assign(value_field_type_die);
    parameters[key_field_name].assign(value_field_type_die);
    parameters[key_die_expression].assign(argument_list_expression);

    insert_parser_metadata(parameters);
    add_unnamed_field_proc_m(parameters);

    return true;
}

/**************************************************************************************************/

bool binspector_parser_t::is_pp_statement()
{    
    return is_pp_include();
}

/**************************************************************************************************/

bool binspector_parser_t::is_pp_include()
{
    if (!is_keyword(key_include))
        return false;

    adobe::any_regular_t value;

    require_token(adobe::string_k, value);

    std::string value_str(value.cast<std::string>());

    // std::cerr << "Include file " << value_str << '\n';

    // do the parse; so far we don't support include directories; at this point
    // it'll get complicated when it does.
    //
    // we also need to track which files we include so we do not include them twice.

    boost::filesystem::path parsepath(include_directory_set_m[0] / value_str.c_str());

    // REVISIT (fbrereto) : A std::string to a c-string to a std::string to a... c'mon.
    if (!exists(parsepath))
        throw_exception(make_string("Could not find requested include file: ",
                                    value_str.c_str()).c_str());

    // check if file has already been parsed and added to the AST.
    if (adobe::find(included_file_set_m, parsepath) != included_file_set_m.end())
        return true;

    included_file_set_m.push_back(parsepath);

    boost::filesystem::ifstream            include_stream(parsepath);
    adobe::line_position_t::getline_proc_t getline(new adobe::line_position_t::getline_proc_impl_t(std::bind(&get_input_line, std::ref(include_stream), std::placeholders::_2)));
    adobe::line_position_t                 position(adobe::name_t(parsepath.string().c_str()), getline);

    try
    {
        binspector_parser_t(include_stream,
                            position,
                            include_directory_set_m,
                            set_structure_proc_m,
                            add_field_proc_m,
                            add_unnamed_field_proc_m,
                            add_typedef_proc_m,
                            included_file_set_m).parse();
    }
    catch (const adobe::stream_error_t& error)
    {
        throw std::runtime_error(adobe::format_stream_error(include_stream, error));
    }

    return true;
}

/**************************************************************************************************/

void binspector_parser_t::require_identifier(adobe::name_t& name_result)
{
    const adobe::stream_lex_token_t& result (get_token());
    
    if (result.first != adobe::identifier_k)
        throw_exception(adobe::identifier_k, result.first);

    name_result = result.second.cast<adobe::name_t>();
}

/**************************************************************************************************/

bool binspector_parser_t::is_simple_assign_field(adobe::name_t keyword, adobe::name_t field_type)
{
    if (!is_keyword(keyword))
        return false;

    adobe::name_t name;

    require_identifier(name);

    require_token(adobe::assign_k);

    adobe::array_t expression;

    require_expression(expression);

    adobe::dictionary_t parameters;

    parameters[key_field_type].assign(field_type);
    parameters[key_field_name].assign(name);
    parameters[key_field_assign_expression].assign(expression);

    parameters[key_field_size_type].assign(field_size_none_k); // intentionally fixed
    parameters[key_field_size_expression].assign(adobe::array_t()); // intentionally fixed
    parameters[key_field_offset_expression].assign(adobe::array_t()); // intentionally fixed

    insert_parser_metadata(parameters);
    add_field_proc_m(name, parameters);

    return true;
}

/**************************************************************************************************/
