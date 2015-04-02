int run_exploit();

typedef struct arm11_shared_data {
	u32 va_pdn_regs;
	u32 va_pxi_regs;
	u32 va_hook1_ret;
};

#define wait() svcSleepThread(1000000000ull)

#define PA_EXC_HANDLER_BASE        0x1FFF4000
#define VA_KERNEL_VERSION_REGISTER 0x1FF80000
#define OFFS_FCRAM_ARM9_PAYLOAD    0x03F00000
#define OFFS_EXC_HANDLER_UNUSED    0xC80

#define ARM_JUMPOUT 0xE51FF004 // LDR PC, [PC, -#04]
#define ARM_RET     0xE12FFF1E // BX LR
#define ARM_NOP     0xE1A00000 // NOP
