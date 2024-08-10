#ifndef LINK_TONC_MGBA
#define LINK_TONC_MGBA

#include <stdarg.h>
#include <stdio.h>
#include "tonc_types.h"

// --------------------------------------------------------------------
// STRUCTS
// --------------------------------------------------------------------

typedef enum LogLevel {
  LOG_FATAL = 0x100,
  LOG_ERR = 0x101,
  LOG_WARN = 0x102,
  LOG_INFO = 0x103
} LogLevel;

// --------------------------------------------------------------------
// MACROS
// --------------------------------------------------------------------

#define REG_LOG_ENABLE *(vu16*)0x4FFF780
#define REG_LOG_LEVEL *(vu16*)0x4FFF700

// --------------------------------------------------------------------
// INLINES
// --------------------------------------------------------------------

static inline void mgbalog(const char* fmt, ...) {
  REG_LOG_ENABLE = 0xC0DE;

  va_list args;
  va_start(args, fmt);

  char* const log = (char*)0x4FFF600;
  vsnprintf(log, 0x100, fmt, args);
  REG_LOG_LEVEL = LogLevel::LOG_WARN;

  va_end(args);
}

#endif  // LINK_TONC_MGBA

// EOF