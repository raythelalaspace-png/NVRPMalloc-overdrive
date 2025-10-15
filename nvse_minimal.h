#pragma once

#include <Windows.h>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <cstring>

// Minimal NVSE plugin interface definitions
typedef unsigned long UInt32;
typedef unsigned char UInt8;
typedef UInt32 PluginHandle;

enum {
    kPluginHandle_Invalid = 0xFFFFFFFF,
};

enum {
    kInterface_Messaging = 2,
};

// Version definitions
#define PACKED_NVSE_VERSION 0x06010000  // NVSE 6.1.0
#define RUNTIME_VERSION_1_4_0_525 0x040000525

struct PluginInfo {
    enum {
        kInfoVersion = 1
    };
    UInt32 infoVersion;
    const char* name;
    UInt32 version;
};

struct NVSEMessagingInterface {
    struct Message {
        const char* sender;
        UInt32 type;
        UInt32 dataLen;
        void* data;
    };

    enum {
        kVersion = 6
    };

    // Message types
    enum {
        kMessage_PostLoad = 0,
        kMessage_ExitGame = 1,
        kMessage_ExitToMainMenu = 2,
        kMessage_LoadGame = 3,
        kMessage_SaveGame = 4,
        kMessage_PreLoadGame = 5,
        kMessage_ExitGame_Console = 6,
        kMessage_PostLoadGame = 7,
        kMessage_PostPostLoad = 8,
        kMessage_RuntimeScriptError = 9,
        kMessage_DeleteGame = 10,
        kMessage_RenameGame = 11,
        kMessage_RenameNewGame = 12,
        kMessage_NewGame = 13,
        kMessage_DeleteGameName = 14,
        kMessage_RenameGameName = 15,
        kMessage_RenameNewGameName = 16,
        kMessage_DeferredInit = 17,
        kMessage_ClearScriptDataCache = 18,
        kMessage_MainGameLoop = 19,
        kMessage_ScriptCompile = 20,
        kMessage_EventListDestroyed = 21,
        kMessage_PostQueryPlugins = 22
    };

    UInt32 version;
    bool (*RegisterListener)(PluginHandle listener, const char* sender, void* handler);
    bool (*Dispatch)(PluginHandle sender, UInt32 messageType, void* data, UInt32 dataLen, const char* receiver);
};

struct NVSEInterface {
    UInt32 nvseVersion;
    UInt32 runtimeVersion;
    UInt32 editorVersion;
    UInt32 isEditor;
    bool (*RegisterCommand)(void* info);
    void (*SetOpcodeBase)(UInt32 opcode);
    void* (*QueryInterface)(UInt32 id);
    PluginHandle (*GetPluginHandle)(void);
    bool (*RegisterTypedCommand)(void* info, UInt8 retnType);
    const char* (*GetRuntimeDirectory)();
    UInt32 isNogore;
    void (*InitExpressionEvaluatorUtils)(void* utils);
    bool (*RegisterTypedCommandVersion)(void* info, UInt8 retnType, UInt32 requiredPluginVersion);
};

// Function declarations that NVSE expects to find
extern "C" {
    bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info);
    bool NVSEPlugin_Load(NVSEInterface* nvse);
}

// Logging class - simplified version
class SimpleLog {
public:
    SimpleLog(const char* filename) : logName(filename) {}
    
    void Log(const char* fmt, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        
        // Get current executable directory
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        char* lastSlash = strrchr(exePath, '\\');
        if (lastSlash) *lastSlash = '\0';
        
        char logPath[MAX_PATH];
        sprintf_s(logPath, "%s\\%s", exePath, logName);
        
        FILE* logFile;
        if (fopen_s(&logFile, logPath, "a") == 0 && logFile) {
            time_t now = time(nullptr);
            struct tm timeinfo;
            localtime_s(&timeinfo, &now);
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
            
            fprintf(logFile, "[%s] %s\n", timestamp, buffer);
            fclose(logFile);
        }
    }

private:
    const char* logName;
};