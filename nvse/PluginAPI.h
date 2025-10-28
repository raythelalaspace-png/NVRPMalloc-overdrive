#pragma once
#include <windows.h>
#include <stdint.h>

typedef unsigned char  UInt8;
typedef unsigned short UInt16;
typedef unsigned int   UInt32;
typedef unsigned long long UInt64;
typedef UInt32 PluginHandle;

// Runtime version constants (FNV)
#ifndef RUNTIME_VERSION_1_4_0_525
#define RUNTIME_VERSION_1_4_0_525 0x0104021D
#endif

// Plugin info structure
struct PluginInfo {
    enum { kInfoVersion = 1 };
    UInt32 infoVersion;
    const char* name;
    UInt32 version;
};

// Forward decls
struct CommandInfo;

// NVSE core interface (subset sufficient for most plugins)
struct NVSEInterface {
    UInt32 nvseVersion;
    UInt32 runtimeVersion;
    UInt32 editorVersion;
    UInt32 isEditor;

    bool (*RegisterCommand)(CommandInfo* info);
    void (*SetOpcodeBase)(UInt32 opcode);
    void* (*QueryInterface)(UInt32 id);
    PluginHandle (*GetPluginHandle)(void);
    bool (*RegisterTypedCommand)(CommandInfo* info, UInt8 retnType);
    const char* (*GetRuntimeDirectory)();
    UInt32 isNogore;
    void (*InitExpressionEvaluatorUtils)(void* utils);
    bool (*RegisterTypedCommandVersion)(CommandInfo* info, UInt8 retnType, UInt32 requiredPluginVersion);
};

// Message interface
struct NVSEMessagingInterface {
    enum { kInterface_Messaging = 3 };

    // Message types (aligned with xNVSE)
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

    struct Message {
        const char* sender;
        UInt32 type;
        UInt32 dataLen;
        void* data;
    };

    typedef void (*EventCallback)(Message* msg);

    UInt32 version;
    bool (*RegisterListener)(PluginHandle listener, const char* sender, EventCallback handler);
    bool (*Dispatch)(PluginHandle sender, UInt32 messageType, void* data, UInt32 dataLen, const char* receiver);
};

// Command info (decl only; not used directly here)
struct CommandInfo {
    const char* longName;
    const char* shortName;
    UInt32 opcode;
    const char* helpText;
    UInt32 needsParent;
    UInt32 numParams;
    void* params;
    bool (*execute)(UInt32, void*, void*, void*, void*, void*, void*, void*, double*);
};
