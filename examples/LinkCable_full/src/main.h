#ifndef MAIN_H
#define MAIN_H

// #define USE_LINK_UNIVERSAL

#ifndef USE_LINK_UNIVERSAL
#include "../../../lib/LinkCable.hpp"
#else
#include "../../../lib/LinkUniversal.hpp"
#endif

#include <tonc.h>

#ifndef USE_LINK_UNIVERSAL
extern LinkCable* linkConnection;
#else
extern LinkUniversal* linkConnection;
#endif

#endif  // MAIN_H
