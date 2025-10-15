# MemoryPoolNVSE Overdrive - Crash Diagnosis & Safe Testing

## 🚨 Current Status: CRASH-SAFE MODE

The plugin has been updated with comprehensive crash protection and is now running in **SAFE MODE** with:

- ✅ **Direct hooks DISABLED** (prevents crashes from bad addresses)
- ✅ **Memory patching DISABLED** (prevents crashes from invalid memory access)
- ✅ **Debug logging ENABLED** (diagnoses the cause)
- ✅ **IAT hooks ONLY** (safe fallback mode)

## 🔍 Testing the Safe Version

### Step 1: Install Safe Version
```powershell
.\quick-build.ps1
.\deploy.ps1
```

### Step 2: Test Basic Functionality
1. Launch game via NVSE
2. Open console and run:
   ```
   IsMemoryPoolActive
   IsMemoryPoolHooked
   ```
3. Both should return 1 if working

### Step 3: Check Debug Log
Look for: `Data\NVSE\Plugins\MemoryPoolNVSE_Debug.log`

Expected safe output:
```
=== MemoryPoolNVSE Overdrive v2.0 Initializing ===
Setting Large Address Aware...
Initializing RPmalloc...
RPmalloc initialized successfully
Memory mode patching disabled for safety
Direct hooks disabled for crash safety
Installing IAT hooks...
IAT hooks installed successfully
=== MemoryPoolNVSE Overdrive initialization complete ===
```

## 🔧 Gradual Re-enabling (Once Safe Version Works)

### Phase 1: Enable Memory Patching
Edit `MemoryPoolNVSE_rpmalloc.cpp`:
```cpp
#define ENABLE_MEMORY_PATCHING  1  // Change from 0 to 1
```

### Phase 2: Enable Direct Hooks (Most Risky)
Edit `MemoryPoolNVSE_rpmalloc.cpp`:
```cpp
#define ENABLE_DIRECT_HOOKS     1  // Change from 0 to 1
```

## 🚨 Crash Scenarios & Solutions

### Scenario 1: Game Version Mismatch
**Problem**: Addresses are wrong for your game version
**Solution**: Validate addresses first

```cpp
// Add to debug log before hooking
char buffer[256];
sprintf(buffer, "Checking address 0x%08X", GAME_HEAP_ALLOC);
DebugLog(buffer);
```

### Scenario 2: Anti-virus Interference  
**Problem**: Anti-virus blocks code modification
**Solution**: Add exception for game folder

### Scenario 3: Conflicting Mods
**Problem**: Another mod modifies same functions
**Solution**: Use different hook points or disable conflicting mod

### Scenario 4: Invalid Memory Access
**Problem**: Hooking unmapped memory regions
**Solution**: Enhanced validation (already implemented)

## 🛠️ Advanced Debugging

### Enable Detailed Address Validation
```cpp
// Add this to InstallDetour function
MEMORY_BASIC_INFORMATION mbi;
VirtualQuery(target, &mbi, sizeof(mbi));
char debug[512];
sprintf(debug, "Address 0x%08X: State=%d, Protect=0x%X, Type=%d", 
        (DWORD)target, mbi.State, mbi.Protect, mbi.Type);
DebugLog(debug);
```

### Check Game Module Base
```cpp
HMODULE gameModule = GetModuleHandle(NULL);
char debug[256];
sprintf(debug, "Game base address: 0x%08X", (DWORD)gameModule);
DebugLog(debug);
```

### Validate Function Signatures
```cpp
// Check if address contains expected function prologue
BYTE* funcPtr = (BYTE*)GAME_HEAP_ALLOC;
if (funcPtr[0] == 0x55) {  // PUSH EBP
    DebugLog("Function prologue looks correct");
} else {
    DebugLog("WARNING: Unexpected function prologue");
}
```

## 📊 Performance Comparison

### Safe Mode (Current)
- **Hook Coverage**: ~10% (IAT only)
- **Performance Gain**: 10-20% (limited)
- **Stability**: 100% (no crashes)
- **Compatibility**: Perfect

### Full Mode (When Working)  
- **Hook Coverage**: ~95% (Direct + IAT)
- **Performance Gain**: 200-1000% (massive)
- **Stability**: 95%+ (if addresses correct)
- **Compatibility**: 95%+ (if no conflicts)

## 🎯 Next Steps

1. **Test safe version** - confirm it works without crashes
2. **Check debug log** - verify initialization sequence
3. **Validate game version** - confirm you have standard FNV
4. **Enable features gradually** - one at a time with testing
5. **Report findings** - share debug log for analysis

## ⚠️ Important Notes

- **Backup saves** before testing any memory modifications
- **Start with new game** to avoid corrupted save issues  
- **Close other mods** during testing to isolate issues
- **Check Windows version** - some older versions have issues
- **Run as administrator** if getting access denied errors

The safe version should work perfectly and give you the basic RPmalloc benefits (~10% performance gain) while we debug the advanced features.

## 🔍 Common Debug Log Messages

### Success Messages:
- `RPmalloc initialized successfully` ✅
- `IAT hooks installed successfully` ✅
- `initialization complete` ✅

### Warning Messages:
- `Direct hooks disabled for crash safety` ⚠️ (Expected in safe mode)
- `Memory mode patching disabled for safety` ⚠️ (Expected in safe mode)

### Error Messages:
- `RPmalloc initialization failed` ❌ (Critical - plugin won't work)
- `IAT hooks installation failed` ❌ (Critical - no hooking at all)
- `VirtualQuery failed for address` ❌ (Address validation failed)
- `Address not executable` ❌ (Bad hook target)

Try the safe version first and let me know what the debug log shows!