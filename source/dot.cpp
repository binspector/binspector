/*
    Copyright 2014 Adobe
    Distributed under the Boost Software License, Version 1.0.
    (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*/
/****************************************************************************************************/

// stdc++
#include <iterator>
#include <iostream>
#include <fstream>

// identity
#include <binspector/common.hpp>
#include <binspector/dot.hpp>

// boost
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/fstream.hpp>

//asl
#include <adobe/algorithm/copy.hpp>
#include <adobe/algorithm/unique.hpp>
#include <adobe/implementation/expression_formatter.hpp>
#include <adobe/string.hpp>

// REVISIT (fbrereto) : There's a ton of string concatenation pessimizations in
//                      this file. They need to be cleaned up.

/****************************************************************************************************/

namespace {

/****************************************************************************************************/

typedef binspector_analyzer_t::structure_map_t::key_type       key_t; // name_t
typedef binspector_analyzer_t::structure_map_t::mapped_type    mapped_t; // array_t
typedef binspector_analyzer_t::structure_map_t::const_iterator iterator_t;
typedef mapped_t::const_iterator                               mapped_iterator_t;

/****************************************************************************************************/

std::string padded_id(const std::size_t x, std::size_t pad)
{
    std::string tmp(boost::lexical_cast<std::string>(x));
    std::size_t count(tmp.size());

    if (count >= pad)
        return tmp;

    std::string tmp2(pad - count, '0');

    return tmp2 += tmp;
}

/****************************************************************************************************/

bool coverage_check(adobe::name_t name)
{
    typedef std::set<adobe::name_t> coverage_set_t;

    static coverage_set_t coverage;

    if (coverage.find(name) != coverage.end())
        return true;

    coverage.insert(name);

    return false;
}

/****************************************************************************************************/

template <typename T>
inline const T& dvalue(const adobe::dictionary_t& dict, adobe::name_t key)
{
    return get_value(dict, key).cast<T>();
}

/****************************************************************************************************/

std::string field_expression(const adobe::dictionary_t& dict, adobe::name_t key)
{
    return adobe::format_expression(dvalue<adobe::array_t>(dict, key), 0, true);
}

/****************************************************************************************************/

adobe::name_t get_field_type(const adobe::dictionary_t& field)
{
    return dvalue<adobe::name_t>(field, key_field_type);
}

/****************************************************************************************************/

void replace_all(std::string& haystack, const std::string& needle, const std::string& new_needle)
{
    std::size_t found(0);

    while (true)
    {
        found = haystack.find(needle, found);

        if (found == std::string::npos)
            return;

        haystack.replace(found, needle.size(), new_needle);

        found += new_needle.size();
    }
}

/****************************************************************************************************/

std::string record_escape(const std::string& src)
{
    std::string result(src);

    replace_all(result, "|", "\\|");
    replace_all(result, "<", "\\<");
    replace_all(result, ">", "\\>");
    replace_all(result, "\"", "\\\"");

    return result;
}

/****************************************************************************************************/

conditional_expression_t get_conditional_type(const adobe::any_regular_t& any_field)
{
    const adobe::dictionary_t& field(any_field.cast<adobe::dictionary_t>());

    if (field.count(key_field_conditional_type) == 0)
        return none_k;

    return dvalue<conditional_expression_t>(field, key_field_conditional_type);
}

/****************************************************************************************************/

std::string name_map(adobe::name_t name)
{
    typedef std::map<adobe::name_t, std::string> map_t;
    typedef map_t::const_iterator                    const_iterator;

    static map_t map_s;

    const_iterator found(map_s.find(name));

    if (found != map_s.end())
        return found->second;

    std::string        tag("node_");
    static std::size_t uid_s(0);
    std::string        tagid(padded_id(uid_s++, 5));

    tag += tagid.c_str();

    map_s[name] = tag;

    return tag;
}

/****************************************************************************************************/

std::string spaces(std::size_t count)
{
    return std::string(count*2, ' ');
}

/****************************************************************************************************/

typedef std::map<std::string, std::string> dot_attribute_map_t;

struct dot_node_t
{
    dot_attribute_map_t attribute_map_m;
    std::string     id_m;
};

struct id_less_t
{
    typedef bool result_type;

    result_type operator() (const dot_node_t& x, const dot_node_t& y) const
    {
        return x.id_m < y.id_m;
    }
};

struct id_equal_t
{
    typedef bool result_type;

    result_type operator() (const dot_node_t& x, const dot_node_t& y) const
    {
        return x.id_m == y.id_m;
    }
};

typedef std::vector<dot_node_t>                     dot_node_vector_t;

struct dot_edge_t
{
    std::string     from_m;
    std::string     to_m;
    dot_attribute_map_t attribute_map_m;
};

inline bool operator<(const dot_edge_t& x, const dot_edge_t& y)
{
    return x.from_m < y.from_m ||
           (x.from_m == y.from_m && x.to_m < y.to_m) ||
           (x.from_m == y.from_m && x.to_m == y.to_m && x.attribute_map_m < y.attribute_map_m);
}

typedef std::set<dot_edge_t> dot_edge_set_t;

struct dot_cluster_t
{
    std::string   label_m;
    dot_node_vector_t node_vector_m;
};

typedef std::vector<dot_cluster_t>                 dot_cluster_vector_t;

struct dot_graph_t
{
    std::string              type_m;
    std::string              name_m;
    dot_node_vector_t            node_vector_m;
    dot_cluster_vector_t         cluster_vector_m;
    dot_edge_set_t               edge_set_m;
    std::vector<std::string> setting_vector_m;
};

/****************************************************************************************************/

void graph_struct(const binspector_analyzer_t::structure_map_t& struct_map,
                  key_t                                         name,
                  const std::string&                            parent_id,
                  binspector_analyzer_t::typedef_map_t          typedef_map, /*copy*/
                  std::size_t                                   depth,
                  dot_graph_t&                                  graph)
{
    if (coverage_check(name))
        return;

    iterator_t found(struct_map.find(name));

    if (found == struct_map.end())
        return;

    const mapped_t&    cur_struct(found->second);
    mapped_iterator_t  iter(cur_struct.begin());
    mapped_iterator_t  last(cur_struct.end());
    std::string        prior_if_id;
    std::string        prior_if_expression;

    for (; iter != last; ++iter)
    {
        adobe::dictionary_t field(iter->cast<adobe::dictionary_t>());
        adobe::name_t       field_name(dvalue<adobe::name_t>(field, key_field_name));
        adobe::name_t       field_type(get_field_type(field));

        if (field_type == value_field_type_typedef_atom ||
            field_type == value_field_type_typedef_named)
        {
            // add the typedef's field details to the typedef map; we're done
            typedef_map[field_name] = field;

            continue;
        }
        else if (field_type == value_field_type_named)
        {
            field = typedef_lookup(typedef_map, field);

            // re-set these values given that the field may have changed.
            field_name = dvalue<adobe::name_t>(field, key_field_name);
            field_type = get_field_type(field);
        }

        if (field_type == value_field_type_struct ||
            field_type == value_field_type_enumerated ||
            field_type == value_field_type_enumerated_option ||
            field_type == value_field_type_enumerated_default)
        {
            adobe::name_t            this_name(dvalue<adobe::name_t>(field, key_named_type_name));
            std::string              this_id(name_map(this_name));
            bool                     has_conditional_type(field.count(key_field_conditional_type) != 0);
            conditional_expression_t conditional_type(has_conditional_type ? dvalue<conditional_expression_t>(field, key_field_conditional_type) : none_k);
            bool                     is_conditional(conditional_type != none_k);
            std::string              struct_label(this_name.c_str());
            bool                     add_node(!is_conditional);

            if (field_type == value_field_type_enumerated)
            {
                std::string printed_name(field_expression(field, key_enumerated_expression));

                struct_label.clear();
                struct_label += "enum: " + printed_name;
            }
            else if (field_type == value_field_type_enumerated_option)
            {
                std::string printed_name(field_expression(field, key_enumerated_option_expression));

                struct_label = printed_name;
            }
            else if (field_type == value_field_type_enumerated_default)
            {
                struct_label = "default";
            }
            else if (conditional_type == if_k)
            {
                mapped_iterator_t  next_node(boost::next(iter));
                bool               next_is_else(next_node != last && get_conditional_type(*next_node) == else_k);
                std::string        printed_name(field_expression(field, key_field_if_expression));
                std::string        escaped_name(record_escape(printed_name));

                if (next_is_else)
                {
                    prior_if_id = this_id;
                    prior_if_expression = escaped_name;
                }
                else
                {
                    dot_node_t  node;
                    std::string label;
    
                    label += "{ <check>" + escaped_name + " | <true>YES }";
    
                    node.id_m = this_id;
                    node.attribute_map_m["shape"] = "record";
                    node.attribute_map_m["label"] = label;
    
                    graph.node_vector_m.push_back(node);    
                }

                dot_edge_t if_edge;
                if_edge.from_m = parent_id;
                if_edge.to_m += this_id + ":check";
                graph.edge_set_m.insert(if_edge);

                this_id += ":true";

                graph_struct(struct_map, this_name, this_id, typedef_map, depth + 1, graph);
            }
            else if (conditional_type == else_k)
            {
                dot_node_t  node;
                std::string label;

                label += "{ <check>" + prior_if_expression + " | { <true>YES | <false>NO } }";

                node.id_m = prior_if_id;
                node.attribute_map_m["shape"] = "record";
                node.attribute_map_m["label"] = label;

                graph.node_vector_m.push_back(node);

                this_id = prior_if_id + ":false";

                graph_struct(struct_map, this_name, this_id, typedef_map, depth + 1, graph);
            }

            if (add_node)
            {
                dot_node_t node;
                dot_edge_t edge;

                node.attribute_map_m["label"] = record_escape(struct_label);
                node.id_m = this_id;

                edge.from_m = parent_id;
                edge.to_m = this_id;

                graph.node_vector_m.push_back(node);
                graph.edge_set_m.insert(edge);

                graph_struct(struct_map, this_name, this_id, typedef_map, depth + 1, graph);
            }
        }
    }
}

/****************************************************************************************************/

std::string format_attribute_map(const dot_attribute_map_t& map)
{
    std::string result;

    if (!map.empty())
    {
        bool first(true);

        result += " [";

        for (dot_attribute_map_t::const_iterator iter(map.begin()), last(map.end()); iter != last; ++iter)
        {
            result += (first ? " " : ", ") + iter->first + " = \"" + iter->second + "\"";

            first = false;
        }

        result += " ]";
    }

    return result;
}

/****************************************************************************************************/

std::string format_node(const dot_node_t& node)
{
    std::string result;

    return result += node.id_m + " " + format_attribute_map(node.attribute_map_m) + ";\n";
}

/****************************************************************************************************/

std::string format_edge(const dot_edge_t& edge)
{
    std::string result;

    return result += edge.from_m + " -> " + edge.to_m + format_attribute_map(edge.attribute_map_m) + ";\n";
}

/****************************************************************************************************/

void format_graph(dot_graph_t& graph, std::ostream& output)
{
    output << graph.type_m << ' ' << graph.name_m << '\n'
           << "{\n";

    for(std::size_t i(0), count(graph.setting_vector_m.size()); i != count; ++i)
        output << spaces(1) << graph.setting_vector_m[i] << ";\n";

    output << '\n';

    adobe::stable_sort(graph.node_vector_m, id_less_t());
    graph.node_vector_m.erase(adobe::unique(graph.node_vector_m, id_equal_t()), graph.node_vector_m.end());

    for(std::size_t i(0), count(graph.node_vector_m.size()); i != count; ++i)
        output << spaces(1) << format_node(graph.node_vector_m[i]);

    output << '\n';

    std::size_t cluster_id(0);

    for(dot_cluster_vector_t::iterator iter(graph.cluster_vector_m.begin()), last(graph.cluster_vector_m.end()); iter != last; ++iter)
    {
        std::string cluster_id_str(padded_id(cluster_id++, 5));

        output << spaces(1) << "subgraph cluster_" << cluster_id_str << '\n'
               << spaces(1) << "{\n"
               << spaces(2) << "label = \"" << iter->label_m << "\";\n";

        dot_node_vector_t& cluster_node_vector(iter->node_vector_m);

        adobe::stable_sort(cluster_node_vector, id_less_t());
        cluster_node_vector.erase(adobe::unique(cluster_node_vector, id_equal_t()), cluster_node_vector.end());

        for(dot_node_vector_t::const_iterator iter(cluster_node_vector.begin()), last(cluster_node_vector.end()); iter != last; ++iter)
            output << spaces(2) << format_node(*iter);

        output << spaces(1) << "}\n\n";
    }

    for(dot_edge_set_t::const_iterator iter(graph.edge_set_m.begin()), last(graph.edge_set_m.end()); iter != last; ++iter)
        output << spaces(1) << format_edge(*iter);

    output << "} // " << graph.name_m << '\n';
}

/****************************************************************************************************/

} // namespace

/****************************************************************************************************/

void dot_graph(const binspector_analyzer_t::structure_map_t& struct_map,
               const std::string&                            starting_struct,
               const boost::filesystem::path&                output_root)
{
    boost::filesystem::path     output_filename(output_root / "graph.gv");
    boost::filesystem::ofstream output(output_filename);
    dot_graph_t                 graph;

    assert (output);

    graph.name_m = "G";
    graph.type_m = "digraph";

    //graph.setting_vector_m.push_back("node [shape=box]");
    //graph.setting_vector_m.push_back("rankdir=TB");
    //graph.setting_vector_m.push_back("packMode=clust");
    graph.setting_vector_m.push_back("outputMode=nodesfirst");

    adobe::name_t starting_name(starting_struct.c_str());
    std::string   starting_id(name_map(starting_name));
    dot_node_t    node;

    node.attribute_map_m["label"] = starting_name.c_str();
    node.id_m = starting_id;

    graph.node_vector_m.push_back(node);

    graph_struct(struct_map,
                 starting_name,
                 starting_id,
                 binspector_analyzer_t::typedef_map_t(),
                 0,
                 graph);

    format_graph(graph, output);

    std::cerr << "File written to " << output_filename.string() << '\n';
}

/****************************************************************************************************/
