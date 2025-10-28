#pragma once

// Wrapper that prefers the official xNVSE/NVSE PluginAPI if available at build time.
// Falls back to our minimal header to remain self-contained.

// If you have the official xNVSE SDK headers, define USE_OFFICIAL_NVSE and
// ensure the include path contains nvse/PluginAPI.h. Otherwise we use a
// compatibility header that matches xNVSE's ABI for our needs.
#ifdef USE_OFFICIAL_NVSE
  #include "nvse/PluginAPI.h"
  #define RPNVSE_OVERDRIVE_NVSE_OFFICIAL 1
#else
  #include "nvse_minimal.h"
#endif

// Ensure messaging interface ID is available
#ifndef kInterface_Messaging
  #define kInterface_Messaging 3
#endif
