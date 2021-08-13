#include <iostream>
#include "desc.h"
#include "parse.h"
#include "gen.h"

using namespace std;

gconf::Desc desc;

int main(/*int argc, char** argv*/)
{
    //for test start
    const char *argv[] =
        {
            "",
            "gconf::GlobalConfig",
            "../test/sample.h"};
    int argc = 3;
    //for test end

    desc.includes_.push_back("sample.h");

    if (argc < 3)
    {
        std::cerr << "need specify target struct and source file" << std::endl;
        exit(-1);
    }

    for (int i = 2; i < argc; ++i)
    {
        if (!VisitSourceFile(argv[i], desc))
        {
            std::cerr << "parse source " << argv[i] << " failed" << std::endl;
            exit(-1);
        }
    }

    gconf::PrintDesc(desc);

    std::list<std::string> struct_names = {argv[1]};
    if (!GenerateXmlReaderCode(desc, struct_names))
    {
        std::cerr << "generate xml reader code failed" << std::endl;
        exit(-1);
    }

    return 0;
}