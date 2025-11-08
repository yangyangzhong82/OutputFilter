#include "mod/MyMod.h"

#include "ll/api/mod/RegisterHelper.h"
#include <MinHook.h>
#include <filesystem>
#include <ll/api/Config.h>
#include <regex>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>


namespace my_mod {

// 配置数据定义
struct ConfigData {
    int                      version  = 1;
    std::vector<std::string> patterns = {};
};
static ConfigData configData;

// 生效过滤关键字列表
static std::vector<std::string>  patternsA;
static std::vector<std::wstring> patternsW;

// 正则过滤列表
static std::vector<std::regex>  patternsRegexA;
static std::vector<std::wregex> patternsRegexW;

// 原始函数指针
using WriteConsoleA_t                    = BOOL(WINAPI*)(HANDLE, const VOID*, DWORD, LPDWORD, LPVOID);
using WriteConsoleW_t                    = BOOL(WINAPI*)(HANDLE, const WCHAR*, DWORD, LPDWORD, LPVOID);
static WriteConsoleA_t TrueWriteConsoleA = nullptr;
static WriteConsoleW_t TrueWriteConsoleW = nullptr;


static BOOL WINAPI HookedWriteConsoleA(HANDLE h, const VOID* buf, DWORD n, LPDWORD w, LPVOID r) {
    std::string s((const char*)buf, n);
    for (auto& reg : patternsRegexA)
        if (std::regex_search(s, reg)) return TRUE;
    return TrueWriteConsoleA(h, buf, n, w, r);
}


static BOOL WINAPI HookedWriteConsoleW(HANDLE h, const WCHAR* buf, DWORD n, LPDWORD w, LPVOID r) {
    std::wstring s(buf, n);
    for (auto& reg : patternsRegexW)
        if (std::regex_search(s, reg)) return TRUE;
    return TrueWriteConsoleW(h, buf, n, w, r);
}

// 将 UTF-8 转为 UTF-16
static std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return L"";
    int          size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size);
    return wstr;
}

MyMod& MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

bool MyMod::load() {
    // 加载并初始化配置
    const auto& cfgDir  = getSelf().getConfigDir();
    auto        cfgPath = cfgDir / "config.json";
    if (!ll::config::loadConfig(configData, cfgPath)) {
        getSelf().getLogger().warn("Cannot load config, saving defaults to {}", cfgPath.string());
        ll::config::saveConfig(configData, cfgPath);
    }
    patternsA = configData.patterns;
    patternsW.clear();
    for (auto& p : patternsA) {
        patternsW.emplace_back(utf8_to_wide(p));
    }

    // 编译正则列表
    patternsRegexA.clear();
    for (auto& pat : patternsA) patternsRegexA.emplace_back(pat, std::regex_constants::ECMAScript);
    patternsRegexW.clear();
    for (auto& wpat : patternsW) patternsRegexW.emplace_back(wpat, std::regex_constants::ECMAScript);

    // 初始化和启用 MinHook
    if (MH_Initialize() == MH_OK) {
        MH_CreateHookApi(
            L"kernel32",
            "WriteConsoleA",
            &HookedWriteConsoleA,
            reinterpret_cast<LPVOID*>(&TrueWriteConsoleA)
        );
        MH_CreateHookApi(
            L"kernel32",
            "WriteConsoleW",
            &HookedWriteConsoleW,
            reinterpret_cast<LPVOID*>(&TrueWriteConsoleW)
        );
        MH_EnableHook(MH_ALL_HOOKS);
        getSelf().getLogger().info("Console API hooks installed");
    } else {
        getSelf().getLogger().warn("MH_Initialize failed");
    }
    getSelf().getLogger().info("MyMod loaded!");
    return true;
}

bool MyMod::enable() {
    getSelf().getLogger().debug("Enabling...");
    getSelf().getLogger().info("OutputFilter插件加载成功，作者：yangyangzhong82");
    return true;
}

bool MyMod::disable() {
    getSelf().getLogger().debug("Disabling...");
    // Code for disabling the mod goes here.
    return true;
}

} // namespace my_mod

LL_REGISTER_MOD(my_mod::MyMod, my_mod::MyMod::getInstance());
