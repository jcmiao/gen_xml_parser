#ifndef GCONF_DESC_H_
#define GCONF_DESC_H_

#include <string>
#include <vector>
#include <map>
#include <clang-c/Index.h>

namespace gconf
{
    struct FieldDesc
    {
        std::string name_;
        std::string type_name_;
        CXTypeKind type_kind_;
        std::vector<std::string> namespaces_;
        std::string type_template_name_;
        std::vector<CXType> template_param_types_;
        std::vector<std::string> annotate_attrs_;
    };

    struct StructDesc
    {
        std::string type_name_;
        std::vector<FieldDesc> field_list_;
    };

    struct Desc
    {
        std::string source_file_;
        std::vector<std::string> includes_;
        std::map<std::string, StructDesc> structs_;

        //temp
        std::vector<std::string> cur_structs_; // 废弃
        CXSourceLocation cur_field_sl_;
        FieldDesc *cur_field_ = nullptr;
    };

    void PrintDesc(const Desc &desc);
}

#endif /* GCONF_DESC_H_ */