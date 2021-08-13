#include "desc.h"
#include <iostream>

namespace gconf
{
    void PrintDesc(const Desc &desc)
    {
        std::cout << desc.source_file_ << std::endl;
        for (auto &s : desc.structs_)
        {
            std::cout << s.second.type_name_ << std::endl;
            std::cout << "{" << std::endl;
            for (auto &f : s.second.field_list_)
            {
                std::cout << "\t// type_kind " << f.type_kind_ << std::endl;
                if (!f.namespaces_.empty())
                {
                    std::cout << "\t// namespaces: ";
                    for (auto &ns : f.namespaces_)
                    {
                        std::cout << ns << "::";
                    }
                    std::cout << std::endl;
                }
                if (!f.type_template_name_.empty())
                {
                    std::cout << "\t// tempalte name: " << f.type_template_name_ << "<";
                    for (size_t i = 0; i < f.template_param_types_.size(); ++i)
                    {
                        if (i)
                            std::cout << ",";

                        std::cout << clang_getCString(clang_getTypeSpelling(f.template_param_types_[i]));
                    }
                    std::cout << ">" << std::endl;
                }
                if (!f.annotate_attrs_.empty())
                {
                    std::cout << "\t// annotate attrs: ";
                    for (size_t i = 0; i < f.annotate_attrs_.size(); ++i)
                    {
                        if (i)
                            std::cout << ",";

                        std::cout << f.annotate_attrs_[i];
                    }
                    std::cout << std::endl;
                }
                std::cout << "\t" << f.type_name_ << "\t" << f.name_ << ";" << std::endl;
            }
            std::cout << "}" << std::endl;
        }
    }
}