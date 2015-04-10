#pragma once

#include "exploitdata.h"

int load_arm9_payload(char *filename);
int run_exploit(int svc_patch);
void redirect_codeflow(u32 *dst_addr, u32 *src_addr);
int do_gshax_copy(void *dst, void *src, unsigned int len);
void priv_write_four(u32 address);
void user_clear_icache();
int corrupt_arm11_kernel_code(void);
int map_arm9_payload(void);
int map_arm11_payload(void);
void exploit_arm9_race_condition();
void apply_patches (bool heal_svc_handler);
int get_exploit_data(struct exploit_data *data);

/* any changes to this structure must also be applied to
   the data structure following the 'arm11_globals_start'
   label of arm11.s */
typedef struct arm11_shared_data {
	u32 va_pdn_regs;
	u32 va_pxi_regs;
	u32 va_hook1_ret;
};

#define BRAHMA_NETWORK_PORT 80

#define ARM_JUMPOUT 0xE51FF004 // LDR PC, [PC, -#04]
#define ARM_RET     0xE12FFF1E // BX LR
#define ARM_NOP     0xE1A00000 // NOP

#define ARM9_PAYLOAD_MAX_SIZE 1024 * 1000