#ifndef _STATE_IN_HPP
#define _STATE_IN_HPP

#define SFVARN_BOOL(x, n) { &(x), 1, PX68KSTATE_RLSB | PX68KSTATE_BOOL, n }
#define SFVARN(x, n) { &(x), (uint32_t)sizeof(x), PX68KSTATE_RLSB, n }
#define SFVAR(x) SFVARN((x), #x)

#define SFARRAYN(x, l, n) { (x), (uint32_t)(l), 0, n }
#define SFARRAY(x, l) SFARRAYN((x), (l), #x)

#define SFARRAY16N(x, l, n) { (x), (uint32_t)((l) * sizeof(uint16_t)), PX68KSTATE_RLSB16, n }
#define SFARRAY16(x, l) SFARRAY16N((x), (l), #x)

#define SFARRAY32N(x, l, n) { (x), (uint32_t)((l) * sizeof(uint32_t)), PX68KSTATE_RLSB32, n }
#define SFARRAY32(x, l) SFARRAY32N((x), (l), #x)

#define SFARRAY64N(x, l, n) { (x), (uint32_t)((l) * sizeof(uint64_t)), PX68KSTATE_RLSB64, n }
#define SFARRAY64(x, l) SFARRAY64N((x), (l), #x)

#define SFEND { 0, 0, 0, 0 }

#endif
