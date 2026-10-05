#pragma once

#if defined(CH582)
#include "CH58x_common.h"
#elif defined(CH573)
#include "CH57x_common.h"

#ifndef __HIGH_CODE
#define __HIGH_CODE   __attribute__((section(".highcode")))
#endif

#ifndef __INTERRUPT
#define __INTERRUPT   __attribute__((interrupt()))
//#define __INTERRUPT   __attribute__((interrupt("WCH-Interrupt-fast")))
#endif

#endif
