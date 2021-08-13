

#include <string>
#include <map>
#include <vector>
#include <unordered_map>

#define map_key() __attribute__((annotate("map_key")))

namespace gconf
{
    struct GlobalConfig
    {
        bool log_fb_msg = false;
        int log_level = 0;
        std::vector<int> vvvvv1;

        struct VecElement
        {
            int a1;
            int a2;
            int a3;
            int a4;
            std::string a5;

            struct VecElement2
            {
                int b1;
                std::string b2;
            };
            std::vector<VecElement2> a6;
        };
        std::vector<VecElement> vvvvv2;

        struct GameServerConfig
        {
            int gs_int;
            int32_t gs_float;
            uint64_t gs_double;
            std::string log_path;

            struct HeroConfig
            {
                int hero_level;
                std::string hero_name;
            } hero;
        } gs;

        int value_int;

        std::map<int, int> ii_map;

        struct MapValue
        {
            map_key() int id;
            std::string name;
            int level;
            bool is_something;
        };
        std::unordered_map<int, MapValue> mapvalue_map;
    };
}