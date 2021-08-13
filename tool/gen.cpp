#include "gen.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <clang-c/Index.h>

struct gen_xml_context
{
    const gconf::Desc &desc;
    std::fstream &fout;
};

static void write_builtin_function(std::fstream &fout);
static bool is_similar_int(CXTypeKind kind);
static bool is_string(const gconf::FieldDesc &fd);
static bool is_string(CXTypeKind kind, const std::string &type_name);

static void replace_all(std::string &str, const std::string &from, const std::string &to)
{
    std::size_t pos = str.find(from);
    while (pos != std::string::npos)
    {
        str.replace(pos, 2, to);
        pos = str.find(from, pos + 1);
    }
}

static bool write_xml_reader_header(const gconf::Desc &desc, const std::list<std::string> &struct_names)
{
    std::string file_base_name = "xml_reader";
    std::string file_name = file_base_name;
    std::transform(file_name.begin(), file_name.end(), file_name.begin(), ::tolower);
    file_name.append("_gen.h");

    std::fstream fout(file_name, std::ios::out | std::ios::trunc);
    if (!fout)
    {
        std::cerr << "open file " << file_name << " to write failed" << std::endl;
        return false;
    }

    std::string header_macro = file_base_name;
    std::transform(header_macro.begin(), header_macro.end(), header_macro.begin(), ::toupper);
    header_macro.append("_GEN_H_");

    fout << "#ifndef " << header_macro << "\n";
    fout << "#define " << header_macro << "\n\n";
    fout << "#include <string>\n";
    for (auto &inc : desc.includes_)
    {
        fout << "#include \"" << inc << "\"\n";
    }
    fout << "\n";

    for (auto &struct_name : struct_names)
    {
        auto iter = desc.structs_.find(struct_name);
        if (iter == desc.structs_.end())
        {
            throw "why?";
        }
        const gconf::StructDesc &sd = iter->second;
        std::string func_name = sd.type_name_;
        replace_all(func_name, "::", "_");
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::tolower);
        fout << "bool load_" << func_name << "(const std::string& xml_path, " << sd.type_name_ << "& conf);\n\n";
    }

    fout << "#endif\n";
    fout.close();

    return true;
}

static void write_xml_reader_struct(gen_xml_context &ctx, const gconf::StructDesc &sd, const std::string &xmlnode, const std::string &varname, std::string indent);

static void remove_struct_prefix(std::string &str)
{
    if (str.substr(0, 7) == "struct ")
    {
        str = str.substr(7);
    }
    else if (str.substr(0, 6) == "class ")
    {
        str = str.substr(6);
    }
}

static void write_xml_reader_vector(gen_xml_context &ctx, const std::string &xmlnode, const gconf::FieldDesc &fd,
                                    const std::string &varname, const std::string &field_node, std::string indent)
{
    CXTypeKind param_kind = fd.template_param_types_[0].kind;
    std::string param_type_name = clang_getCString(clang_getTypeSpelling(fd.template_param_types_[0]));
    remove_struct_prefix(param_type_name);
    std::string param_var_name = "var_vele_of_" + fd.name_;

    ctx.fout << indent << "{\n";
    indent += "\t";
    ctx.fout << indent << param_type_name << " " << param_var_name << ";\n";
    if (CXType_Bool == param_kind)
    {
        ctx.fout << indent << "read_xml_bool(parser, " << field_node << ", " << param_var_name << ");\n";
    }
    else if (is_similar_int(param_kind))
    {
        ctx.fout << indent << "read_xml_similar_int(parser, " << field_node << ", " << param_var_name << ");\n";
    }
    else if (CXType_Elaborated == param_kind || CXType_Record == param_kind)
    {
        auto iter = ctx.desc.structs_.find(param_type_name);
        if (iter == ctx.desc.structs_.end())
        {
            std::cerr << __LINE__ << " "
                      << "parse CXType_Elaborated type " << param_type_name << " falied" << std::endl;
            exit(-1);
        }

        write_xml_reader_struct(ctx, iter->second, field_node, param_var_name, indent);
    }
    else
    {
        std::cout << param_var_name << "gen reader code failed" << std::endl;
        exit(1);
    }
    ctx.fout << indent << varname << ".push_back(" << param_var_name << ");\n";
    indent.pop_back();
    ctx.fout << indent << "}\n";
}

static void write_xml_reader_map(gen_xml_context &ctx, const std::string &xmlnode, const gconf::FieldDesc &fd,
                                 const std::string &varname, const std::string &field_node, std::string indent)
{
    CXTypeKind key_kind = fd.template_param_types_[0].kind;
    CXTypeKind value_kind = fd.template_param_types_[1].kind;
    std::string key_type_name = clang_getCString(clang_getTypeSpelling(fd.template_param_types_[0]));
    std::string value_type_name = clang_getCString(clang_getTypeSpelling(fd.template_param_types_[1]));
    remove_struct_prefix(key_type_name);
    remove_struct_prefix(value_type_name);

    ctx.fout << indent << "{\n";
    indent += "\t";

    if (!is_similar_int(key_kind) && !is_string(key_kind, key_type_name))
    {
        std::cerr << "map key just support simple type, such as int, int64, string" << std::endl;
        exit(-1);
    }

    if (is_similar_int(value_kind) || is_string(value_kind, value_type_name))
    {
        ctx.fout << indent << key_type_name << " map_key;\n";
        ctx.fout << indent << value_type_name << " map_value;\n";
        ctx.fout << indent << "if (read_xml_simple_kv(parser, " << field_node << ", map_key, map_value))\n";
        ctx.fout << indent << "{\n";
        ctx.fout << indent << "\t" << varname << "[map_key] = map_value;\n";
        ctx.fout << indent << "}\n";
    }
    else if (CXType_Elaborated == value_kind || CXType_Record == value_kind)
    {
        auto iter = ctx.desc.structs_.find(value_type_name);
        if (iter == ctx.desc.structs_.end())
        {
            std::cerr << __LINE__ << " parse CXType_Elaborated type " << value_type_name << " falied" << std::endl;
            exit(-1);
        }

        std::string value_var_name = "var_map_value_of_" + fd.name_;
        ctx.fout << indent << value_type_name << " " << value_var_name << ";\n";
        write_xml_reader_struct(ctx, iter->second, field_node, value_var_name, indent);
        // 这里要先找到key，如果没有，找第一个匹配的类型
        const gconf::FieldDesc *key_fd = nullptr;
        for (auto &ifd : iter->second.field_list_)
        {
            if (std::find(ifd.annotate_attrs_.begin(), ifd.annotate_attrs_.end(), "map_key") != ifd.annotate_attrs_.end())
            {
                key_fd = &ifd;
                break;
            }
        }

        if (!key_fd)
        {
            for (auto &ifd : iter->second.field_list_)
            {
                if (ifd.type_kind_ == key_kind)
                {
                    key_fd = &ifd;
                    break;
                }
            }
        }
        if (!key_fd)
        {
            std::cerr << __LINE__ << " parse CXType_Elaborated type " << value_type_name << " falied, unspecify key" << std::endl;
            exit(-1);
        }

        ctx.fout << indent << varname << "[" << value_var_name << "." << key_fd->name_ << "] = " << value_var_name << ";\n";
    }
    else
    {
        std::cerr << __LINE__ << "parse CXType_Elaborated type " << value_type_name << " falied" << std::endl;
        exit(-1);
    }

    indent.pop_back();
    ctx.fout << indent << "}\n";
}

static void write_xml_reader_repeat(gen_xml_context &ctx, const std::string &xmlnode, const gconf::FieldDesc &fd,
                                    const std::string &varname, std::string indent)
{
    std::string field_node = fd.name_ + "_node";
    ctx.fout << indent << "xmlNodePtr " << field_node << " = parser.getChildNode(" << xmlnode << ", \"" << fd.name_ << "\");\n";
    ctx.fout << indent << "while (" << field_node << ")\n";
    ctx.fout << indent << "{\n";
    indent += "\t";
    if (fd.type_template_name_ == "vector")
    {
        write_xml_reader_vector(ctx, xmlnode, fd, varname, field_node, indent);
    }
    else if (fd.type_template_name_ == "map" || fd.type_template_name_ == "unordered_map")
    {
        write_xml_reader_map(ctx, xmlnode, fd, varname, field_node, indent);
    }
    indent.pop_back();
    ctx.fout << indent << "    " << field_node << " = parser.getNextNode(" << field_node << ", \"" << fd.name_ << "\");\n";
    ctx.fout << indent << "}\n";
}

static void write_xml_reader_struct(gen_xml_context &ctx, const gconf::StructDesc &sd, const std::string &xmlnode, const std::string &varname, std::string indent)
{
    for (auto &fd : sd.field_list_)
    {
        std::string field_name = varname + "." + fd.name_;
        if (fd.type_kind_ == CXType_Bool)
        {
            ctx.fout << indent << "read_xml_child_bool(parser, " << xmlnode << ", \"" << fd.name_ << "\", " << field_name << ");\n";
        }
        else if (is_similar_int(fd.type_kind_))
        {
            ctx.fout << indent << "read_xml_child_similar_int(parser, " << xmlnode << ", \"" << fd.name_ << "\", " << field_name << ");\n";
        }
        else if (fd.type_kind_ == CXType_Elaborated)
        {
            if (fd.type_name_ == "std::string")
            {
                ctx.fout << indent << "read_xml_child_string(parser, " << xmlnode << ", \"" << fd.name_ << "\", " << field_name << ");\n";
            }
            else if (fd.type_template_name_ == "vector" && fd.namespaces_.size() == 1 && fd.namespaces_.front() == "std")
            {
                if (fd.template_param_types_.size() != 1)
                {
                    std::cerr << "vector template param parse error " << fd.type_name_ << std::endl;
                    exit(-1);
                }
                write_xml_reader_repeat(ctx, xmlnode, fd, field_name, indent);
            }
            else if (fd.type_template_name_ == "map" && fd.namespaces_.size() == 1 && fd.namespaces_.front() == "std" && fd.template_param_types_.size() == 2)
            {
                write_xml_reader_repeat(ctx, xmlnode, fd, field_name, indent);
            }
            else if (fd.type_template_name_ == "unordered_map" && fd.namespaces_.size() == 1 && fd.namespaces_.front() == "std" && fd.template_param_types_.size() == 2)
            {
                write_xml_reader_repeat(ctx, xmlnode, fd, field_name, indent);
            }
            else
            {
                auto iter = ctx.desc.structs_.find(fd.type_name_);
                if (iter == ctx.desc.structs_.end())
                {
                    std::cerr << __LINE__ << "parse CXType_Elaborated type " << fd.type_name_ << " falied" << std::endl;
                    exit(-1);
                }

                std::string node_name = fd.name_ + "_node";
                ctx.fout << indent << "xmlNodePtr " << node_name << " = parser.getChildNode(" << xmlnode << ", \"" << fd.name_ << "\");\n";
                ctx.fout << indent << "if (" << node_name << ")\n";
                ctx.fout << indent << "{\n";
                indent += '\t';
                write_xml_reader_struct(ctx, iter->second, node_name, field_name, indent);
                indent.pop_back();
                ctx.fout << indent << "}\n";
            }
        }
        else
        {
            auto kindStr = clang_getCString(clang_getTypeKindSpelling(fd.type_kind_));
            std::cerr << "unsupported field kind " << kindStr << std::endl;
            exit(-1);
        }
    }
}

static bool write_xml_reader_cpp(gen_xml_context &ctx, const gconf::StructDesc &sd)
{
    std::string func_name = sd.type_name_;
    replace_all(func_name, "::", "_");
    std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::tolower);
    ctx.fout << "bool load_" << func_name << "(const std::string& xml_path, " << sd.type_name_ << "& conf)\n";
    ctx.fout << "{\n";

    std::string indent = "\t";
    ctx.fout << indent << "zXMLParser parser;\n";
    ctx.fout << indent << "if (!parser.initFile(xml_path))\n";
    ctx.fout << indent << "{\n";
    ctx.fout << indent << "\treturn false;\n";
    ctx.fout << indent << "}\n";

    ctx.fout << indent << "xmlNodePtr root = parser.getRootNode(nullptr);\n";
    ctx.fout << indent << "if (!root)\n";
    ctx.fout << indent << "{\n";
    ctx.fout << indent << "\treturn false;\n";
    ctx.fout << indent << "}\n";

    write_xml_reader_struct(ctx, sd, "root", "conf", indent);

    ctx.fout << indent << "return true;\n";
    ctx.fout << "}\n";

    return true;
}

static bool write_xml_reader_cpp(const gconf::Desc &desc, const std::list<std::string> &struct_names)
{
    std::string file_base_name = "xml_reader";
    std::string header_file_name = file_base_name;
    std::transform(header_file_name.begin(), header_file_name.end(), header_file_name.begin(), ::tolower);
    header_file_name.append("_gen.h");

    std::string cpp_file_name = file_base_name;
    std::transform(cpp_file_name.begin(), cpp_file_name.end(), cpp_file_name.begin(), ::tolower);
    cpp_file_name.append("_gen.cpp");

    std::fstream fout(cpp_file_name, std::ios::out | std::ios::trunc);
    if (!fout)
    {
        std::cerr << "open file " << cpp_file_name << " to write failed" << std::endl;
        return false;
    }

    fout << "#include \"" << header_file_name << "\"\n";
    fout << "#include \"moon/zXMLParser.h\"\n";

    fout << "\n";

    write_builtin_function(fout);
    fout << "\n";

    for (auto &struct_name : struct_names)
    {
        auto iter = desc.structs_.find(struct_name);
        if (iter == desc.structs_.end())
        {
            throw "why?";
        }
        gen_xml_context ctx{desc, fout};
        if (!write_xml_reader_cpp(ctx, iter->second))
        {
            std::cerr << "generate struct " << struct_name << " cpp code failed" << std::endl;
            return false;
        }
        fout << "\n";
    }

    fout.close();
    return true;
}

bool GenerateXmlReaderCode(const gconf::Desc &desc, const std::list<std::string> &struct_names)
{
    for (auto &struct_name : struct_names)
    {
        auto iter = desc.structs_.find(struct_name);
        if (iter == desc.structs_.end())
        {
            std::cerr << "struct name " << struct_name << " is not defined" << std::endl;
            return false;
        }
    }

    if (!write_xml_reader_header(desc, struct_names))
    {
        return false;
    }
    if (!write_xml_reader_cpp(desc, struct_names))
    {
        return false;
    }

    return true;
}

static void write_builtin_function(std::fstream &fout)
{

    fout << "static bool read_xml_bool(zXMLParser & parser, xmlNodePtr node, bool &b)\n";
    fout << "{\n";
    fout << "    if (!node)\n";
    fout << "    {\n";
    fout << "        return false;\n";
    fout << "    }\n";
    fout << "    std::string value_str;\n";
    fout << "    parser.getNodeContentStr(node, value_str);\n";
    fout << "    if (value_str == \"true\")\n";
    fout << "    {\n";
    fout << "        b = true;\n";
    fout << "    }\n";
    fout << "    else if (value_str == \"false\")\n";
    fout << "    {\n";
    fout << "        b = false;\n";
    fout << "    }\n";
    fout << "    else\n";
    fout << "    {\n";
    fout << "        for (auto c : value_str)\n";
    fout << "        {\n";
    fout << "            if (!isdigit(c))\n";
    fout << "            {\n";
    fout << "                b = false;\n";
    fout << "                return false;\n";
    fout << "            }\n";
    fout << "        }\n";
    fout << "        b = atoi(value_str.c_str()) > 0;\n";
    fout << "    }\n";
    fout << "    return true;\n";
    fout << "}\n";

    fout << "\n";
    fout << "static bool read_xml_child_bool(zXMLParser & parser, xmlNodePtr pnode, const std::string &node_name, bool &b)\n";
    fout << "{\n";
    fout << "    if (!pnode)\n";
    fout << "    {\n";
    fout << "        return false;\n";
    fout << "    }\n";
    fout << "    std::string value_str;\n";
    fout << "    if (!parser.getNodePropStr(pnode, node_name.c_str(), value_str))\n";
    fout << "    {\n";
    fout << "        xmlNodePtr node = parser.getChildNode(pnode, node_name.c_str());\n";
    fout << "        if (node)\n";
    fout << "        {\n";
    fout << "            parser.getNodeContentStr(node, value_str);\n";
    fout << "        }\n";
    fout << "        else\n";
    fout << "        {\n";
    fout << "            b = false;\n";
    fout << "            return false;\n";
    fout << "        }\n";
    fout << "    }\n";
    fout << "    if (value_str == \"true\")\n";
    fout << "    {\n";
    fout << "        b = true;\n";
    fout << "    }\n";
    fout << "    else if (value_str == \"false\")\n";
    fout << "    {\n";
    fout << "        b = false;\n";
    fout << "    }\n";
    fout << "    else\n";
    fout << "    {\n";
    fout << "        for (auto c : value_str)\n";
    fout << "        {\n";
    fout << "            if (!isdigit(c))\n";
    fout << "            {\n";
    fout << "                b = false;\n";
    fout << "                return false;\n";
    fout << "            }\n";
    fout << "        }\n";
    fout << "        b = atoi(value_str.c_str()) > 0;\n";
    fout << "    }\n";
    fout << "    return true;\n";
    fout << "}\n";

    fout << "\n";

    fout << "template<class T> \n";
    fout << "static bool read_xml_similar_int(zXMLParser & parser, xmlNodePtr node, T & v)\n";
    fout << "{\n";
    fout << "    if (!node)\n";
    fout << "    {\n";
    fout << "        return false;\n";
    fout << "    }\n";
    fout << "    T value = 0;\n";
    fout << "    if (parser.getNodeContentNum(node, &value, sizeof(value)))\n";
    fout << "    {\n";
    fout << "        v = value;\n";
    fout << "    }\n";
    fout << "    else\n";
    fout << "    {\n";
    fout << "        v = T();\n";
    fout << "    }\n";
    fout << "    return true;\n";
    fout << "}\n";

    fout << "\n";
    fout << "template<class T> \n";
    fout << "static bool read_xml_child_similar_int(zXMLParser &parser, xmlNodePtr pnode, const std::string &node_name, T &v)\n";
    fout << "{\n";
    fout << "    if (!pnode)\n";
    fout << "    {\n";
    fout << "        return false;\n";
    fout << "    }\n";
    fout << "    T value = 0;\n";
    fout << "    if (parser.getNodePropNum(pnode, node_name.c_str(), &value, sizeof(value)))\n";
    fout << "    {\n";
    fout << "    	v = value;\n";
    fout << "    	return true;\n";
    fout << "    }    \n";
    fout << "    xmlNodePtr node = parser.getChildNode(pnode, node_name.c_str());\n";
    fout << "    if (node)\n";
    fout << "    {\n";
    fout << "        if (parser.getNodeContentNum(node, &value, sizeof(value)))\n";
    fout << "        {\n";
    fout << "            v = value;\n";
    fout << "        }\n";
    fout << "        else\n";
    fout << "        {\n";
    fout << "            v = T();\n";
    fout << "        }\n";
    fout << "    }\n";
    fout << "    else\n";
    fout << "    {\n";
    fout << "        v = T();\n";
    fout << "        return false;\n";
    fout << "    }\n";
    fout << "    return true;\n";
    fout << "}\n";

    fout << "\n";
    fout << "static bool read_xml_string(zXMLParser & parser, xmlNodePtr node, std::string & v)\n";
    fout << "{\n";
    fout << "    if (!node)\n";
    fout << "    {\n";
    fout << "        return false;\n";
    fout << "    }\n";
    fout << "    return parser.getNodeContentStr(node, v);\n";
    fout << "}\n";

    fout << "\n";
    fout << "static bool read_xml_child_string(zXMLParser &parser, xmlNodePtr pnode, const std::string &node_name, std::string &v)\n";
    fout << "{\n";
    fout << "    if (!pnode)\n";
    fout << "    {\n";
    fout << "        return false;\n";
    fout << "    }\n";
    fout << "    if (!parser.getNodePropStr(pnode, node_name.c_str(), v))\n";
    fout << "    {\n";
    fout << "        xmlNodePtr node = parser.getChildNode(pnode, node_name.c_str());\n";
    fout << "        if (node)\n";
    fout << "        {\n";
    fout << "            parser.getNodeContentStr(node, v);\n";
    fout << "        }\n";
    fout << "        else\n";
    fout << "        {\n";
    fout << "            return false;\n";
    fout << "        }\n";
    fout << "    }\n";
    fout << "    return true;\n";
    fout << "}\n";

    fout << "\n";

    fout << "template <class K, class V>\n";
    fout << "static bool read_xml_simple_kv(zXMLParser & parser, xmlNodePtr node, K & k, V & v)\n";
    fout << "{\n";
    fout << "    if (!node)\n";
    fout << "    {\n";
    fout << "        return false;\n";
    fout << "    }\n";
    fout << "    if (!parser.getNodePropNum(node, \"key\", &k, sizeof(k)))\n";
    fout << "    {\n";
    fout << "        xmlNodePtr key_node = parser.getChildNode(node, \"key\");\n";
    fout << "        if (!key_node)\n";
    fout << "        {\n";
    fout << "            return false;\n";
    fout << "        }\n";
    fout << "        if (!parser.getNodeContentNum(key_node, &k, sizeof(k)))\n";
    fout << "        {\n";
    fout << "            return false;\n";
    fout << "        }\n";
    fout << "    }\n";
    fout << "    if (!parser.getNodePropNum(node, \"value\", &v, sizeof(v)))\n";
    fout << "    {\n";
    fout << "        xmlNodePtr value_node = parser.getChildNode(node, \"value\");\n";
    fout << "        if (!value_node)\n";
    fout << "        {\n";
    fout << "            return false;\n";
    fout << "        }\n";
    fout << "        if (!parser.getNodeContentNum(value_node, &v, sizeof(v)))\n";
    fout << "        {\n";
    fout << "            return false;\n";
    fout << "        }\n";
    fout << "    }\n";
    fout << "    return true;\n";
    fout << "}\n";
}

static bool is_similar_int(CXTypeKind kind)
{
    if (kind >= CXType_Char_U && kind <= CXType_LongDouble)
    {
        return true;
    }
    if (CXType_Float128 == kind || CXType_Float16 == kind || CXType_Enum == kind)
    {
        return true;
    }
    return false;
}

static bool is_string(const gconf::FieldDesc &fd)
{
    return is_string(fd.type_kind_, fd.type_name_);
}

static bool is_string(CXTypeKind kind, const std::string &type_name)
{
    return kind == CXType_Elaborated && type_name == "std::string";
}