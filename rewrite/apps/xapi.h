#ifndef OPENICESTAR_XAPI_H
#define OPENICESTAR_XAPI_H
#include <stdint.h>

#define U
#define XAPI(name, vector, slot, parameters) uint32_t x_##name parameters;
#include "xapi.def"
#undef XAPI
#undef U
#endif
