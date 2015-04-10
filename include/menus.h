#pragma once

#include "textmenu.h"

#define BRAHMADIR "/brahma/"

int print_menu (int idx, struct menu_t *menu);
int print_file_list (int idx, struct menu_t *menu);
int print_main_menu(int idx, struct menu_t *menu);

int get_filename(int idx, char *buf, u32 size);

int menu_cb_load(int idx, void *param);
int menu_cb_choose_file(int idx, void *param);
int menu_cb_run(int idx, void *param);
int menu_cb_recv(int idx, void *param);

static const struct menu_t g_main_menu = {
	3,
	{
		{"Load ARM9 payload", &menu_cb_choose_file},
		{"Receive ARM9 payload", &menu_cb_recv},
		{"Run ARM9 payload", &menu_cb_run}
	}
};

static const struct menu_t g_file_list = {
	10,
	{
		{"Slot 0", &menu_cb_load},
		{"Slot 1", &menu_cb_load},
		{"Slot 2", &menu_cb_load},
		{"Slot 3", &menu_cb_load},
		{"Slot 4", &menu_cb_load},
		{"Slot 5", &menu_cb_load},
		{"Slot 6", &menu_cb_load},
		{"Slot 7", &menu_cb_load},
		{"Slot 8", &menu_cb_load},
		{"Slot 9", &menu_cb_load}
	}
};