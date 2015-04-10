#pragma once

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

Result SOCInit();
Result SOCExit();

static u32 *SOC_buffer = 0;