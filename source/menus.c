#include <dirent.h>
#include <3ds.h>
#include "menus.h"

int print_menu (int idx, struct menu_t *menu) {
	int i;
	int newidx;
	int count = menu_get_element_count(menu);
	
	newidx = menu_update_index(idx, menu);

	for (i=0; i<count; i++) {
		if (newidx == i)
			printf("[ %s ]\n", menu_get_element_name(i, menu));
		else
			printf("  %s  \n", menu_get_element_name(i, menu));
	}
	
	return newidx;	
}

int print_file_list (int idx, struct menu_t *menu) {	
	int i = 0;
	int newidx;
   	DIR *dp;
    struct dirent *entry;
	char *filename = 0;
	int totalfiles = 0;
	int num_printed = 0;

	consoleClear();

    printf("ARM9 payloads (%s):\n\n\n", BRAHMADIR);
	printf("===========================\n");

	int count = menu_get_element_count(menu);
	
	newidx = menu_update_index(idx, menu);
    if((dp = opendir(BRAHMADIR))) {   	
		for (i=0; i<count; i++) {
			if ((entry = readdir(dp)) != 0) {
				filename = entry->d_name;
			}
			else {
				filename = "---";
			}
			if (newidx == i)
				printf("[ %s ] %s\n", menu_get_element_name(i, menu), filename);
			else
				printf("  %s   %s\n", menu_get_element_name(i, menu), filename);
		}
    	closedir(dp);
    }
    else {
    	printf("[!] Could not open '%s'\n", BRAHMADIR);
	}

	printf("===========================\n\n");
	printf("A:     Confirm\n");
	printf("B:     Back\n");

	return newidx;	
}

int print_main_menu(int idx, struct menu_t *menu) {
	int newidx = 0;
	consoleClear();

	printf("\n* BRAHMA *\n\n\n");
	printf("===========================\n");
	newidx = print_menu(idx, menu);
	printf("===========================\n\n");	
	printf("A:     Confirm\n");
	printf("B:     Exit\n");

	return newidx;
}

int get_filename(int idx, char *buf, u32 size) {
   	DIR *dp;
    struct dirent *entry;
    int result = 0;
    int numfiles = 0;

    if((dp = opendir(BRAHMADIR)) && buf && size) {   	
		while((entry = readdir(dp)) != NULL) {
			if (numfiles == idx) {
				snprintf(buf, size-1, "%s%s", BRAHMADIR, entry->d_name);
				result = 1;
				break;
			}
			numfiles++;
		}
    	closedir(dp);
    }
    return result;
}

int menu_cb_recv(int idx, void *param) {
	return recv_arm9_payload();
}

int menu_cb_load(int idx, void *param) {
	char filename[256];
	int result = 0;

	if (param) {
		if (get_filename(*(u32 *)param, &filename, sizeof(filename))) {
			printf("[+] Loading %s\n", filename);
			result = load_arm9_payload(filename);
		}
	}
	return result;
}

int menu_cb_choose_file(int idx, void *param) {
	int curidx = idx;
	int loaded = 0;

	while (aptMainLoop()) {
		gspWaitForVBlank();

		curidx = print_file_list(curidx, &g_file_list);	           
		u32 kDown = wait_key(); 

		if (kDown & KEY_B) {
			break;
		}
		else if (kDown & KEY_A) {
			consoleClear();
			loaded = menu_execute_function(curidx, &g_file_list, &curidx);
			printf("%s\n", loaded? "[+] Success":"[!] Failure");
			wait_any_key();
		}
		else if (kDown & KEY_UP) {
			curidx--;
		}
		else if (kDown & KEY_DOWN) {
			curidx++;
		}
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	return 0;
}

int menu_cb_run(int idx, void *param) {
	int fail_stage;
	
	printf("[+] Running ARM9 payload\n");	
	fail_stage = run_exploit(false);
	
	char *msg;
	switch (fail_stage) {
		case 1:
			msg = "[!] ARM11 exploit failed";
			break;
		case 2:
			msg = "[!] ARM9 exploit failed";
			break;
		default:
			msg = "[!] Unexpected error";
	}
	printf("%s\n", msg);
	return 1;
}