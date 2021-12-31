// dllmain.cpp : Определяет точку входа для приложения DLL.
#include <Windows.h>
#include <codecvt>
#include <locale>
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <EventAPI.h>
#include <LLAPI.h>
#include <MC/Actor.hpp>
#include <MC/Item.hpp>
#include <MC/ItemStack.hpp>
#include <MC/CommandOrigin.hpp>
#include <MC/CommandOutput.hpp>
#include <MC/CommandPosition.hpp>
#include <MC/CommandRegistry.hpp>
#include <MC/Level.hpp>
#include <MC/Player.hpp>
#include <MC/ServerPlayer.hpp>
#include <MC/Tag.hpp>
#include <MC/Types.hpp>
#include <RegCommandAPI.h>
#include <ServerAPI.h>
#include <MC/CompoundTag.hpp>
#include <MC/ListTag.hpp>
#include <iostream>
#pragma warning(push)
#pragma warning(disable : 4996)
#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
#include<third-party/yaml-cpp/yaml.h>
#pragma warning(pop)
#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>

#pragma comment(lib, "bedrock_server_api.lib")
#pragma comment(lib, "bedrock_server_var.lib")
#pragma comment(lib, "LiteLoader.lib")
#pragma comment(lib, "yaml-cpp.lib")
#pragma comment(lib, "SymDBHelper.lib")

using namespace std;

std::wstring to_wstring(const std::string& str,
    const std::locale& loc = std::locale())
{
    std::vector<wchar_t> buf(str.size());
    std::use_facet<std::ctype<wchar_t>>(loc).widen(str.data(),
        str.data() + str.size(),
        buf.data());
    return std::wstring(buf.data(), buf.size());
}

std::string utf8_encode(const std::wstring& wstr)
{
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

class _Group {
public:
    string         name; //имя группы
    string         prefix;
    vector<string> perms;       //права
    bool           is_default;  //установлен ли по умолчанию
    string         inheritance; //группу,которую будем наследовать
};

class _Groups {
public:
    vector<_Group> groups; //группы
};

class _User {
public:
    string nickname; //ник
    string prefix;
    string suffix;
    string         group;       //группа игрока
    vector<string> permissions; //права игрока
};

class Users {
public:
    vector<_User> users; //игроки
};

struct KitUser
{
    string nickname;
    vector<string> cooldown_table;
};

struct KitUsers
{
    vector<KitUser> users;
};

namespace YAML
{
    template <>
    struct convert<KitUser>
    {
        static Node encode(const KitUser& rhs)
        {
            Node node;
            node[rhs.nickname] = YAML::Node(YAML::NodeType::Map);
            if (rhs.cooldown_table.size() == 0)
            {
                node[rhs.nickname]["table"] = vector<string>(0);
            }
            node[rhs.nickname]["table"] = rhs.cooldown_table;
            return node;
        }
        static bool decode(const Node& node, KitUser& rhs)
        {
            using namespace std;
            string name;
            for (const auto& kv : node)
            {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name = kv.first.as<std::string>();
                break;
            }
            rhs.nickname = name;
            rhs.cooldown_table = node[name]["table"].as<std::vector<string>>();
            return true;
        }
    };
} // namespace YAML

namespace YAML
{
    template <>
    struct convert<_Group>
    {
        static Node encode(const _Group& rhs)
        {
            Node node;
            node[rhs.name] = YAML::Node(YAML::NodeType::Map);
            node[rhs.name]["prefix"] = rhs.prefix;
            node[rhs.name]["inheritance"] = rhs.inheritance;
            node[rhs.name]["default"] = rhs.is_default;
            if (rhs.perms.size() == 0)
            {
                node[rhs.name]["permissions"] = vector<string>(0);
            }
            node[rhs.name]["permissions"] = rhs.perms;
            return node;
        }
        static bool decode(const Node& node, _Group& rhs)
        {
            using namespace std;
            string name;
            for (const auto& kv : node)
            {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name = kv.first.as<std::string>();
                break;
            }
            rhs.name = name;
            rhs.prefix = node[name]["prefix"].as<std::string>();
            rhs.inheritance = node[name]["inheritance"].as<std::string>();
            rhs.is_default = node[name]["default"].as<bool>();
            rhs.perms = node[name]["permissions"].as<std::vector<string>>();
            return true;
        }
    };
} // namespace YAML

string utf8_to_string(const char* utf8str, const locale& loc)
{
    // UTF-8 to wstring
    wstring_convert<codecvt_utf8<wchar_t>> wconv;
    wstring wstr = wconv.from_bytes(utf8str);
    // wstring to string
    vector<char> buf(wstr.size());
    use_facet<ctype<wchar_t>>(loc).narrow(wstr.data(), wstr.data() + wstr.size(), '?', buf.data());
    return string(buf.data(), buf.size());
}

namespace YAML
{
    template <>
    struct convert<_User>
    {
        static Node encode(const _User& rhs)
        {
            Node node;
            node[rhs.nickname] = YAML::Node(YAML::NodeType::Map);
            node[rhs.nickname]["group"] = (rhs.group);
            node[rhs.nickname]["prefix"] = (rhs.prefix);
            node[rhs.nickname]["suffix"] = (rhs.suffix);
            if (rhs.permissions.size() == 0)
            {
                node[rhs.nickname]["permissions"] = {};
            }
            node[rhs.nickname]["permissions"] = rhs.permissions;
            return node;
        }

        static bool decode(const Node& node, _User& rhs) {
            using namespace std;
            string name;
            for (const auto& kv : node) {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name = kv.first.as<std::string>();
                break;
            }
            rhs.nickname = name;
            rhs.group = node[name]["group"].as<string>();
            rhs.prefix = node[name]["prefix"].as<string>();
            rhs.suffix = node[name]["suffix"].as<string>();
            rhs.permissions = node[name]["permissions"].as<vector<string>>();
            return true;
        }
    };
} // namespace YAML


_Group load_group(string name) {
    YAML::Node config = YAML::LoadFile("plugins/PurePerms/groups.yml");
    _Group group;
    using namespace std;
    for (const auto& p : config["groups"]) {
        _Group g = p.as<_Group>();
        if (g.name == name)
        {
            group = g;
            break;
        }
    }
    return group;
}

_User load_user(string nick) {
    YAML::Node config = YAML::LoadFile("plugins/PurePerms/users.yml");
    using namespace std;
    _User user;
    for (const auto& p : config["users"]) {
        _User us = p.as<_User>();
        if (nick == us.nickname)
        {
            user = us;
            break;
        }
    }
    return user;
}

KitUser load_kituser(string nick) {
    YAML::Node config = YAML::LoadFile("plugins/AdvancedKits/users.yml");
    using namespace std;
    KitUser user;
    for (const auto& p : config["users"]) {
        KitUser us = p.as<KitUser>();
        if (nick == us.nickname)
        {
            user = us;
            break;
        }
    }
    return user;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        LL::registerPlugin("AdvancedKits v1.0", "Урезанный порт aDVANCEDkITS с PMMP на LiteLoader 2.0", LL::Version(0, 0, 0, LL::Version::Release));
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

class Kit
{
public:
    string name;
    vector<string> items;
    string helmet, chestplate, leggings, boots;
    string cmd;
    string permission;
    string cooldown;
    vector<string> effects;
};

class Kits
{
public:
    vector<Kit> kits;
};

namespace YAML
{
    template <>
    struct convert<Kit>
    {
        static Node encode(const Kit& rhs)
        {
            Node node;
            node[rhs.name] = YAML::Node(YAML::NodeType::Map);
            node[rhs.name]["permission"] = rhs.permission;
            if (rhs.items.size() == 0)
            {
                node[rhs.name]["items"] = vector<string>(0);
            }
            node[rhs.name]["items"] = rhs.items;
            node[rhs.name]["helmet"] = rhs.helmet;
            node[rhs.name]["chestplate"] = rhs.chestplate;
            node[rhs.name]["leggings"] = rhs.leggings;
            node[rhs.name]["boots"] = rhs.boots;
            node[rhs.name]["cooldown"] = rhs.cooldown;
            node[rhs.name]["cmd"] = rhs.cmd;
            if (rhs.effects.size() == 0)
            {
                node[rhs.name]["effects"] = vector<string>(0);
            }
            node[rhs.name]["effects"] = rhs.effects;
            return node;
        }
        static bool decode(const Node& node, Kit& rhs)
        {
            using namespace std;
            string name;
            for (const auto& kv : node)
            {
                if (kv.first.as<std::string>() == "")
                {
                    continue;
                }
                name = kv.first.as<std::string>();
                break;
            }
            rhs.name = name;
            rhs.permission = node[name]["permission"].as<string>();
            rhs.items = node[name]["items"].as<std::vector<string>>();
            rhs.helmet = node[name]["helmet"].as<std::string>();
            rhs.chestplate = node[name]["chestplate"].as<std::string>();
            rhs.leggings = node[name]["leggings"].as<std::string>();
            rhs.boots = node[name]["boots"].as<std::string>();
            rhs.cooldown = node[name]["cooldown"].as<string>();
            rhs.cmd = node[name]["cmd"].as<std::string>();
            rhs.effects = node[name]["effects"].as<std::vector<string>>();
            return true;
        }
    };
} // namespace YAML

Kit load_kit(string name) {
    YAML::Node config = YAML::LoadFile("plugins/AdvancedKits/kits.yml");
    using namespace std;
    Kit kit;
    for (const auto& p : config["kits"]) {
        Kit us = p.as<Kit>();
        if (name == us.name)
        {
            kit = us;
            break;
        }
    }
    return kit;
}

vector<string> split(string s, string delimiter) {
    size_t         pos_start = 0, pos_end, delim_len = delimiter.length();
    string         token;
    vector<string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }
    res.push_back(s.substr(pos_start));
    return res;
}

YAML::Node  config;
Kits kits;
YAML::Node  config1;
KitUsers kusers;

std::string ReplaceAll(const std::string& inputStr, const std::string& src, const std::string& dst)
{
    std::regex rx(src.c_str());
    return std::regex_replace(inputStr, rx, dst);
}

class KitConsoleCmd : public Command
{
    CommandSelector<Actor> player;
    string name;
public:
    void execute(CommandOrigin const& ori, CommandOutput& output) const override
    {
        _Groups    groups;
        Users      users;
        Kits ks;
        auto nodes = YAML::LoadFile("plugins/PurePerms/users.yml");
        for (const auto& p : nodes["users"])
        {
            users.users.push_back(p.as<_User>());
        }
        auto nodes1 = YAML::LoadFile("plugins/PurePerms/groups.yml");
        for (const auto& p : nodes1["groups"])
        {
            groups.groups.push_back(p.as<_Group>());
        }
        auto nodes2 = YAML::LoadFile("plugins/AdvancedKits/kits.yml");
        for (const auto& p : nodes2["kits"])
        {
            kits.kits.push_back(p.as<Kit>());
        }
        if (name == "" || player.getName() == "")
        {
            output.error("Invalid argument command!");
            return;
        }
        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console && name != "" && player.getName() != "")
        {
            for (auto v : kits.kits)
            {
                if (name == v.name)
                {
                    auto db = Level::getAllPlayers();
                    for (auto x : db)
                    {
                        auto res_nick = split(x->getName(), " ");
                        if (player.getName() == res_nick[2])
                        {
                            for (auto it : v.items)
                            {
                                auto pars = split(it, ":");
                                if (pars.size() == 3)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(pars[2])); //кол-во
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else  if (pars.size() == 4)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(pars[2])); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(pars[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else  if (pars.size() == 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(pars[2])); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(pars[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(pars[4].c_str()));
                                    ench->putShort("lvl", stoi(pars[5].c_str()));
                                    list_ench->add(move(ench));
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else  if (pars.size() > 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(pars[2])); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(pars[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    for (int i = 4; i < pars.size()-1; i+=2)
                                    {
                                        auto ench = CompoundTag::create();
                                        ench->putShort("id", stoi(pars[i].c_str()));
                                        ench->putShort("lvl", stoi(pars[i+1].c_str()));
                                        list_ench->add(move(ench));
                                    }
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                  //  cerr << nbt->toSNBT() << endl;
                                    x->add(*item);
                                }
                            }
                            auto helmet = split(v.helmet, ":");
                            if (helmet.size() == 3)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + helmet[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(helmet[2])); //кол-во
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (helmet.size() == 4)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + helmet[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(helmet[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (helmet.size() == 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + helmet[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(helmet[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                auto ench = CompoundTag::create();
                                ench->putShort("id", stoi(helmet[4].c_str()));
                                ench->putShort("lvl", stoi(helmet[5].c_str()));
                                list_ench->add(move(ench));
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (helmet.size() > 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + helmet[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(helmet[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                for (int i = 4; i < helmet.size() - 1; i+=2)
                                {
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(helmet[i].c_str()));
                                    ench->putShort("lvl", stoi(helmet[i + 1].c_str()));
                                    list_ench->add(move(ench));
                                }
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            auto chestplate = split(v.chestplate, ":");
                            if (chestplate.size() == 3)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(chestplate[2])); //кол-во
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (chestplate.size() == 4)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(chestplate[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (chestplate.size() == 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(chestplate[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                auto ench = CompoundTag::create();
                                ench->putShort("id", stoi(chestplate[4].c_str()));
                                ench->putShort("lvl", stoi(chestplate[5].c_str()));
                                list_ench->add(move(ench));
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (chestplate.size() > 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(chestplate[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                for (int i = 4; i < chestplate.size() - 1; ++i)
                                {
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(chestplate[i].c_str()));
                                    ench->putShort("lvl", stoi(chestplate[i + 1].c_str()));
                                    list_ench->add(move(ench));
                                }
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            auto leggings = split(v.leggings, ":");
                            if (leggings.size() == 3)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + leggings[0]));//предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(leggings[2])); //кол-во
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (leggings.size() == 4)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + leggings[0]));///предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(leggings[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (leggings.size() == 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + leggings[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(leggings[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                auto ench = CompoundTag::create();
                                ench->putShort("id", stoi(leggings[4].c_str()));
                                ench->putShort("lvl", stoi(leggings[5].c_str()));
                                list_ench->add(move(ench));
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (leggings.size() > 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + leggings[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(leggings[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                for (int i = 4; i < leggings.size() - 1;i+=2)
                                {
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(leggings[i].c_str()));
                                    ench->putShort("lvl", stoi(leggings[i + 1].c_str()));
                                    list_ench->add(move(ench));
                                }
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            auto boots = split(v.boots, ":");
                            if (boots.size() == 3)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + boots[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(boots[2])); //кол-во
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (boots.size() == 4)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(boots[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (boots.size() == 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(boots[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                auto ench = CompoundTag::create();
                                ench->putShort("id", stoi(boots[4].c_str()));
                                ench->putShort("lvl", stoi(boots[5].c_str()));
                                list_ench->add(move(ench));
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (boots.size() > 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(boots[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                for (int i = 4; i < boots.size() - 1; i+=2)
                                {
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(boots[i].c_str()));
                                    ench->putShort("lvl", stoi(boots[i + 1].c_str()));
                                    list_ench->add(move(ench));
                                }
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            for (int j = 0; j < v.effects.size(); j+=3)
                            {
                                auto effect = split(v.effects[j], ":");
                                MobEffectInstance inst = MobEffectInstance::MobEffectInstance(stoi(effect[0]), stoi(effect[1]), stoi(effect[2]));
                                try
                                {
                                    x->addEffect(inst);
                                }
                                catch (...)
                                {
                                    ;
                                }
                            }
                            x->sendText(v.cmd, TextType::CHAT);
                            output.success("Succefull");
                            return;
                        }
                    }
                }
            }
        }
        output.error("Player not found!");
        return;
    }
    static void setup(CommandRegistry* registry) 
    {
        registry->registerCommand(
            "kitconsole", "Give kit for player from console.", CommandPermissionLevel::Any, { (CommandFlagValue)0 },
            { (CommandFlagValue)0x80 });
        registry->registerOverload<KitConsoleCmd>("kitconsole", RegisterCommandHelper::makeMandatory(&KitConsoleCmd::player, "player"), RegisterCommandHelper::makeMandatory(&KitConsoleCmd::name, "name"));
    }
};

extern "C" char* strptime(const char* s,
    const char* f,
    struct tm* tm) {
    // Isn't the C++ standard lib nice? std::get_time is defined such that its
    // format parameters are the exact same as strptime. Of course, we have to
    // create a string stream first, and imbue it with the current C locale, and
    // we also have to make sure we return the right things if it fails, or
    // if it succeeds, but this is still far simpler an implementation than any
    // of the versions in any of the C standard libraries.
    std::istringstream input(s);
    input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
    input >> std::get_time(tm, f);
    if (input.fail()) {
        return nullptr;
    }
    return (char*)(s + input.tellg());
}

long gettimezone() {
    TIME_ZONE_INFORMATION tzinfo;
    GetTimeZoneInformation(&tzinfo);
    return tzinfo.Bias;
}

using time_point = std::chrono::system_clock::time_point;
std::string serializeTimePoint(const time_point& time, const std::string& format)
{
    std::time_t tt = std::chrono::system_clock::to_time_t(time);
    std::tm tm = *std::gmtime(&tt); //GMT (UTC)
    //std::tm tm = *std::localtime(&tt); //Locale time-zone, usually UTC by default.
    std::stringstream ss;
    ss << std::put_time(&tm, format.c_str());
    return ss.str();
}

unsigned long tsfstr(std::string datetime = "1970.01.01 00:00:00") {
    struct std::tm tm;
    std::istringstream ss(datetime);
    ss >> std::get_time(&tm, "%Y.%m.%d %H:%M:%S"); // or just %T in this case
    std::time_t time = mktime(&tm);
    return time;
}

const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y.%m.%d %X", &tstruct);

    return buf;
}

class KitCmd : public Command
{
    string name;
public:
    void execute(CommandOrigin const& ori, CommandOutput& output) const override
    {
        _Groups    groups;
        Users      users;
        Kits ks;
        KitUsers kusers;
        auto nodes = YAML::LoadFile("plugins/PurePerms/users.yml");
        for (const auto& p : nodes["users"])
        {
            users.users.push_back(p.as<_User>());
        }
        auto nodes1 = YAML::LoadFile("plugins/PurePerms/groups.yml");
        for (const auto& p : nodes1["groups"])
        {
            groups.groups.push_back(p.as<_Group>());
        }
        auto nodes2 = YAML::LoadFile("plugins/AdvancedKits/kits.yml");
        for (const auto& p : nodes2["kits"])
        {
            kits.kits.push_back(p.as<Kit>());
        }
        auto nodes3 = YAML::LoadFile("plugins/AdvancedKits/users.yml");
        for (const auto& p : nodes3["users"])
        {
            kusers.users.push_back(p.as<KitUser>());
        }
        if (name == "")
        {
            output.error("Invalid argument command!");
            return;
        }
        if (ori.getPermissionsLevel() == CommandPermissionLevel::Console)
        {
            output.error("Usage /kitconsole <player> <kit> for giving kit for player!");
            return;
        }
        auto plain = ori.getPlayer()->getName();
        using namespace std;
        auto nick = split(plain, " ");
        string res_nick1;
        for (auto n : nick)
        {
            for (auto v : users.users)
            {
                if (n == v.nickname)
                {
                    res_nick1 = n;
                    break;
                }
            }
        }
        auto kit = load_kit(name);
        auto pl = load_user(res_nick1);
        auto gr = load_group(pl.group);
        auto ku = load_kituser(res_nick1);
        for (auto v : pl.permissions)
        {
            for (auto cooldown : ku.cooldown_table)
            {
                auto vec = split(cooldown, " ");
                struct tm tm1, tm2;
                time_t t1, t2, t3;
                memset(&tm1, 0, sizeof(struct tm));
                memset(&tm2, 0, sizeof(struct tm));
                strptime(currentDateTime().c_str(), "%Y.%m.%d %H:%M:%S", &tm2);
                strptime((vec[0] + " " + vec[1]).c_str(), "%Y.%m.%d %H:%M:%S", &tm1);
                t1 = mktime(&tm1);
                t2 = mktime(&tm2);
                if (t2 <= t1)
                {
                    break;
                }
                else if (t2 > t1)
                {
                    output.error("Wait before using the kit again!");
                    return;
                }
            }
             for (auto v1 : kits.kits)
            {
                if (name == v1.name)
                {
                    auto db = Level::getAllPlayers();
                    for (auto x : db)
                    {
                        auto res_nick = split(x->getName(), " ");
                        if (v == kit.permission)
                        {
                            for (auto it : v1.items)
                            {
                                auto pars = split(it, ":");
                                if (pars.size() == 3)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(pars[2])); //кол-во
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else  if (pars.size() == 4)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(pars[2])); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(pars[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else  if (pars.size() == 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(pars[2])); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(pars[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(pars[4].c_str()));
                                    ench->putShort("lvl", stoi(pars[5].c_str()));
                                    list_ench->add(move(ench));
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else  if (pars.size() > 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(pars[2])); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(pars[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    for (int i = 4; i < pars.size()-1; i+=2)
                                    {
                                        auto ench = CompoundTag::create();
                                        ench->putShort("id", stoi(pars[i].c_str()));
                                        ench->putShort("lvl", stoi(pars[i+1].c_str()));
                                        list_ench->add(move(ench));
                                    }
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                  //  cerr << nbt->toSNBT() << endl;
                                    x->add(*item);
                                }
                            }
                            auto helmet = split(v1.helmet, ":");
                            if (helmet.size() == 3)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + helmet[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(helmet[2])); //кол-во
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (helmet.size() == 4)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + helmet[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(helmet[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (helmet.size() == 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + helmet[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(helmet[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                auto ench = CompoundTag::create();
                                ench->putShort("id", stoi(helmet[4].c_str()));
                                ench->putShort("lvl", stoi(helmet[5].c_str()));
                                list_ench->add(move(ench));
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (helmet.size() > 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + helmet[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(helmet[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                for (int i = 4; i < helmet.size() - 1; i+=2)
                                {
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(helmet[i].c_str()));
                                    ench->putShort("lvl", stoi(helmet[i + 1].c_str()));
                                    list_ench->add(move(ench));
                                }
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            auto chestplate = split(v1.chestplate, ":");
                            if (chestplate.size() == 3)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(chestplate[2])); //кол-во
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (chestplate.size() == 4)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(chestplate[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (chestplate.size() == 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(chestplate[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                auto ench = CompoundTag::create();
                                ench->putShort("id", stoi(chestplate[4].c_str()));
                                ench->putShort("lvl", stoi(chestplate[5].c_str()));
                                list_ench->add(move(ench));
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (chestplate.size() > 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(chestplate[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                for (int i = 4; i < chestplate.size() - 1; ++i)
                                {
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(chestplate[i].c_str()));
                                    ench->putShort("lvl", stoi(chestplate[i + 1].c_str()));
                                    list_ench->add(move(ench));
                                }
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            auto leggings = split(v1.leggings, ":");
                            if (leggings.size() == 3)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + leggings[0]));//предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(leggings[2])); //кол-во
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (leggings.size() == 4)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + leggings[0]));///предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(leggings[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (leggings.size() == 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + leggings[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(leggings[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                auto ench = CompoundTag::create();
                                ench->putShort("id", stoi(leggings[4].c_str()));
                                ench->putShort("lvl", stoi(leggings[5].c_str()));
                                list_ench->add(move(ench));
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (leggings.size() > 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + leggings[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(leggings[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                for (int i = 4; i < leggings.size() - 1;i+=2)
                                {
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(leggings[i].c_str()));
                                    ench->putShort("lvl", stoi(leggings[i + 1].c_str()));
                                    list_ench->add(move(ench));
                                }
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            auto boots = split(v1.boots, ":");
                            if (boots.size() == 3)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + boots[0])); //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(boots[2])); //кол-во
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (boots.size() == 4)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(boots[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (boots.size() == 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(boots[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                auto ench = CompoundTag::create();
                                ench->putShort("id", stoi(boots[4].c_str()));
                                ench->putShort("lvl", stoi(boots[5].c_str()));
                                list_ench->add(move(ench));
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            else if (boots.size() > 6)
                            {
                                auto nbt = CompoundTag::create();
                                nbt->putByte("WasPickedUp", 0);
                                nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                auto nbt1 = CompoundTag::create();
                                auto nbt2 = CompoundTag::create();
                                nbt2->putString("Name", std::move(boots[3]));
                                nbt1->putInt("RepairCost", 0);
                                nbt1->putCompound("display", move(nbt2));
                                auto list_ench = ListTag::create();
                                for (int i = 4; i < boots.size() - 1; i+=2)
                                {
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(boots[i].c_str()));
                                    ench->putShort("lvl", stoi(boots[i + 1].c_str()));
                                    list_ench->add(move(ench));
                                }
                                nbt1->put("ench", move(list_ench));
                                nbt->put("tag", move(nbt1));
                                auto item = ItemStack::create(std::move(nbt));
                                x->add(*item);
                            }
                            for (int j = 0; j < v1.effects.size(); j+=3)
                            {
                                auto effect = split(v1.effects[j], ":");
                                MobEffectInstance inst = MobEffectInstance::MobEffectInstance(stoi(effect[0]), stoi(effect[1]), stoi(effect[2]));
                                try
                                {
                                    x->addEffect(inst);
                                }
                                catch (...)
                                {
                                    ;
                                }
                            }
                            config1.reset();
                            bool is_succ = false;
                            for (int i = 0; i < kusers.users.size(); ++i)
                            {
                                if (res_nick1 == kusers.users[i].nickname)
                                {
                                    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
                                    auto tm = split(kit.cooldown, ":");
                                    std::chrono::time_point<std::chrono::system_clock> end = now + std::chrono::days(stoi(tm[0])) + std::chrono::hours(stoi(tm[1])) + std::chrono::minutes(stoi(tm[2])) + std::chrono::seconds(stoi(tm[3]));
                                    kusers.users[i].cooldown_table.push_back(name + " " + serializeTimePoint(end, "%Y.%m.%d %H:%M:%S"));
                                    is_succ = true;
                                    break;
                                }
                            }
                            if (!is_succ)
                            {
                                std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
                                auto tm = split(kit.cooldown, ":");
                                std::chrono::time_point<std::chrono::system_clock> end = now + std::chrono::days(stoi(tm[0])) + std::chrono::hours(stoi(tm[1])) + std::chrono::minutes(stoi(tm[2])) + std::chrono::seconds(stoi(tm[3]));
                                KitUser kuser;
                                kuser.nickname = res_nick1;
                                kuser.cooldown_table.push_back(name + " " + serializeTimePoint(end, "%Y.%m.%d %H:%M:%S"));
                                kusers.users.push_back(kuser);
                            }
                            for (auto v2 : kusers.users)
                                config1["users"].push_back(v2);
                            remove("plugins/AdvancedKits/users.yml");
                            ofstream fout("plugins/AdvancedKits/users.yml");
                            fout << config1;
                            fout.close();
                            output.success(v1.cmd);
                            return;
                        }
                    }
                }
            }
            for (auto v : gr.perms)
            {
                for (auto cooldown : ku.cooldown_table)
                {
                    auto vec = split(cooldown, " ");
                    struct tm tm1, tm2;
                    time_t t1, t2, t3;
                    memset(&tm1, 0, sizeof(struct tm));
                    memset(&tm2, 0, sizeof(struct tm));
                    strptime(currentDateTime().c_str(), "%Y.%m.%d %H:%M:%S", &tm2);
                    strptime((vec[0] + " " + vec[1]).c_str(), "%Y.%m.%d %H:%M:%S", &tm1);
                    t1 = mktime(&tm1);
                    t2 = mktime(&tm2);
                    if (t2 <= t1)
                    {
                        break;
                    }
                    else if (t2 > t1)
                    {
                        output.error("Wait before using the kit again!");
                        return;
                    }
                }
                for (auto v1 : kits.kits)
                {
                    if (name == v1.name)
                    {
                        auto db = Level::getAllPlayers();
                        for (auto x : db)
                        {
                            auto res_nick = split(x->getName(), " ");
                            if (v == kit.permission)
                            {
                                for (auto it : v1.items)
                                {
                                    auto pars = split(it, ":");
                                    if (pars.size() == 3)
                                    {
                                        auto nbt = CompoundTag::create();
                                        nbt->putByte("WasPickedUp", 0);
                                        nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                        nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                        nbt->putByte("Count", stoi(pars[2])); //кол-во
                                        auto item = ItemStack::create(std::move(nbt));
                                        x->add(*item);
                                    }
                                    else  if (pars.size() == 4)
                                    {
                                        auto nbt = CompoundTag::create();
                                        nbt->putByte("WasPickedUp", 0);
                                        nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                        nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                        nbt->putByte("Count", stoi(pars[2])); //кол-во
                                        auto nbt1 = CompoundTag::create();
                                        auto nbt2 = CompoundTag::create();
                                        nbt2->putString("Name", std::move(pars[3]));
                                        nbt1->putInt("RepairCost", 0);
                                        nbt1->putCompound("display", move(nbt2));
                                        auto item = ItemStack::create(std::move(nbt));
                                        x->add(*item);
                                    }
                                    else  if (pars.size() == 6)
                                    {
                                        auto nbt = CompoundTag::create();
                                        nbt->putByte("WasPickedUp", 0);
                                        nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                        nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                        nbt->putByte("Count", stoi(pars[2])); //кол-во
                                        auto nbt1 = CompoundTag::create();
                                        auto nbt2 = CompoundTag::create();
                                        nbt2->putString("Name", std::move(pars[3]));
                                        nbt1->putInt("RepairCost", 0);
                                        nbt1->putCompound("display", move(nbt2));
                                        auto list_ench = ListTag::create();
                                        auto ench = CompoundTag::create();
                                        ench->putShort("id", stoi(pars[4].c_str()));
                                        ench->putShort("lvl", stoi(pars[5].c_str()));
                                        list_ench->add(move(ench));
                                        nbt1->put("ench", move(list_ench));
                                        nbt->put("tag", move(nbt1));
                                        auto item = ItemStack::create(std::move(nbt));
                                        x->add(*item);
                                    }
                                    else  if (pars.size() > 6)
                                    {
                                        auto nbt = CompoundTag::create();
                                        nbt->putByte("WasPickedUp", 0);
                                        nbt->putShort("Damage", (short)stoi(pars[1].c_str())); //дамаг
                                        nbt->putString("Name", std::move("minecraft:" + pars[0])); //предмет(алмаз и т.д)
                                        nbt->putByte("Count", stoi(pars[2])); //кол-во
                                        auto nbt1 = CompoundTag::create();
                                        auto nbt2 = CompoundTag::create();
                                        nbt2->putString("Name", std::move(pars[3]));
                                        nbt1->putInt("RepairCost", 0);
                                        nbt1->putCompound("display", move(nbt2));
                                        auto list_ench = ListTag::create();
                                        for (int i = 4; i < pars.size() - 1; i += 2)
                                        {
                                            auto ench = CompoundTag::create();
                                            ench->putShort("id", stoi(pars[i].c_str()));
                                            ench->putShort("lvl", stoi(pars[i + 1].c_str()));
                                            list_ench->add(move(ench));
                                        }
                                        nbt1->put("ench", move(list_ench));
                                        nbt->put("tag", move(nbt1));
                                        auto item = ItemStack::create(std::move(nbt));
                                        //  cerr << nbt->toSNBT() << endl;
                                        x->add(*item);
                                    }
                                }
                                auto helmet = split(v1.helmet, ":");
                                if (helmet.size() == 3)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + helmet[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(helmet[2])); //кол-во
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (helmet.size() == 4)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + helmet[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(helmet[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (helmet.size() == 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + helmet[0]));  //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(helmet[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(helmet[4].c_str()));
                                    ench->putShort("lvl", stoi(helmet[5].c_str()));
                                    list_ench->add(move(ench));
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (helmet.size() > 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(helmet[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + helmet[0]));  //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(helmet[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(helmet[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    for (int i = 4; i < helmet.size() - 1; i += 2)
                                    {
                                        auto ench = CompoundTag::create();
                                        ench->putShort("id", stoi(helmet[i].c_str()));
                                        ench->putShort("lvl", stoi(helmet[i + 1].c_str()));
                                        list_ench->add(move(ench));
                                    }
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                auto chestplate = split(v1.chestplate, ":");
                                if (chestplate.size() == 3)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(chestplate[2])); //кол-во
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (chestplate.size() == 4)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(chestplate[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (chestplate.size() == 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(chestplate[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(chestplate[4].c_str()));
                                    ench->putShort("lvl", stoi(chestplate[5].c_str()));
                                    list_ench->add(move(ench));
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (chestplate.size() > 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(chestplate[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + chestplate[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(chestplate[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(chestplate[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    for (int i = 4; i < chestplate.size() - 1; ++i)
                                    {
                                        auto ench = CompoundTag::create();
                                        ench->putShort("id", stoi(chestplate[i].c_str()));
                                        ench->putShort("lvl", stoi(chestplate[i + 1].c_str()));
                                        list_ench->add(move(ench));
                                    }
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                auto leggings = split(v1.leggings, ":");
                                if (leggings.size() == 3)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + leggings[0]));//предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(leggings[2])); //кол-во
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (leggings.size() == 4)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + leggings[0]));///предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(leggings[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (leggings.size() == 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + leggings[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(leggings[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(leggings[4].c_str()));
                                    ench->putShort("lvl", stoi(leggings[5].c_str()));
                                    list_ench->add(move(ench));
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (leggings.size() > 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(leggings[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + leggings[0]));  //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(leggings[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(leggings[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    for (int i = 4; i < leggings.size() - 1; i += 2)
                                    {
                                        auto ench = CompoundTag::create();
                                        ench->putShort("id", stoi(leggings[i].c_str()));
                                        ench->putShort("lvl", stoi(leggings[i + 1].c_str()));
                                        list_ench->add(move(ench));
                                    }
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                auto boots = split(v1.boots, ":");
                                if (boots.size() == 3)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + boots[0])); //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(boots[2])); //кол-во
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (boots.size() == 4)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(boots[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (boots.size() == 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(boots[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    auto ench = CompoundTag::create();
                                    ench->putShort("id", stoi(boots[4].c_str()));
                                    ench->putShort("lvl", stoi(boots[5].c_str()));
                                    list_ench->add(move(ench));
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                else if (boots.size() > 6)
                                {
                                    auto nbt = CompoundTag::create();
                                    nbt->putByte("WasPickedUp", 0);
                                    nbt->putShort("Damage", (short)stoi(boots[1].c_str())); //дамаг
                                    nbt->putString("Name", std::move("minecraft:" + boots[0]));  //предмет(алмаз и т.д)
                                    nbt->putByte("Count", stoi(boots[2].c_str())); //кол-во
                                    auto nbt1 = CompoundTag::create();
                                    auto nbt2 = CompoundTag::create();
                                    nbt2->putString("Name", std::move(boots[3]));
                                    nbt1->putInt("RepairCost", 0);
                                    nbt1->putCompound("display", move(nbt2));
                                    auto list_ench = ListTag::create();
                                    for (int i = 4; i < boots.size() - 1; i += 2)
                                    {
                                        auto ench = CompoundTag::create();
                                        ench->putShort("id", stoi(boots[i].c_str()));
                                        ench->putShort("lvl", stoi(boots[i + 1].c_str()));
                                        list_ench->add(move(ench));
                                    }
                                    nbt1->put("ench", move(list_ench));
                                    nbt->put("tag", move(nbt1));
                                    auto item = ItemStack::create(std::move(nbt));
                                    x->add(*item);
                                }
                                for (int j = 0; j < v1.effects.size(); j += 3)
                                {
                                    auto effect = split(v1.effects[j], ":");
                                    MobEffectInstance inst = MobEffectInstance::MobEffectInstance(stoi(effect[0]), stoi(effect[1]), stoi(effect[2]));
                                    try
                                    {
                                        x->addEffect(inst);
                                    }
                                    catch (...)
                                    {
                                        ;
                                    }
                                }
                                config1.reset();
                                bool is_succ = false;
                                for (int i = 0; i < kusers.users.size(); ++i)
                                {
                                    if (res_nick1 == kusers.users[i].nickname)
                                    {
                                        std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
                                        auto tm = split(kit.cooldown, ":");
                                        std::chrono::time_point<std::chrono::system_clock> end = now + std::chrono::days(stoi(tm[0])) + std::chrono::hours(stoi(tm[1])) + std::chrono::minutes(stoi(tm[2])) + std::chrono::seconds(stoi(tm[3]));
                                        kusers.users[i].cooldown_table.push_back(name + " " + serializeTimePoint(end, "%Y.%m.%d %H:%M:%S"));
                                        is_succ = true;
                                        break;
                                    }
                                }
                                if (!is_succ)
                                {
                                    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
                                    auto tm = split(kit.cooldown, ":");
                                    std::chrono::time_point<std::chrono::system_clock> end = now + std::chrono::days(stoi(tm[0])) + std::chrono::hours(stoi(tm[1])) + std::chrono::minutes(stoi(tm[2])) + std::chrono::seconds(stoi(tm[3]));
                                    KitUser kuser;
                                    kuser.nickname = res_nick1;
                                    kuser.cooldown_table.push_back(name + " " + serializeTimePoint(end, "%Y.%m.%d %H:%M:%S"));
                                    kusers.users.push_back(kuser);
                                }
                                for (auto v2 : kusers.users)
                                    config1["users"].push_back(v2);
                                remove("plugins/AdvancedKits/users.yml");
                                ofstream fout("plugins/AdvancedKits/users.yml");
                                fout << config1;
                                fout.close();
                                output.success(v1.cmd);
                                return;
                            }
                        }
                    }
                }
            }
        }
        output.error("You do not have permission to execute this command!");
        return;
    }
    static void setup(CommandRegistry* registry)
    {
        registry->registerCommand(
            "kit", "Give kit.", CommandPermissionLevel::Any, { (CommandFlagValue)0 },
            { (CommandFlagValue)0x80 });
        registry->registerOverload<KitCmd>("kit", RegisterCommandHelper::makeMandatory(&KitCmd::name, "name"));
    }
};

void entry();

extern "C" {
    _declspec(dllexport) void onPostInit() {
        std::ios::sync_with_stdio(false);
        entry();
    }
}

void entry()
{
    Event::ServerStartedEvent::subscribe([](const Event::ServerStartedEvent& ev)
        {
            try
            {
                using namespace std;
                std::filesystem::create_directory("plugins/AdvancedKits");
                ifstream in("plugins/AdvancedKits/kits.yml");
                if (!in.is_open())
                {
                    in.close();
                    Kit kit;
                    kit.name = "Start";
                    kit.permission = "advancedkits.start";
                    kit.cmd = "You have been given a set kit start!";
                    kit.items.push_back("diamond:0:16");
                    kit.items.push_back("apple:0:64");
                    kit.items.push_back("wood:0:64");
                    kit.items.push_back("torch:0:64");
                    kit.items.push_back("cobblestone:0:64");
                    kit.items.push_back("iron_sword:0:1:Железный меч:12:1:11:2");
                    kit.items.push_back("iron_axe:0:1:Железный топор:15:2");
                    kit.items.push_back("iron_pickaxe:0:1:Железная кирка:15:2:18:1");
                    kit.items.push_back("iron_shovel:0:1:Железная лопата4");
                    kit.helmet = "iron_helmet:0:1";
                    kit.chestplate = "iron_chestplate:0:1";
                    kit.leggings = "iron_leggings:0:1";
                    kit.boots = "iron_boots:0:1";
                    kit.cooldown = "1:0:0:0";
                    kit.effects.push_back("1:10000:1");
                    config["kits"].push_back(kit);
                    std::ofstream fout("plugins/AdvancedKits/kits.yml");
                    fout << config;
                    fout.close();
                }
                config = YAML::LoadFile("plugins/AdvancedKits/kits.yml");
                in.open("plugins/AdvancedKits/users.yml");
                if (!in.is_open())
                {
                    in.close();
                    KitUser user;
                    user.nickname = "test";
                    config1["users"].push_back(user);
                    std::ofstream fout("plugins/AdvancedKits/users.yml");
                    locale utfFile("en_US.UTF-8");
                    fout.imbue(utfFile);
                    fout << config1;
                    fout.close();
                }
                config1 = YAML::LoadFile("plugins/AdvancedKits/users.yml");
                for (const auto& p : config["kits"]) {
                    kits.kits.push_back(p.as<Kit>());
                }
                kits.kits.clear();
                for (const auto& p : config1["users"]) {
                    kusers.users.push_back(p.as<KitUser>());
                }
            }
            catch (std::exception& e)
            {
                std::cerr << e.what() << std::endl;
            }
            return 1;
        });
    Event::RegCmdEvent::subscribe([](const Event::RegCmdEvent& ev) 
    {
       KitConsoleCmd::setup(ev.mCommandRegistry);
       KitCmd::setup(ev.mCommandRegistry);
       return 1;
    });
}