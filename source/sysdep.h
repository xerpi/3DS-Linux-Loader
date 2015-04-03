#define SYS_MODEL_NONE    0
#define SYS_MODEL_OLD_3DS 1
#define SYS_MODEL_NEW_3DS 2

#define PA_EXC_HANDLER_BASE        0x1FFF4000
#define VA_KERNEL_VERSION_REGISTER 0x1FF80000
#define OFFS_FCRAM_ARM9_PAYLOAD    0x03F00000
#define OFFS_EXC_HANDLER_UNUSED    0xC80

typedef struct exploit_data {

	u32 kernel_version;
	u32 sys_model; // mask
		
	u32 va_patch_createthread;
	u32 va_patch_svc_handler;
	u32 va_patch_hook1;
	u32 va_patch_hook2;
	u32 va_hook1_ret;
				
	u32 va_fcram_base;
	u32 va_exc_handler_base_W;
	u32 va_exc_handler_base_X;
	u32 va_kernelsetstate;
	
	u32 va_pdn_regs;
	u32 va_pxi_regs;
};

// add all vulnerable systems below
struct exploit_data supported_systems[] = {
	{
		0x022E0000,        // kernel version
		SYS_MODEL_NEW_3DS, // model
		0xDFF83837,        // VA of CreateThread code to corrupt
		0xDFF82260,        // VA of ARM11 Kernel SVC handler priv check
		0xDFFE7A50,        // VA of 1st hook for firmlaunch
		0xDFFF4994,        // VA of 2nd hook for firmlaunch
		0xFFF28A58,        // VA of return address from 1st hook 
		0xE0000000,        // VA of FCRAM
		0xDFFF4000,        // VA of lower mapped exception handler base
		0xFFFF0000,        // VA of upper mapped exception handler base
		0xFFF158F8,        // VA of the KernelSetState syscall (upper mirror)
		0xFFFBE000,        // VA PDN registers
		0xFFFC0000         // VA PXI registers		
	},
	{
		0x022C0600,        // kernel version
		SYS_MODEL_NEW_3DS, // model
		0xDFF83837,        // VA of CreateThread code to corrupt
		0xDFF82260,        // VA of ARM11 Kernel SVC handler priv check
		0xDFFE7A50,        // VA of 1st hook for firmlaunch
		0xDFFF4994,        // VA of 2nd hook for firmlaunch
		0xFFF28A58,        // VA of return address from 1st hook 
		0xE0000000,        // VA of FCRAM
		0xDFFF4000,        // VA of lower mapped exception handler base
		0xFFFF0000,        // VA of upper mapped exception handler base
		0xFFF158F8,        // VA of the KernelSetState syscall (upper mirror)
		0xFFFBE000,        // VA PDN registers
		0xFFFC0000         // VA PXI registers		
	}
};