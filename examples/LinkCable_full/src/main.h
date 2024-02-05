#ifndef MAIN_H
#define MAIN_H

#include <tonc.h>
#include "../../../lib/LinkCable.hpp"
#include "../../../lib/LinkUniversal.hpp"

// #define USE_LINK_UNIVERSAL

#ifndef USE_LINK_UNIVERSAL
extern LinkCable* linkConnection;
#endif
#ifdef USE_LINK_UNIVERSAL
extern LinkUniversal* linkConnection;
#endif

#endif  // MAIN_H
