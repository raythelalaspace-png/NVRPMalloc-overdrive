#pragma once
#include <stdarg.h>

// Exposed logging function implemented in OverdrivePlugin.cpp
#ifdef __cplusplus
extern "C" {
#endif
void LogWrite(int level, const char* file, int line, const char* fmt, ...);
#ifdef __cplusplus
}
#endif

#ifndef LOG
  #define LOG(level, ...) LogWrite(level, __FILE__, __LINE__, __VA_ARGS__)
  #define LOG_TRACE(...)  LOG(0, __VA_ARGS__)
  #define LOG_DEBUG(...)  LOG(1, __VA_ARGS__)
  #define LOG_INFO(...)   LOG(2, __VA_ARGS__)
  #define LOG_WARN(...)   LOG(3, __VA_ARGS__)
  #define LOG_ERROR(...)  LOG(4, __VA_ARGS__)
  #define LOG_FATAL(...)  LOG(5, __VA_ARGS__)
#endif

#ifndef LOGI
  #define LOGI(...) LOG_INFO(__VA_ARGS__)
#endif
#ifndef LOGW
  #define LOGW(...) LOG_WARN(__VA_ARGS__)
#endif
