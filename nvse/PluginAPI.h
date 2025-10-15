#pragma once
#include <windows.h>

// Basic types (defined first)
typedef unsigned char UInt8;
typedef unsigned short UInt16;
typedef unsigned int UInt32;
typedef unsigned long long UInt64;

// Minimal NVSE Plugin API definitions for HeapMaster
#define NVSE_VERSION_INTEGER 4

// Runtime version constants
#define RUNTIME_VERSION_1_4_0_525 0x0104021D

// Plugin info structure
struct PluginInfo {
    enum {
        kInfoVersion = 1
    };
    UInt32 infoVersion;
    const char* name;
    UInt32 version;
};

// NVSE interface
struct NVSEInterface {
    UInt32 nvseVersion;
    UInt32 runtimeVersion;
    bool isEditor;
    
    void* (*QueryInterface)(UInt32 id);
    UInt32 (*GetPluginHandle)(void);
};

// Message interface
struct NVSEMessagingInterface {
    enum {
        kInterface_Messaging = 3
    };
    
    enum {
        kMessage_PostLoad = 0,
        kMessage_PostPostLoad = 1,
        kMessage_PreLoadGame = 2,
        kMessage_PostLoadGame = 3,
        kMessage_SaveGame = 4,
        kMessage_DeleteGame = 5,
        kMessage_RenameGame = 6,
        kMessage_RenameNewGame = 7,
        kMessage_NewGame = 8,
        kMessage_LoadGame = 9,
        kMessage_ExitGame = 10,
        kMessage_ExitToMainMenu = 11,
        kMessage_Precompile = 12,
        kMessage_RuntimeScriptError = 13
    };
    
    struct Message {
        const char* sender;
        UInt32 type;
        UInt32 dataLen;
        void* data;
    };
    
    bool (*RegisterListener)(UInt32 pluginHandle, const char* sender, void* handler);
};

// Plugin interface constants
static const UInt32 kInterface_Messaging = 3;
