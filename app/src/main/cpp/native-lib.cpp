#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <android/log.h>
#include <cmath>
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_android.h"
#include "ImGui/imgui_impl_opengl3.h"
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include "Misc/Logging.h"
#include "Misc/JNIStuff.h"
#include "Misc/FileWrapper.h"
#include "Misc/Utils.h"
#include "ByNameModding/BNM.hpp"
#include "Obfuscation/Obfuscate.h"
#include "Obfuscation/Custom_Obfuscate.h"
#include "Unity/Screen.h"
#include "Unity/Input.h"
#include "Hooking/JNIHooks.h"
#include "Misc/ImGuiStuff.h"

#define ewgegggg OBFUSCATE("libil2cpp.so")

// Function offsets
#define GET_INT_OFFSET      "0xxxxx" // ObscuredPrefs.GetInt
#define SET_INT_OFFSET      "0xxxxx" // ObscuredPrefs.SetInt
#define SET_STRING_OFFSET   "0xxxxx" // ObscuredPrefs.SetString
#define GET_STRING_OFFSET   "0xxxxx" // ObscuredPrefs.GetString
#define HAS_KEY_OFFSET      "0xxxxx" // ObscuredPrefs.HasKey
#define CHAT_MESSAGE_OFFSET "0xxxxx" // ChatMessage
#define PUSH_MESSAGE_KILLS_OFFSET "0xxxxx" // unused
#define GET_BUTTON_DOWN_OFFSET "0xxxxx" // unused

#pragma clang diagnostic push
#pragma ide diagnostic ignored "NullDereference"

bool emulator = true;
bool imgui_initialized = false;
pthread_t main_render_thread = 0;

namespace ModMenu {
    struct monoString {
        void* klass;
        void* monitor;
        int length;
        uint16_t chars[1];

        int getLength() { return length; }
        uint16_t* getChars() { return chars; }
    };

    template<typename T>
    struct monoArray {
        void* klass;
        void* monitor;
        void* bounds;
        int32_t max_length;
        T elements[1];

        int32_t getLength() { return max_length; }
        T* getPointer() { return elements; }
    };
}

using MonoStringPtr = std::unique_ptr<char[], void(*)(char*)>;
MonoStringPtr MakeMonoStringPtr(char* ptr) {
    return MonoStringPtr(ptr, [](char* p) { if (p) delete[] p; });
}

ModMenu::monoString* CreateCustomMonoString(const char* str) {
    if (!str) return nullptr;

    int len = strlen(str);
    if (len == 0) return nullptr;

    size_t size = sizeof(ModMenu::monoString) + (len - 1) * sizeof(uint16_t);
    ModMenu::monoString* monoStr = (ModMenu::monoString*)malloc(size);
    if (!monoStr) {
        LOGE("CreateCustomMonoString: Failed to allocate memory");
        return nullptr;
    }

    monoStr->klass = nullptr;
    monoStr->monitor = nullptr;
    monoStr->length = len;

    uint16_t* chars = monoStr->getChars();
    for (int i = 0; i < len; ++i) {
        chars[i] = (uint16_t)str[i];
    }

    return monoStr;
}

MonoStringPtr MonoStringToUTF8(ModMenu::monoString* str) {
    if (!str) {
        LOGE("MonoStringToUTF8: Null string");
        return MakeMonoStringPtr(strdup(""));
    }
    int len = str->getLength();
    if (len <= 0) {
        return MakeMonoStringPtr(strdup(""));
    }
    uint16_t* chars = (uint16_t*)str->getChars();
    if (!chars) {
        LOGE("MonoStringToUTF8: Null chars");
        return MakeMonoStringPtr(strdup(""));
    }

    std::vector<char> buffer;
    buffer.reserve(len * 4 + 1);

    for (int i = 0; i < len; ++i) {
        uint32_t codePoint = 0;
        uint16_t c = chars[i];

        if (c >= 0xD800 && c <= 0xDBFF) {
            if (i + 1 < len) {
                uint16_t next = chars[i + 1];
                if (next >= 0xDC00 && next <= 0xDFFF) {
                    codePoint = 0x10000 + ((c - 0xD800) << 10) + (next - 0xDC00);
                    ++i;
                } else {
                    codePoint = 0xFFFD;
                }
            } else {
                codePoint = 0xFFFD;
            }
        } else if (c >= 0xDC00 && c <= 0xDFFF) {
            codePoint = 0xFFFD;
        } else {
            codePoint = c;
        }

        if (codePoint <= 0x7F) {
            buffer.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FF) {
            buffer.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            buffer.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint <= 0xFFFF) {
            buffer.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            buffer.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            buffer.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint <= 0x10FFFF) {
            buffer.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            buffer.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            buffer.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            buffer.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            buffer.push_back(static_cast<char>(0xEF));
            buffer.push_back(static_cast<char>(0xBF));
            buffer.push_back(static_cast<char>(0xBD));
        }
    }

    buffer.push_back('\0');

    char* result = new char[buffer.size()];
    std::memcpy(result, buffer.data(), buffer.size());
    return MakeMonoStringPtr(result);
}

std::set<std::string> knownPlayers;
std::mutex playersMutex;

void AddKnownPlayer(const std::string& playerName) {
    if (!playerName.empty() && playerName != "null") {
        std::lock_guard<std::mutex> lock(playersMutex);
        knownPlayers.insert(playerName);
    }
}

namespace ImGuiLogger {
    struct LogEntry {
        std::string function;
        std::string key;
        std::string value;
        std::string defaultValue;
        int hasKeyResult;
        bool isError;
        std::string errorMsg;
    };

    enum class FilterMode {
        None,
        Value_1,
        Value_0
    };

    std::vector<LogEntry> logBuffer;
    std::vector<std::string> knownKeys;
    std::mutex logMutex;
    const size_t maxLogLines = 100;

    bool logGetInt = true;
    bool logSetInt = true;
    bool logSetString = true;
    bool logGetString = true;
    bool logHasKey = true;
    bool logChatMessage = false;
    bool logPushMessageKills = false;
    bool logGetButtonDown = false;

    bool simplifiedLog = false;
    FilterMode filterMode = FilterMode::None;

    void AddKnownKey(const std::string& key) {
        if (!key.empty() && key != "null" && std::find(knownKeys.begin(), knownKeys.end(), key) == knownKeys.end()) {
            knownKeys.push_back(key);
        }
    }

    void Log(const char* function, const char* key, const char* value = nullptr, const char* defaultValue = nullptr, int hasKeyResult = -1) {
        std::lock_guard<std::mutex> lock(logMutex);
        LogEntry entry;
        entry.function = function ? function : "";
        entry.key = key ? key : "";
        entry.value = value ? value : "";
        entry.defaultValue = defaultValue ? defaultValue : "";
        entry.hasKeyResult = hasKeyResult;
        entry.isError = false;

        logBuffer.push_back(entry);
        if (entry.function != "ChatMessage" && entry.function != "PushMessageKills" && entry.function != "GetButtonDown" && !entry.key.empty() && entry.key != "null") {
            AddKnownKey(entry.key);
        }
        if (logBuffer.size() > maxLogLines) {
            logBuffer.erase(logBuffer.begin());
        }
    }

    void LogError(const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        std::lock_guard<std::mutex> lock(logMutex);
        LogEntry entry;
        entry.isError = true;
        entry.errorMsg = buf;
        logBuffer.push_back(entry);
        if (logBuffer.size() > maxLogLines) {
            logBuffer.erase(logBuffer.begin());
        }

        __android_log_print(ANDROID_LOG_ERROR, "ModMenu", "Error: %s", buf);
    }

    std::string FormatLog(const LogEntry& entry) {
        if (entry.isError) {
            return entry.errorMsg;
        }
        if (simplifiedLog) {
            if (entry.function == "HasKey") {
                return entry.key.empty() ? "HasKey null" : "HasKey " + entry.key + " (" + std::to_string(entry.hasKeyResult) + ")";
            } else if (entry.function == "ChatMessage") {
                return entry.key + ": " + entry.value;
            } else if (entry.function == "PushMessageKills") {
                return entry.key + " killed by " + entry.value;
            } else if (entry.function == "GetButtonDown") {
                return "GetButtonDown: " + entry.key;
            } else {
                std::string result = entry.function + " " + (entry.key.empty() ? "null" : entry.key);
                if (!entry.value.empty()) {
                    result += " (" + entry.value + ")";
                }
                return result;
            }
        } else {
            if (entry.function == "GetInt") {
                return "ObscuredPrefs.GetInt called with key: " + (entry.key.empty() ? "null" : entry.key) +
                       ", value: " + entry.value + ", default: " + entry.defaultValue;
            } else if (entry.function == "SetInt") {
                return "ObscuredPrefs.SetInt called with key: " + (entry.key.empty() ? "null" : entry.key) +
                       ", value: " + entry.value;
            } else if (entry.function == "SetString") {
                return "ObscuredPrefs.SetString called with key: " + (entry.key.empty() ? "null" : entry.key) +
                       ", value: " + (entry.value.empty() ? "null" : entry.value);
            } else if (entry.function == "GetString") {
                return "ObscuredPrefs.GetString called with key: " + (entry.key.empty() ? "null" : entry.key) +
                       ", result: " + (entry.value.empty() ? "null" : entry.value) +
                       ", default: " + (entry.defaultValue.empty() ? "null" : entry.defaultValue);
            } else if (entry.function == "HasKey") {
                return "ObscuredPrefs.HasKey called with key: " + (entry.key.empty() ? "null" : entry.key) +
                       ", result: " + std::to_string(entry.hasKeyResult);
            } else if (entry.function == "ChatMessage") {
                return "ChatMessage from " + entry.key + ": " + entry.value;
            } else if (entry.function == "PushMessageKills") {
                return "PushMessageKills: " + entry.key + " killed by " + entry.value;
            } else if (entry.function == "GetButtonDown") {
                return "GetButtonDown called with button: " + entry.key;
            }
            return "Unknown log entry";
        }
    }

    std::string GetMessageLogs() {
        std::string messageLogs;
        for (const auto& entry : logBuffer) {
            if (entry.function == "ChatMessage") {
                messageLogs += entry.key + ": " + entry.value + "\n";
            } else if (entry.function == "PushMessageKills") {
                messageLogs += entry.key + " killed by " + entry.value + "\n";
            } else if (entry.function == "GetButtonDown") {
                messageLogs += "GetButtonDown: " + entry.key + "\n";
            }
        }
        return messageLogs;
    }
}

typedef int (*GetInt_t)(ModMenu::monoString* key, int defaultValue);
GetInt_t old_GetInt;
int GetInt_hook(ModMenu::monoString* key, int defaultValue) {
    try {
        if (!old_GetInt) {
            ImGuiLogger::LogError("GetInt_hook: old_GetInt is null");
            return defaultValue;
        }

        if (!key) {
            if (ImGuiLogger::logGetInt) ImGuiLogger::Log("GetInt", nullptr, nullptr, std::to_string(defaultValue).c_str());
            return old_GetInt(key, defaultValue);
        }

        int value = old_GetInt(key, defaultValue);
        if (ImGuiLogger::logGetInt) {
            auto keyStr = MonoStringToUTF8(key);
            ImGuiLogger::Log("GetInt", keyStr.get(), std::to_string(value).c_str(), std::to_string(defaultValue).c_str());
        }
        return value;
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("GetInt_hook: Exception: %s", e.what());
        return defaultValue;
    } catch (...) {
        ImGuiLogger::LogError("GetInt_hook: Unknown exception");
        return defaultValue;
    }
}

typedef void (*SetInt_t)(ModMenu::monoString* key, int value);
SetInt_t old_SetInt;
void SetInt_hook(ModMenu::monoString* key, int value) {
    try {
        if (!old_SetInt) {
            ImGuiLogger::LogError("SetInt_hook: old_SetInt is null");
            return;
        }

        if (!key) {
            if (ImGuiLogger::logSetInt) ImGuiLogger::Log("SetInt", nullptr, std::to_string(value).c_str());
            old_SetInt(key, value);
            return;
        }

        if (ImGuiLogger::logSetInt) {
            auto keyStr = MonoStringToUTF8(key);
            ImGuiLogger::Log("SetInt", keyStr.get(), std::to_string(value).c_str());
        }
        old_SetInt(key, value);
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("SetInt_hook: Exception: %s", e.what());
    } catch (...) {
        ImGuiLogger::LogError("SetInt_hook: Unknown exception");
    }
}

typedef void (*SetString_t)(ModMenu::monoString* key, ModMenu::monoString* value);
SetString_t old_SetString;
void SetString_hook(ModMenu::monoString* key, ModMenu::monoString* value) {
    try {
        if (!old_SetString) {
            ImGuiLogger::LogError("SetString_hook: old_SetString is null");
            return;
        }

        if (!key) {
            if (ImGuiLogger::logSetString) {
                auto valueStr = MonoStringToUTF8(value);
                ImGuiLogger::Log("SetString", nullptr, valueStr.get());
            }
            old_SetString(key, value);
            return;
        }

        if (ImGuiLogger::logSetString) {
            auto keyStr = MonoStringToUTF8(key);
            auto valueStr = MonoStringToUTF8(value);
            ImGuiLogger::Log("SetString", keyStr.get(), valueStr.get() ? valueStr.get() : "null");
        }
        old_SetString(key, value);
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("SetString_hook: Exception: %s", e.what());
    } catch (...) {
        ImGuiLogger::LogError("SetString_hook: Unknown exception");
    }
}

typedef ModMenu::monoString* (*GetString_t)(ModMenu::monoString* key, ModMenu::monoString* defaultValue);
GetString_t old_GetString;
ModMenu::monoString* GetString_hook(ModMenu::monoString* key, ModMenu::monoString* defaultValue) {
    try {
        if (!old_GetString) {
            ImGuiLogger::LogError("GetString_hook: old_GetString is null");
            return nullptr;
        }

        if (!key) {
            if (ImGuiLogger::logGetString) {
                auto defaultStr = MonoStringToUTF8(defaultValue);
                ImGuiLogger::Log("GetString", nullptr, nullptr, defaultStr.get());
            }
            return old_GetString(key, defaultValue);
        }

        ModMenu::monoString* result = old_GetString(key, defaultValue);
        if (ImGuiLogger::logGetString) {
            auto keyStr = MonoStringToUTF8(key);
            auto resultStr = MonoStringToUTF8(result);
            auto defaultStr = MonoStringToUTF8(defaultValue);
            ImGuiLogger::Log("GetString", keyStr.get(), resultStr.get() ? resultStr.get() : "null", defaultStr.get() ? defaultStr.get() : "null");
        }
        return result;
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("GetString_hook: Exception: %s", e.what());
        return nullptr;
    } catch (...) {
        ImGuiLogger::LogError("GetString_hook: Unknown exception");
        return nullptr;
    }
}

typedef int (*HasKey_t)(ModMenu::monoString* key);
HasKey_t old_HasKey;
int HasKey_hook(ModMenu::monoString* key) {
    try {
        if (!old_HasKey) {
            ImGuiLogger::LogError("HasKey_hook: old_HasKey is null");
            return 0;
        }

        if (!key) {
            if (ImGuiLogger::logHasKey) ImGuiLogger::Log("HasKey", nullptr, nullptr, nullptr, 0);
            return old_HasKey(key);
        }

        int result = old_HasKey(key);
        if (ImGuiLogger::logHasKey) {
            auto keyStr = MonoStringToUTF8(key);
            ImGuiLogger::Log("HasKey", keyStr.get(), nullptr, nullptr, result);
        }
        return result;
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("HasKey_hook: Exception: %s", e.what());
        return 0;
    } catch (...) {
        ImGuiLogger::LogError("HasKey_hook: Unknown exception");
        return 0;
    }
}

typedef void (*ChatMessage_t)(void* thisPtr, ModMenu::monoString* playerName, ModMenu::monoString* textM, bool admin, bool proffesion, bool MistMoney, bool youtuber);
ChatMessage_t old_ChatMessage;
void ChatMessage_hook(void* thisPtr, ModMenu::monoString* playerName, ModMenu::monoString* textM, bool admin, bool proffesion, bool MistMoney, bool youtuber) {
    if (!old_ChatMessage) {
        ImGuiLogger::LogError("ChatMessage_hook: old_ChatMessage is null");
        return;
    }

    if (playerName) {
        auto nameStr = MonoStringToUTF8(playerName);
        if (nameStr.get()) {
            AddKnownPlayer(nameStr.get());
        }
    }

    if (ImGuiLogger::logChatMessage) {
        auto nameStr = MonoStringToUTF8(playerName);
        auto textStr = MonoStringToUTF8(textM);
        ImGuiLogger::Log("ChatMessage", nameStr.get() ? nameStr.get() : "null", textStr.get() ? textStr.get() : "null");
    }
    old_ChatMessage(thisPtr, playerName, textM, admin, proffesion, MistMoney, youtuber);
}

typedef void (*PushMessageKills_t)(void* thisPtr, ModMenu::monoString* playerNameDead, ModMenu::monoString* playerKiller, int lineHeight, bool suicide);
PushMessageKills_t old_PushMessageKills;
void PushMessageKills_hook(void* thisPtr, ModMenu::monoString* playerNameDead, ModMenu::monoString* playerKiller, int lineHeight, bool suicide) {
    if (!old_PushMessageKills) {
        ImGuiLogger::LogError("PushMessageKills_hook: old_PushMessageKills is null");
        return;
    }

    if (playerNameDead) {
        auto deadStr = MonoStringToUTF8(playerNameDead);
        if (deadStr.get()) {
            AddKnownPlayer(deadStr.get());
        }
    }

    if (playerKiller) {
        auto killerStr = MonoStringToUTF8(playerKiller);
        if (killerStr.get()) {
            AddKnownPlayer(killerStr.get());
        }
    }

    if (ImGuiLogger::logPushMessageKills) {
        auto deadStr = MonoStringToUTF8(playerNameDead);
        auto killerStr = MonoStringToUTF8(playerKiller);
        ImGuiLogger::Log("PushMessageKills", deadStr.get() ? deadStr.get() : "null", killerStr.get() ? killerStr.get() : "null");
    }
    old_PushMessageKills(thisPtr, playerNameDead, playerKiller, lineHeight, suicide);
}

typedef bool (*GetButtonDown_t)(ModMenu::monoString* buttonName);
GetButtonDown_t old_GetButtonDown;
bool GetButtonDown_hook(ModMenu::monoString* buttonName) {
    try {
        if (!old_GetButtonDown) {
            ImGuiLogger::LogError("GetButtonDown_hook: old_GetButtonDown is null");
            return false;
        }

        bool result = old_GetButtonDown(buttonName);
        if (ImGuiLogger::logGetButtonDown) {
            auto buttonStr = MonoStringToUTF8(buttonName);
            ImGuiLogger::Log("GetButtonDown", buttonStr.get() ? buttonStr.get() : "null");
        }
        return result;
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("GetButtonDown_hook: Exception: %s", e.what());
        return false;
    } catch (...) {
        ImGuiLogger::LogError("GetButtonDown_hook: Unknown exception");
        return false;
    }
}

void *set_prefs_thread(void *) {
    sleep(16);
    void (*SetInt)(void *key, int value) = (void (*)(void *, int)) getAbsoluteAddress(
            ewgegggg, string2Offset(OBFUSCATE("0xOFFSET")));


    if (!SetInt) {
        ImGuiLogger::LogError("set_prefs_thread: Failed to get SetInt address");
    }

    return nullptr;
}

void *xT0u1v2(void *) {
    using namespace BNM;

    int retries = 0;
    while (!Il2cppLoaded() && retries < 30) {
        sleep(2);
        retries++;
    }
    if (!Il2cppLoaded()) {
        LOGE("IL2CPP not loaded after 60 seconds");
        return nullptr;
    }

    AttachIl2Cpp();

    void *getInt_target_addr = (void *)getAbsoluteAddress(ewgegggg, string2Offset(OBFUSCATE(GET_INT_OFFSET)));
    if (getInt_target_addr) {
        DobbyHook(getInt_target_addr, (void *)GetInt_hook, (void **)&old_GetInt);
    } else {
        LOGE("Failed to get address for GetInt");
    }

    void *setInt_target_addr = (void *)getAbsoluteAddress(ewgegggg, string2Offset(OBFUSCATE(SET_INT_OFFSET)));
    if (setInt_target_addr) {
        DobbyHook(setInt_target_addr, (void *)SetInt_hook, (void **)&old_SetInt);
    } else {
        LOGE("Failed to get address for SetInt");
    }

    void *setString_target_addr = (void *)getAbsoluteAddress(ewgegggg, string2Offset(OBFUSCATE(SET_STRING_OFFSET)));
    if (setString_target_addr) {
        DobbyHook(setString_target_addr, (void *)SetString_hook, (void **)&old_SetString);
    } else {
        LOGE("Failed to get address for SetString");
    }

    void *getString_target_addr = (void *)getAbsoluteAddress(ewgegggg, string2Offset(OBFUSCATE(GET_STRING_OFFSET)));
    if (getString_target_addr) {
        DobbyHook(getString_target_addr, (void *)GetString_hook, (void **)&old_GetString);
    } else {
        LOGE("Failed to get address for GetString");
    }

    void *hasKey_target_addr = (void *)getAbsoluteAddress(ewgegggg, string2Offset(OBFUSCATE(HAS_KEY_OFFSET)));
    if (hasKey_target_addr) {
        DobbyHook(hasKey_target_addr, (void *)HasKey_hook, (void **)&old_HasKey);
    } else {
        LOGE("Failed to get address for HasKey");
    }

    void *chatMessage_target_addr = (void *)getAbsoluteAddress(ewgegggg, string2Offset(OBFUSCATE(CHAT_MESSAGE_OFFSET)));
    if (chatMessage_target_addr) {
        DobbyHook(chatMessage_target_addr, (void *)ChatMessage_hook, (void **)&old_ChatMessage);
        LOGE("ChatMessage hooked at %p", chatMessage_target_addr);
    } else {
        LOGE("Failed to get address for ChatMessage");
    }

    void *pushMessageKills_target_addr = (void *)getAbsoluteAddress(ewgegggg, string2Offset(OBFUSCATE(PUSH_MESSAGE_KILLS_OFFSET)));
    if (pushMessageKills_target_addr) {
        DobbyHook(pushMessageKills_target_addr, (void *)PushMessageKills_hook, (void **)&old_PushMessageKills);
        LOGE("PushMessageKills hooked at %p", pushMessageKills_target_addr);
    } else {
        LOGE("Failed to get address for PushMessageKills");
    }

    void *get_button_down_target_addr = (void *)getAbsoluteAddress(ewgegggg, string2Offset(OBFUSCATE(GET_BUTTON_DOWN_OFFSET)));
    if (get_button_down_target_addr) {
        DobbyHook(get_button_down_target_addr, (void *)GetButtonDown_hook, (void **)&old_GetButtonDown);
        LOGE("GetButtonDown hooked at %p", get_button_down_target_addr);
    } else {
        LOGE("Failed to get address for GetButtonDown");
    }

    Unity::Screen::Setup();
    if (emulator) {
        Unity::Input::Setup();
    }

    DetachIl2Cpp();

    pthread_t adminThread;
    pthread_create(&adminThread, nullptr, set_prefs_thread, nullptr);

    return nullptr;
}

void xU1v2w3() {
    try {
        ImGui::Begin(OBFUSCATE("Mod Menu"));

        ImGui::Text("Logging Controls:");

        ImGui::Text("ObscuredPrefs:");
        ImGui::Checkbox("GetInt", &ImGuiLogger::logGetInt);
        ImGui::SameLine();
        ImGui::Checkbox("SetInt", &ImGuiLogger::logSetInt);
        ImGui::SameLine();
        ImGui::Checkbox("SetString", &ImGuiLogger::logSetString);
        ImGui::Checkbox("GetString", &ImGuiLogger::logGetString);
        ImGui::SameLine();
        ImGui::Checkbox("HasKey", &ImGuiLogger::logHasKey);

        ImGui::Text("Chat:");

        ImGui::Separator();

        ImGui::Columns(2, "MainColumns", true);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.7f);
        ImGui::SetColumnWidth(1, ImGui::GetWindowWidth() * 0.3f);

        ImGui::Text("Log Controls:");
        ImGui::Checkbox("Simplified Log Format", &ImGuiLogger::simplifiedLog);

        if (ImGui::Button("Filter Value = 1")) {
            ImGuiLogger::filterMode = ImGuiLogger::FilterMode::Value_1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Filter Value = 0")) {
            ImGuiLogger::filterMode = ImGuiLogger::FilterMode::Value_0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Filter")) {
            ImGuiLogger::filterMode = ImGuiLogger::FilterMode::None;
        }

        if (ImGui::Button("Clear Logs")) {
            std::lock_guard<std::mutex> lock(ImGuiLogger::logMutex);
            ImGuiLogger::logBuffer.clear();
            ImGuiLogger::knownKeys.clear();
        }

        ImGui::Separator();

        if (ImGui::BeginTabBar("LogTabs")) {
            if (ImGui::BeginTabItem("Main Logs")) {
                ImGui::Text("Main Log Output:");
                ImGui::BeginChild("LogWindow", ImVec2(0, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
                {
                    std::lock_guard<std::mutex> lock(ImGuiLogger::logMutex);
                    for (size_t i = 0; i < ImGuiLogger::logBuffer.size(); ++i) {
                        const auto& entry = ImGuiLogger::logBuffer[i];

                        bool shouldDisplay = true;
                        if (ImGuiLogger::filterMode != ImGuiLogger::FilterMode::None) {
                            bool matchesFilter = false;
                            if (entry.function == "HasKey") {
                                int value = entry.hasKeyResult;
                                if (ImGuiLogger::filterMode == ImGuiLogger::FilterMode::Value_1 && value == 1) {
                                    matchesFilter = true;
                                } else if (ImGuiLogger::filterMode == ImGuiLogger::FilterMode::Value_0 && value == 0) {
                                    matchesFilter = true;
                                }
                            }
                            if (!entry.value.empty()) {
                                try {
                                    int value = std::stoi(entry.value);
                                    if (ImGuiLogger::filterMode == ImGuiLogger::FilterMode::Value_1 && value == 1) {
                                        matchesFilter = true;
                                    } else if (ImGuiLogger::filterMode == ImGuiLogger::FilterMode::Value_0 && value == 0) {
                                        matchesFilter = true;
                                    }
                                } catch (...) {}
                            }
                            if (!entry.defaultValue.empty()) {
                                try {
                                    int value = std::stoi(entry.defaultValue);
                                    if (ImGuiLogger::filterMode == ImGuiLogger::FilterMode::Value_1 && value == 1) {
                                        matchesFilter = true;
                                    } else if (ImGuiLogger::filterMode == ImGuiLogger::FilterMode::Value_0 && value == 0) {
                                        matchesFilter = true;
                                    }
                                } catch (...) {}
                            }
                            shouldDisplay = matchesFilter;
                        }

                        if (shouldDisplay) {
                            std::string formatted = ImGuiLogger::FormatLog(entry);
                            ImGui::TextUnformatted(formatted.c_str());
                        }
                    }
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                        ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::NextColumn();

        ImGui::Text("Generate Hook File:");
        static char filePath[256] = "/sdcard/hooks.txt";
        static int defaultValue = 1;
        static std::string statusMessage = "";
        ImGui::InputText("File Path", filePath, sizeof(filePath));
        ImGui::InputInt("Default Value", &defaultValue);
        if (ImGui::Button("Generate Hooks")) {
            std::lock_guard<std::mutex> lock(ImGuiLogger::logMutex);
            std::ofstream outFile(filePath);
            if (outFile.is_open()) {
                outFile << "// Generated hooks for ObscuredPrefs.SetInt\n";
                outFile << "void *set_prefs_thread(void *) {\n";
                outFile << "    sleep(16);\n";
                outFile << "    void (*SetInt)(void *key, int value) = (void (*)(void *, int)) getAbsoluteAddress(\n";
                outFile << "            ewgegggg, string2Offset(OBFUSCATE(\"0xOFFSET\")));\n\n";
                outFile << "    if (SetInt) {\n";
                for (const auto& key : ImGuiLogger::knownKeys) {
                    std::string varName = key;
                    std::replace_if(varName.begin(), varName.end(),
                                    [](char c) { return !std::isalnum(c) && c != '_'; }, '_');
                    if (!varName.empty() && std::isdigit(varName[0])) {
                        varName = "key_" + varName;
                    }
                    outFile << "        monoString *" << varName << "Key = CreateCustomMonoString(OBFUSCATE(\"" << key << "\"));\n";
                    outFile << "        if (" << varName << "Key) {\n";
                    outFile << "            SetInt(" << varName << "Key, " << defaultValue << ");\n";
                    outFile << "        }\n\n";
                }
                outFile << "    }\n\n";
                outFile << "    return nullptr;\n";
                outFile << "}\n";
                outFile.close();
                statusMessage = "File saved successfully to " + std::string(filePath);
                LOGE("Hooks written to %s", filePath);
            } else {
                statusMessage = "Failed to write file to " + std::string(filePath);
                LOGE("Failed to write hooks to %s", filePath);
            }
        }

        ImGui::Separator();

        ImGui::Text("Status:");
        ImGui::TextWrapped("%s", statusMessage.c_str());

        ImGui::Columns(1);

        ImGui::End();
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("xU1v2w3: Exception: %s", e.what());
    } catch (...) {
        ImGuiLogger::LogError("xU1v2w3: Unknown exception");
    }
}

void xV2w3x4() {
    try {
        if (main_render_thread == 0) {
            main_render_thread = pthread_self();
            LOGE("Main render thread set to %lu", (unsigned long)main_render_thread);
        } else if (pthread_self() != main_render_thread) {
            LOGE("xV2w3x4: Called from wrong thread: %lu, expected: %lu", (unsigned long)pthread_self(), (unsigned long)main_render_thread);
            return;
        }

        if (init && Unity::Screen::is_done) {
            ImGuiIO &io = ImGui::GetIO();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplAndroid_NewFrame(Unity::Screen::Width.get(), Unity::Screen::Height.get());
            ImGui::NewFrame();
            xU1v2w3();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            ImGui::EndFrame();
        }
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("xV2w3x4: Exception: %s", e.what());
    } catch (...) {
        ImGuiLogger::LogError("xV2w3x4: Unknown exception");
    }
}

EGLBoolean (*old_xW3x4y5)(EGLDisplay, EGLSurface);
EGLBoolean xW3x4y5(EGLDisplay _display, EGLSurface _surface) {
    try {
        if (!imgui_initialized) {
            SetupImGui();
            imgui_initialized = true;
            LOGE("ImGui initialized in xW3x4y5");
        }
        xV2w3x4();
        if (!old_xW3x4y5) {
            ImGuiLogger::LogError("xW3x4y5: old_xW3x4y5 is null");
            return EGL_FALSE;
        }
        return old_xW3x4y5(_display, _surface);
    } catch (const std::exception& e) {
        ImGuiLogger::LogError("xW3x4y5: Exception: %s", e.what());
        return EGL_FALSE;
    } catch (...) {
        ImGuiLogger::LogError("xW3x4y5: Unknown exception");
        return EGL_FALSE;
    }
}

__attribute__((constructor))
void xX4y5z6() {
    auto eglhandle = dlopen(OBFUSCATE("libEGL.so"), RTLD_LAZY);
    if (!eglhandle) {
        LOGE("Failed to load libEGL.so");
        return;
    }

    auto eglSwapBuffers = dlsym(eglhandle, OBFUSCATE("eglSwapBuffers"));
    if (!eglSwapBuffers) {
        LOGE("Failed to find eglSwapBuffers");
        dlclose(eglhandle);
        return;
    }

    hook(eglSwapBuffers, (void *) xW3x4y5, (void **) &old_xW3x4y5);
    if (!old_xW3x4y5) {
        LOGE("Failed to hook eglSwapBuffers");
        dlclose(eglhandle);
        return;
    }

    pthread_t ptid;
    pthread_create(&ptid, NULL, xT0u1v2, NULL);
}

#pragma clang diagnostic pop
