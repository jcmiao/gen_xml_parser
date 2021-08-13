#include "parse.h"
#include <clang-c/Index.h>
#include <iostream>
#include <algorithm>

// /opt/env/lib/libclang.so.10

std::ostream &operator<<(std::ostream &stream, const CXString &str)
{
    stream << clang_getCString(str);
    clang_disposeString(str);
    return stream;
}

static void debug_print_cursor(CXCursor c, CXCursor parent);

static CXChildVisitResult ParseStructDecl(CXCursor c, CXCursor parent, gconf::Desc &desc);
static CXChildVisitResult ParseFieldDecl(CXCursor c, CXCursor parent, gconf::Desc &desc);
static CXChildVisitResult ParseNamespaceRefDecl(CXCursor c, CXCursor parent, gconf::Desc &desc);
static CXChildVisitResult ParseTempalteRefDecl(CXCursor c, CXCursor parent, gconf::Desc &desc);
static CXChildVisitResult ParseAnnotateAttr(CXCursor c, CXCursor parent, gconf::Desc &desc);
static CXChildVisitResult ParseCursor(CXCursor c, CXCursor parent, CXClientData client_data);

bool VisitSourceFile(const char *source_file, gconf::Desc &desc)
{
    CXTranslationUnit unit = nullptr;

    const char *args[] = {
        "-ast-dump",
        "-x",
        "c++",
        "-I../tool_include"};
    CXIndex index = clang_createIndex(0, 0);
    int ret = clang_parseTranslationUnit2(
        index,
        source_file, args, 4,
        nullptr, 0,
        CXTranslationUnit_None, &unit);

    if (unit == nullptr)
    {
        std::cerr << "Unable to parse translation unit. Quitting. " << ret << std::endl;
        return false;
    }

    desc.source_file_ = source_file;

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(cursor, ParseCursor, &desc);

    // todo: 这里暂时不能回收
    // clang_disposeTranslationUnit(unit);
    // clang_disposeIndex(index);
    return true;
}

static CXChildVisitResult ParseCursor(CXCursor c, CXCursor parent, CXClientData client_data)
{
    auto pdesc = (gconf::Desc *)(client_data);
    auto cursorType = clang_getCursorType(c);
    auto typeSpell = clang_getCString(clang_getTypeKindSpelling(cursorType.kind));

    auto cursorKind = clang_getCursorKind(c);
    if (CXCursor_StructDecl == cursorKind || CXCursor_ClassDecl == cursorKind)
    {
        return ParseStructDecl(c, parent, *pdesc);
    }
    else if (CXCursor_FieldDecl == cursorKind)
    {
        return ParseFieldDecl(c, parent, *pdesc);
    }
    else if (CXCursor_NamespaceRef == cursorKind && CXCursor_FieldDecl == clang_getCursorKind(parent))
    {
        return ParseNamespaceRefDecl(c, parent, *pdesc);
    }
    else if (CXCursor_TemplateRef == cursorKind && CXCursor_FieldDecl == clang_getCursorKind(parent))
    {
        return ParseTempalteRefDecl(c, parent, *pdesc);
    }
    else if (CXCursor_AnnotateAttr == cursorKind)
    {
        ParseAnnotateAttr(c, parent, *pdesc);
    }
    else
    {
        // debug_print_cursor(c, parent);
    }
    return CXChildVisit_Recurse;
}

static CXChildVisitResult ParseStructDecl(CXCursor c, CXCursor parent, gconf::Desc &desc)
{
    auto pCursorType = clang_getCursorType(parent);
    auto pTypeSpell = clang_getCString(clang_getTypeSpelling(pCursorType));

    auto cursorType = clang_getCursorType(c);
    auto typeSpell = clang_getCString(clang_getTypeSpelling(cursorType));

    auto pCursorKind = clang_getCursorKind(parent);
    if (CXCursor_FieldDecl == pCursorKind)
    {
        return CXChildVisit_Continue;
    }

    // struct decl 和 field declare会重复
    if (desc.structs_.count(typeSpell))
    {
        return CXChildVisit_Continue;
    }

    if (desc.cur_structs_.empty())
    {
        desc.cur_structs_.push_back(typeSpell);
        // std::cout << "1 push current: " << typeSpell << std::endl;
    }
    else
    {
        if (desc.cur_structs_.back() != pTypeSpell)
        {
            // std::cout << "pop back: " << desc.cur_structs_.back() << " because of current" << pTypeSpell << std::endl;
            // debug_print_cursor(c, parent);
            desc.cur_structs_.pop_back();
        }
        desc.cur_structs_.push_back(typeSpell);
        // std::cout << "2 push current: " << typeSpell << std::endl;
    }

    gconf::StructDesc &sd = desc.structs_[typeSpell];
    sd.type_name_ = typeSpell;
    return CXChildVisit_Recurse;
}

static CXChildVisitResult ParseFieldDecl(CXCursor c, CXCursor parent, gconf::Desc &desc)
{
    auto pkind = clang_getCursorKind(parent);
    if (CXCursor_StructDecl == pkind || CXCursor_ClassDecl == pkind)
    {
        auto cursorType = clang_getCursorType(c);
        auto typeSpell = clang_getCString(clang_getTypeSpelling(cursorType));

        auto pCursorType = clang_getCursorType(parent);
        auto pTypeSpell = clang_getCString(clang_getTypeSpelling(pCursorType));
        auto fieldName = clang_getCString(clang_getCursorSpelling(c));

        gconf::StructDesc &sd = desc.structs_[pTypeSpell];
        if (sd.field_list_.end() == std::find_if(sd.field_list_.begin(), sd.field_list_.end(), [fieldName](const gconf::FieldDesc &fd)
                                                 { return fieldName == fd.name_; }))
        {
            sd.field_list_.emplace_back();
            gconf::FieldDesc &fd = sd.field_list_.back();
            fd.name_ = fieldName;
            fd.type_kind_ = cursorType.kind;
            fd.type_name_ = typeSpell;
            if (fd.type_name_.substr(0, 7) == "struct " || fd.type_name_.substr(0, 6) == "class ")
            {
                auto pos = fd.type_name_.rfind(' ', fd.type_name_.size());
                fd.type_name_ = sd.type_name_ + "::" + fd.type_name_.substr(pos + 1);
            }

            int nargu = clang_Type_getNumTemplateArguments(cursorType);
            for (int i = 0; i < nargu; ++i)
            {
                auto argType = clang_Type_getTemplateArgumentAsType(cursorType, i);
                fd.template_param_types_.push_back(argType);
            }

            auto sl = clang_getCursorLocation(c);
            if (!clang_equalLocations(desc.cur_field_sl_, sl))
            {
                desc.cur_field_sl_ = sl;
                desc.cur_field_ = &fd;
                debug_print_cursor(c, parent);
            }
        }
    }
    return CXChildVisit_Recurse;
}

static CXChildVisitResult ParseNamespaceRefDecl(CXCursor c, CXCursor parent, gconf::Desc &desc)
{
    if (CXCursor_FieldDecl == clang_getCursorKind(parent))
    {
        do
        {
            auto sl = clang_getCursorLocation(parent);
            if (!clang_equalLocations(desc.cur_field_sl_, sl))
            {
                break;
            }
            // if (desc.cur_structs_.empty())
            //     break;

            // auto fieldName = clang_getCString(clang_getCursorSpelling(parent));
            // gconf::StructDesc &sd = desc.structs_[desc.cur_structs_.back()];

            // auto iter = std::find_if(sd.field_list_.begin(), sd.field_list_.end(), [fieldName](const gconf::FieldDesc &fd)
            //                          { return fieldName == fd.name_; });

            // if (iter == sd.field_list_.end())
            //     break;
            desc.cur_field_->namespaces_.push_back(clang_getCString(clang_getCursorSpelling(c)));

            // iter->namespaces_.push_back(clang_getCString(clang_getCursorSpelling(c)));
        } while (false);
    }
    return CXChildVisit_Recurse;
}

static CXChildVisitResult ParseTempalteRefDecl(CXCursor c, CXCursor parent, gconf::Desc &desc)
{
    if (CXCursor_FieldDecl == clang_getCursorKind(parent))
    {
        do
        {
            auto sl = clang_getCursorLocation(parent);
            if (!clang_equalLocations(desc.cur_field_sl_, sl))
            {
                break;
            }
            // if (desc.cur_structs_.empty())
            //     break;

            // auto fieldName = clang_getCString(clang_getCursorSpelling(parent));
            // gconf::StructDesc &sd = desc.structs_[desc.cur_structs_.back()];

            // auto iter = std::find_if(sd.field_list_.begin(), sd.field_list_.end(), [fieldName](const gconf::FieldDesc &fd)
            //                          { return fieldName == fd.name_; });

            // if (iter == sd.field_list_.end())
            //     break;

            desc.cur_field_->type_template_name_ = clang_getCString(clang_getCursorSpelling(c));
            // iter->type_template_name_ = clang_getCString(clang_getCursorSpelling(c));
        } while (false);
    }
    return CXChildVisit_Recurse;
}

static CXChildVisitResult ParseAnnotateAttr(CXCursor c, CXCursor parent, gconf::Desc &desc)
{
    if (CXCursor_FieldDecl == clang_getCursorKind(parent))
    {
        do
        {
            auto sl = clang_getCursorLocation(parent);
            if (!clang_equalLocations(desc.cur_field_sl_, sl))
            {
                break;
            }

            desc.cur_field_->annotate_attrs_.push_back(clang_getCString(clang_getCursorSpelling(c)));
        } while (false);
    }
    return CXChildVisit_Recurse;
}

static void debug_print_cursor(CXCursor c, CXCursor parent)
{
    std::cout << clang_getCursorSpelling(parent) << "----";
    std::cout << clang_getTypeSpelling(clang_getCursorType(parent)) << "----";
    std::cout << clang_getCursorKind(parent) << "----";
    std::cout << clang_getCursorKindSpelling(clang_getCursorKind(parent)) << std::endl;
    std::cout << clang_getCursorSpelling(c) << "----";
    std::cout << clang_getTypeSpelling(clang_getCursorType(c)) << "----";
    std::cout << clang_getCursorKind(c) << "----";
    std::cout << clang_getCursorKindSpelling(clang_getCursorKind(c)) << std::endl;
    std::cout << "------------------------------------" << std::endl;
}
