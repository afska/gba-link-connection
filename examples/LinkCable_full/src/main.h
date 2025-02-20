#ifndef MAIN_H
#define MAIN_H

// #define USE_LINK_UNIVERSAL

#define LINK_CABLE_DEBUG_MODE
#define LINK_WIRELESS_DEBUG_MODE

#ifndef USE_LINK_UNIVERSAL
#include "../../../lib/LinkCable.hpp"
#else
#include "../../../lib/LinkUniversal.hpp"
#endif

#include "../../_lib/common.h"
#include "../../_lib/libgba-sprite-engine/scene.h"

#ifndef USE_LINK_UNIVERSAL
extern LinkCable* linkConnection;
#else
extern LinkUniversal* linkConnection;
#endif

#endif  // MAIN_H
