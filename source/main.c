#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "brahma.h"
#include "hid.h"
#include "menus.h"
#include "sochlp.h"

void interact_with_user() {
	int menuidx = 0;
	
	while (aptMainLoop()) {
		gspWaitForVBlank();

		menuidx = print_main_menu(menuidx, &g_main_menu);		

		u32 kDown = wait_key(); 
		
		if (kDown & KEY_B) {
			break;
		}
		else if (kDown & KEY_A) {
			consoleClear();
			printf("\n");
			if (menu_execute_function(menuidx, &g_main_menu, 0))
				wait_any_key();
		}
		else if (kDown & KEY_UP) {
			menuidx--;
		}
		else if (kDown & KEY_DOWN) {
			menuidx++;
		}
		gfxFlushBuffers();
		gfxSwapBuffers(); 
	}

	return 0;
}

int quick_boot_firm(int load_from_disk) {
	if (load_from_disk)
		load_arm9_payload("/arm9payload.bin");
	run_exploit(false);	
}

int main() {
	// Initialize services
	srvInit();
	aptInit();
	hidInit(NULL);
	gfxInitDefault();
	fsInit();
	sdmcInit();
	hbInit();
	qtmInit();
	
	consoleInit(GFX_BOTTOM, NULL);

	hidScanInput();
	u32 kHeld = hidKeysHeld();
 
	if (kHeld & KEY_LEFT) {
		/* load default payload from dosk and run exploit */
		quick_boot_firm(1);
		printf("[!] Quickload failed\n");
		wait_any_key();
	} else if (kHeld & KEY_RIGHT) {
		/* reboot only */
		quick_boot_firm(0);
	}

	SOCInit();	
	interact_with_user();

	hbExit();
	sdmcExit();
	fsExit();
	gfxExit();
	hidExit();
	aptExit();
	srvExit();
	SOCExit();

	// Return to hbmenu
	return 0;
}