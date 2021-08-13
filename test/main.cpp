#include <iostream>
#include "xml_reader_gen.h"

void print_conf(const gconf::GlobalConfig &conf)
{
    std::cout << "log_fb_msg: " << conf.log_fb_msg << std::endl;
    std::cout << "log_level: " << conf.log_level << std::endl;
    for (auto &v : conf.vvvvv1)
    {
        std::cout << "vvvvv1: " << v << std::endl;
    }
    for (auto &v : conf.vvvvv2)
    {
        std::cout << "vvvvv2: " << v.a1 << "-" << v.a2 << "-" << v.a3 << "-" << v.a4 << "-" << v.a5 << "-" << std::endl;
        std::cout << "\ta6: ";
        for (auto &v2 : v.a6)
        {
            std::cout << v2.b1 << "-" << v2.b2;
        }
        std::cout << std::endl;
    }
    std::cout << "gs:" << std::endl;
    std::cout << "\tgs_int: " << conf.gs.gs_int << std::endl;
    std::cout << "\tgs_float: " << conf.gs.gs_float << std::endl;
    std::cout << "\tgs_double: " << conf.gs.gs_double << std::endl;
    std::cout << "\tlog_path: " << conf.gs.log_path << std::endl;

    std::cout << "\thero:" << std::endl;
    std::cout << "\t\thero_level: " << conf.gs.hero.hero_level << std::endl;
    std::cout << "\t\thero_name: " << conf.gs.hero.hero_name << std::endl;

    std::cout << "value_int: " << conf.value_int << std::endl;
    for (auto &it : conf.ii_map)
    {
        std::cout << "ii_map: " << it.first << "-" << it.second << std::endl;
    }
    for (auto &it : conf.mapvalue_map)
    {
        std::cout << "mapvalue_map: " << it.first << "-" << it.second.id << "-" << it.second.id
                  << "-" << it.second.name
                  << "-" << it.second.level
                  << "-" << it.second.is_something << std::endl;
    }
}

int main()
{
    gconf::GlobalConfig conf;
    bool ret = load_gconf_globalconfig("../test/config.xml", conf);
    if (!ret)
    {
        std::cout << "load config failed" << std::endl;
    }
    print_conf(conf);
    return 0;
}