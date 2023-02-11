#ifndef MAIN_H
#define MAIN_H

#include <tonc.h>
#include "../../../lib/LinkCable.h"
#include "../../../lib/LinkUniversal.h"

// #define USE_LINK_UNIVERSAL

#ifndef USE_LINK_UNIVERSAL
extern LinkCable* link;
#endif
#ifdef USE_LINK_UNIVERSAL
extern LinkUniversal* link;
#endif

#endif  // MAIN_H
