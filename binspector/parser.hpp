/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/**************************************************************************************************/

#ifndef BINSPECTOR_PARSER_HPP
#define BINSPECTOR_PARSER_HPP

// asl
#include <adobe/config.hpp> // order dependency: needed for BOOST_WINDOWS

// stdc++
#include <functional>

// boost
#include <boost/filesystem.hpp>

// asl
#include <adobe/implementation/expression_parser.hpp>

// application
#include <binspector/common.hpp>

/**************************************************************************************************/
/*
    translation_unit   = { struct_set }
    struct_set         = [ struct | pp_statement ] { struct_set }
    struct             = "struct" identifier '{' { statement_set } '}'
    statement_set      = scope_or_statement { statement_set }
    scope_or_statement = conditional_scope | enum_scope | sentry_scope | statement
    conditional_scope  = if_scope { else_scope }
    if_scope           = "if" '(' expression ')' scope_content
    else_scope         = "else" scope_content
    scope_content      = '{' { statement_set } '}' | scope_or_statement
    enum_scope         = "enumerate" '(' expression ')' enum_content
    enum_content       = enum_entry_list | enum_entry_map
    enum_entry_list    = '[' enum_list_item_set ']'
    enum_list_item_set = enum_list_item { ',' enum_list_item_set }
    enum_list_item     = expression
    enum_entry_map     = '{' enum_map_item_set { enum_map_default } '}'
    enum_map_item_set  = enum_map_item { enum_map_item_set }
    enum_map_item      = expression ':' scope_content
    enum_map_default   = "default" ':' scope_content
    sentry_scope       = sentry '(' expression ')' scope_content
    statement          = [ typedef | unnamed_statement | named_statement ] ';'
    unnamed_statement  = [ notify | summary | die ]
    named_statement    = [ invariant | constant | skip | slot | signal | field ]
    typedef            = "typedef" field_type identifier
    field_type         = named_field | atom_field
    named_field        = identifier
    atom_field         = [ "float" | "unsigned" | "signed" ] expression [ "big" | "little" | expression ]
    notify             = "notify" argument_list
    summary            = "summary" argument_list
    die                = "die" argument_list
    invariant          = "invariant" identifier '=' expression
    constant           = [ "const" | "invis" ] identifier '=' expression { "noprint" }
    skip               = "skip" identifier '[' expression ']'
    field_size         = '[' { [ "while" | "terminator" | "delimiter" ] ':' } expression ']' { "shuffle" }
    slot               = "slot" identifier '=' expression
    signal             = "signal" identifier '=' expression
    field              = field_type identifier { field_size } { offset } { atom_invariant }
    atom_invariant     = '<==' expression
    offset             = '@' expression
    pp_statement       = pp_include
    pp_include         = "include" string
*/
/**************************************************************************************************/

class binspector_parser_t : protected adobe::expression_parser
{
public:
    typedef std::function<void (adobe::name_t)>              set_structure_proc_t;
    typedef std::function<void (adobe::name_t,
                                const adobe::dictionary_t&)> add_field_proc_t;
    typedef std::function<void (const adobe::dictionary_t&)> add_unnamed_field_proc_t;
    typedef std::function<void (adobe::name_t,
                                const adobe::dictionary_t&)> add_typedef_proc_t;
    typedef std::vector<boost::filesystem::path>             include_directory_set_t;
    typedef std::vector<boost::filesystem::path>             included_file_set_t;

    binspector_parser_t(std::istream&                   in,
                        const adobe::line_position_t&   position,
                        const include_directory_set_t&  include_directory_set,
                        const set_structure_proc_t&     set_structure_proc,
                        const add_field_proc_t&         add_field_proc,
                        const add_unnamed_field_proc_t& add_unnamed_field_proc,
                        const add_typedef_proc_t&       add_typedef_proc,
                        const included_file_set_t&      included_file_set = included_file_set_t());

//  translation_unit = { struct_set }.
    void parse();

private:
    bool is_struct_set();
    bool is_struct();
    bool is_statement_set();
    bool is_scope_or_statement();
    bool is_conditional_scope();
    bool is_if_scope();
    bool is_else_scope();
    void require_scope_content(adobe::name_t scope_name);
    bool is_enum_scope();
    void require_enum_content(adobe::name_t scope_name);
    bool is_enum_entry_list();
    bool is_enum_list_item_set();
    bool is_enum_list_item();
    bool is_enum_entry_map();
    bool is_enum_map_item_set();
    bool is_enum_map_item();
    bool is_enum_map_default();
    bool is_sentry_scope();
    bool is_statement();
    bool is_unnamed_statement();
    bool is_named_statement();
    bool is_field_type(adobe::name_t&    named_field_identifier,
                       atom_base_type_t& atom_base_type,
                       adobe::array_t&   bit_count_expression,
                       adobe::array_t&   is_big_endian_expression);
    bool is_named_field(adobe::name_t& field_identifier);
    bool is_atom_field(atom_base_type_t& atom_base_type,
                       adobe::array_t&   bit_count_expression,
                       adobe::array_t&   is_big_endian_expression);
    bool is_typedef();
    bool is_invariant();
    bool is_constant();
    bool is_notify();
    bool is_skip();
    bool is_field_size(field_size_t&   field_size_type,
                       adobe::array_t& field_size_expression,
                       bool&           shuffleable);
    bool is_slot();
    bool is_signal();
    bool is_field();
    bool is_atom_invariant(adobe::array_t& atom_invariant_expression);
    bool is_offset(adobe::array_t& offset_expression);
    bool is_summary();
    bool is_die();

    // "pp" is for "preprocessor", though technically it isn't one.
    bool is_pp_statement();
    bool is_pp_include();

    // a la require_token, etc.
    void require_identifier(adobe::name_t& name_result);

    // utility routine for slot, signal, etc.
    bool is_simple_assign_field(adobe::name_t keyword, adobe::name_t field_type);

    // utility routine to add parser metadata to the AST node
    void insert_parser_metadata(adobe::dictionary_t& parameters);

    include_directory_set_t  include_directory_set_m;
    included_file_set_t      included_file_set_m;
    set_structure_proc_t     set_structure_proc_m;
    add_field_proc_t         add_field_proc_m;
    add_unnamed_field_proc_t add_unnamed_field_proc_m;
    add_typedef_proc_t       add_typedef_proc_m;
    adobe::name_t            current_struct_m;
};

/**************************************************************************************************/

std::string get_input_line(std::istream& stream, std::streampos position);

/**************************************************************************************************/
// BINSPECTOR_PARSER_HPP
#endif

/**************************************************************************************************/
