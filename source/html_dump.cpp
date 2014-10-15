/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// identity
#include <binspector/html_dump.hpp>

// stdc++
#include <fstream>

// application
#include <binspector/bitreader.hpp>
#include <binspector/common.hpp>

/****************************************************************************************************/

namespace {

/****************************************************************************************************/

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

void detail_struct_or_array_node(bitreader_t&         /*input*/,
                                 std::ostream&        output,
                                 inspection_forest_t& forest,
                                 inspection_branch_t  struct_node)
{
    bool is_array_root(struct_node->get_flag(is_array_root_k));
    bool is_struct(node_property(struct_node, NODE_PROPERTY_IS_STRUCT));

    output << "path: " << build_path(forest.begin(), struct_node) << "<br/>"
             << "type: " << (is_array_root ? "array" : "struct") << "<br/>";

    if (is_struct)
        output << "struct: " << node_property(struct_node, STRUCT_PROPERTY_STRUCT_NAME) << "<br/>";

    if (is_array_root)
        output << "elements: " << node_property(struct_node, ARRAY_ROOT_PROPERTY_SIZE) << "<br/>";

    inspection_forest_t::child_iterator first_child(adobe::child_begin(struct_node));
    inspection_forest_t::child_iterator last_child(adobe::child_end(struct_node));

    if (first_child == last_child)
        return;

    inspection_position_t start_byte_offset(starting_offset_for(struct_node));
    inspection_position_t end_byte_offset(ending_offset_for(struct_node));
    inspection_position_t size(end_byte_offset - start_byte_offset + inspection_byte_k);
    boost::uint64_t       bytes(size.bytes());

    output << "bytes: [ " << start_byte_offset << " .. " << end_byte_offset << " ]<br/>"
             << "size: " << bytes << " bytes";

    if (bytes >= 1024)
        output << " (" << bytes / 1024.0 << " KB)";

    if (bytes >= 1024 * 1024)
        output << " (" << bytes / (1024.0 * 1024) << " MB)";

    if (bytes >= 1024 * 1024 * 1024)
        output << " (" << bytes / (1024.0 * 1024 * 1024) << " GB)";

    output << "<br/>";
}

/****************************************************************************************************/

void detail_atom_node(bitreader_t&         input,
                      std::ostream&        output,
                      inspection_forest_t& forest,
                      inspection_branch_t  atom_node)
{
    atom_base_type_t      base_type(node_property(atom_node, ATOM_PROPERTY_BASE_TYPE));
    bool                  is_big_endian(node_property(atom_node, ATOM_PROPERTY_IS_BIG_ENDIAN));
    boost::uint64_t       bit_count(node_property(atom_node, ATOM_PROPERTY_BIT_COUNT));
    inspection_position_t position(node_value(atom_node, ATOM_VALUE_LOCATION));
    rawbytes_t            raw(input.read_bits(position, bit_count));
    adobe::any_regular_t  value(evaluate(raw, bit_count, base_type, is_big_endian));

    output << "path: " << build_path(forest.begin(), atom_node) << "<br/>"
           << "format: " << bit_count << "-bit "
                            << (base_type == atom_signed_k   ? "signed" :
                                base_type == atom_unsigned_k ? "unsigned" :
                                base_type == atom_float_k    ? "float" :
                                                               "<ERR>") << ' '
                            << (bit_count > 8 ? is_big_endian ? "big-endian" : "little-endian" : "")
                            << "<br/>"
           << "offset: "
           << position
           << "<br/>";

    output << "raw: " << std::hex;
    for (std::size_t i(0); i < raw.size(); ++i)
    {
        output << "0x";
        output.width(2);
        output.fill('0');
        output << static_cast<int>(raw[i]) << ' ';
    }
    output << std::dec << "<br/>";

    output << "value: " << std::dec;
    stream_out(value, bit_count, base_type, output);
    output << " (0x" << std::hex;
    stream_out(value, bit_count, base_type, output);
    output << ")" << std::dec;
}

/****************************************************************************************************/

void detail_skip(bitreader_t&         /*input*/,
                 std::ostream&        output,
                 inspection_forest_t& forest,
                 inspection_branch_t  skip_node)
{
    // inspection_position_t position(node_value(skip_node, SKIP_VALUE_LOCATION));

    output << "path: " << build_path(forest.begin(), skip_node) << "<br/>"
           << "type: skip<br/>";

    inspection_position_t start_byte_offset(starting_offset_for(skip_node));
    inspection_position_t end_byte_offset(ending_offset_for(skip_node));
    inspection_position_t size(end_byte_offset - start_byte_offset + inspection_byte_k);

    output << "bytes: [ " << start_byte_offset << " .. " << end_byte_offset << " ]<br/>";
    output << "size: " << size.bytes() << " bytes";
}

/****************************************************************************************************/

void detail_node(bitreader_t&         input,
                 std::ostream&        output,
                 inspection_forest_t& forest,
                 inspection_branch_t  node)
{
    bool is_struct(node_property(node, NODE_PROPERTY_IS_STRUCT));
    bool is_skip(node_property(node, NODE_PROPERTY_IS_SKIP));
    bool is_atom(node_property(node, NODE_PROPERTY_IS_ATOM));
    bool is_array_root(node->get_flag(is_array_root_k));

    if (is_struct || is_array_root)
    {
        detail_struct_or_array_node(input, output, forest, node);
    }
    else if (is_skip)
    {
        detail_skip(input, output, forest, node);
    }
    else if (is_atom)
    {
        detail_atom_node(input, output, forest, node);
    }
    else
    {
        output << "No details available";
    }
}

/****************************************************************************************************/

void print_node(bitreader_t&          input,
                std::ostream&         output,
                inspection_forest_t&  forest,
                depth_full_iterator_t iterator)
{
    inspection_branch_t       branch(iterator.base());
    std::size_t               depth(iterator.depth());
    const_inspection_branch_t pbranch(property_node_for(branch));
    bool                      is_atom(pbranch->get_flag(type_atom_k));
    bool                      is_const(pbranch->get_flag(type_const_k));
    bool                      is_struct(pbranch->get_flag(type_struct_k));
    bool                      is_skip(pbranch->get_flag(type_skip_k));
    bool                      is_slot(pbranch->get_flag(type_slot_k));
    adobe::name_t             name(pbranch->name_m);
    bool                      is_array_element(branch->get_flag(is_array_element_k)); // not pbranch!
    bool                      is_array_root(branch->get_flag(is_array_root_k)); // not pbranch!
    bool                      is_noprint(is_slot ||
                                         (is_const && node_value(branch, CONST_VALUE_NO_PRINT)));

    if (is_noprint)
        return;

    if (branch.edge() == adobe::forest_leading_edge)
    {
        if (depth == 0)
            output << "<li class='open'>";
        else if (is_struct || is_array_root)
            output << "<li class='collapsed'>";
        else
            output << "<li class='atomic'>";

        if (is_struct)
        {
            adobe::name_t struct_name(node_property(pbranch, STRUCT_PROPERTY_STRUCT_NAME));

            output << '(' << struct_name << ") ";
        }
        else if (is_atom)
        {
            atom_base_type_t      base_type(node_property(pbranch, ATOM_PROPERTY_BASE_TYPE));
            bool                  is_big_endian(node_property(pbranch, ATOM_PROPERTY_IS_BIG_ENDIAN));
            boost::uint64_t       bit_count(node_property(pbranch, ATOM_PROPERTY_BIT_COUNT));
            // inspection_position_t position(node_value(branch, ATOM_VALUE_LOCATION));

            output << '('
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
            output << "(const) ";
        }
        else if (is_skip)
        {
            output << "(skip) ";
        }

        output << "<span class='tip_region'><span class='detailed'>";

        if (is_array_root)
        {
            boost::uint64_t size(node_property(pbranch, ARRAY_ROOT_PROPERTY_SIZE));

            output << name << '[' << size << ']';
        }
        else if (is_array_element)
        {
            boost::uint64_t index(node_value(branch, ARRAY_ELEMENT_VALUE_INDEX));

            output << '[' << index << ']';
        }
        else if (is_skip)
        {
            inspection_position_t start_byte_offset(starting_offset_for(branch));
            inspection_position_t end_byte_offset(ending_offset_for(branch));
            inspection_position_t size(end_byte_offset - start_byte_offset + inspection_byte_k);

            output << name << '[' << size << ']';
        }
        else
        {
            output << name;
        }

        if (!branch->summary_m.empty())
        {
            output << " (" << branch->summary_m << ")";
        }

        output << "</span><span class='tip'>";

        detail_node(input, output, forest, branch);

        output << "</span></span>\n";

        if (is_const)
        {
            bool noprint(node_value(branch, CONST_VALUE_NO_PRINT));

            if (!noprint)
            {
                output << ": ";

                if (node_value(branch, CONST_VALUE_IS_EVALUATED))
                {
                    const adobe::any_regular_t& value(node_value(branch, CONST_VALUE_EVALUATED_VALUE));

                    // any_regular_t only knows how to serialize a handful of types;
                    // sadly that means we have to special-case those it cannot handle.

                    if (value.type_info() == typeid(inspection_position_t))
                        output << value.cast<inspection_position_t>();
                    else
                        output << value;
                }
                else
                {
                    const adobe::array_t& const_expression(node_value(branch, CONST_VALUE_EXPRESSION));
    
                    output << contextual_evaluation_of<adobe::any_regular_t>(const_expression,
                                                                             forest.begin(),
                                                                             adobe::find_parent(branch),
                                                                             input);
                }
            }
        }
        else if (is_atom && !is_array_root)
        {
            atom_base_type_t      base_type(node_property(pbranch, ATOM_PROPERTY_BASE_TYPE));
            bool                  is_big_endian(node_property(pbranch, ATOM_PROPERTY_IS_BIG_ENDIAN));
            boost::uint64_t       bit_count(node_property(pbranch, ATOM_PROPERTY_BIT_COUNT));
            inspection_position_t position(node_value(branch, ATOM_VALUE_LOCATION));

            output << ": ";

            stream_out(fetch_and_evaluate(input, position, bit_count, base_type, is_big_endian),
                       bit_count,
                       base_type,
                       output);
        }
        else if (is_struct ||
                 (is_array_root && node_property(pbranch, ARRAY_ROOT_PROPERTY_SIZE) != 0))
        {
            std::string ulclass = "alpha";

            if (depth % 2)
                ulclass = "beta";

            output << "<ul class=\"" << ulclass << "\">\n";
        }
    }
    else // trailing edge
    {
        if (is_struct ||
            (is_array_root && node_property(pbranch, ARRAY_ROOT_PROPERTY_SIZE) != 0))
        {
            output << "</ul>\n";
        }
        else
        {
            output << "</li>\n";
        }
    }
}

/****************************************************************************************************/

} // namespace

/****************************************************************************************************/
#if 0
#pragma mark -
#endif
/****************************************************************************************************/

void binspector_html_dump(std::istream&      input,
                          auto_forest_t      aforest,
                          std::ostream&      output,
                          const std::string& coutput,
                          const std::string& cerrput)
{
    inspection_forest_t& forest(*aforest);
    inspection_branch_t  begin(adobe::leading_of(forest.begin()));
    inspection_branch_t  end(adobe::trailing_of(forest.begin()));
    bitreader_t          bitreader(input);

    output << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n"
           << "<html>\n"
           << "<head>\n"
           << "<title>Binspector</title>\n"
           << "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"/>\n"
           << "<link rel=\"stylesheet\" type=\"text/css\" href=\"http://fonts.googleapis.com/css?family=Ubuntu|Ubuntu+Mono|Mate+SC\"/>\n"
           << "<link rel='stylesheet' type='text/css' href='http://ajax.googleapis.com/ajax/libs/jqueryui/1.8.16/themes/blitzer/jquery-ui.css'/>\n"
           << "<script type='text/javascript' src='https://ajax.googleapis.com/ajax/libs/jquery/1.7.0/jquery.min.js'></script>\n"
           << "<script type='text/javascript' src='https://ajax.googleapis.com/ajax/libs/jqueryui/1.8.16/jquery-ui.min.js'></script>\n"
           << "<script type='text/javascript'>$(function(){\n"
           << "    $('.tip_region').each(function()\n"
           << "        {\n"
           << "        var tip = $(this).find('.tip');\n"
           << "\n"
           << "        $(this).hover(\n"
           << "            function() { tip.appendTo('body'); },\n"
           << "            function() { tip.appendTo(this); }\n"
           << "        ).mousemove(function(e)\n"
           << "            {\n"
           << "            tip.css({ left: 0, top: $(window).scrollTop() });\n"
           << "            });\n"
           << "        });\n"
           << "    $('li').each(function()\n"
           << "        {\n"
           << "        $(this).click(function(e)\n"
           << "            {\n"
           << "            e.stopPropagation();\n"
           << "\n"
           << "            if ($(this).hasClass('atomic'))\n"
           << "                return;\n"
           << "\n"
           << "            if ($(this).hasClass('open'))\n"
           << "                {\n"
           << "                $(this).removeClass('open');\n"
           << "                $(this).addClass('collapsed');\n"
           << "                $(this).children().not(':first').hide();\n"
           << "                }\n"
           << "            else\n"
           << "                {\n"
           << "                $(this).removeClass('collapsed');\n"
           << "                $(this).addClass('open');\n"
           << "                $(this).children().show();\n"
           << "                }\n"
           << "            });\n"
           << "        });\n"
           << "});</script>\n"
           << "<style type='text/css'>\n"
           << "body\n"
           << "    {\n"
           << "    font-family: \"Ubuntu Mono\", Monaco, monospace;\n"
           << "    font-size: 14pt;\n"
           << "    }\n"
           << "ul\n"
           << "    {\n"
           << "    list-style: none;\n"
           << "    margin-left: 0;\n"
           << "    padding-left: 1em;\n"
           << "    text-indent: -1em;\n"
           << "    cursor: pointer;\n"
           << "    }\n"
           << "li\n"
           << "    {\n"
           << "    background: white;\n"
           << "    }\n"
           << "li:hover\n"
           << "    {\n"
           << "    background: #d0dafd;\n"
           << "    }\n"
           << ".collapsed:before\n"
           << "    {\n"
           << "    content: \"+   \";\n"
           << "    }\n"
           << ".open:before\n"
           << "    {\n"
           << "    content: \"-   \";\n"
           << "    }\n"
           << ".atomic\n"
           << "    {\n"
           << "    padding-left: 1em;\n"
           << "    }\n"
           << ".collapsed li\n"
           << "    {\n"
           << "    display: none;\n"
           << "    }\n"
           << "pre.stdout\n"
           << "    {\n"
           << "    background: #eee;\n"
           << "    padding: 10px;\n"
           << "    }\n"
           << "pre.stderr\n"
           << "    {\n"
           << "    background: #fbb;\n"
           << "    padding: 10px;\n"
           << "    }\n"
           << ".detailed\n"
           << "    {\n"
           << "    color: #339;\n"
           << "    }\n"
           << ".alpha\n"
           << "    {\n"
           << "    background: white;\n"
           << "    }\n"
           << ".beta\n"
           << "    {\n"
           << "    background: white;\n"
           << "    }\n"
           << ".tip\n"
           << "    {\n"
           << "    border: 2px solid black;\n"
           << "    background: yellow;\n"
           << "    padding: 10px;\n"
           << "    position: absolute;\n"
           << "    z-index: 1000;\n"
           << "    -webkit-border-radius: 3px;\n"
           << "    -moz-border-radius: 3px;\n"
           << "    border-radius: 3px;\n"
           << "    }\n"
           << ".tip_region .tip\n"
           << "    {\n"
           << "    display: none;\n"
           << "    }\n"
           << "</style>\n"
           << "</head>\n"
           << "<body>\n"
           << "<h1>Binary File Analysis</h1>\n"
           << "<h2>Notifications</h2>\n"
           << "<pre class='stdout'>\n"
           << (coutput.empty() ? std::string("none") : coutput)
           << "</pre>\n"
           << "<h2>Errors &amp; Warnings</h2>\n"
           << "<pre class='stderr'>\n"
           << (cerrput.empty() ? std::string("none") : cerrput)
           << "</pre>\n"
           << "<h2>Tree</h2>\n"
           << "<ul class='beta'>\n";

    for (depth_full_iterator_t iter(begin), last(++end); iter != last; ++iter)
        print_node(bitreader, output, forest, iter);

    output << "</ul>\n";
    output << "</body>\n";
    output << "</html>\n";
}

/****************************************************************************************************/
