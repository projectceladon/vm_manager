/*
 * Copyright (c) 2020 Intel Corporation.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <curses.h>
#include <form.h>
#include <menu.h>
#include <signal.h>
#include <assert.h>
#include <uuid/uuid.h>
#include "guest.h"
#include "utils.h"

static int disp_field_rows = FORM_NUM;
#define FORM_ROWS   (disp_field_rows + 2)
#define MENU_ROWS 3U
#define MSG_ROWS 1U

#define STATIC_ROWS (2U + 1U + 1U + MENU_ROWS) //	Border, title, message box, menu
#define MIN_FORM_ROWS 8U
#define MIN_ROWS ((MIN_FORM_ROWS) + (STATIC_ROWS))

#define WIN_BODY_ROWS   ((FORM_ROWS) + (MENU_ROWS) + (MSG_ROWS) + 3U)
static uint32_t body_width = 80U; // default body width is 80 and will be updated in prepare_layout()
#define WIN_BODY_COLS   (uint32_t)(body_width)
#define WIN_BODY_STARTX  0U
#define WIN_BODY_STARTY  0U

#define FORM_COLS   ((WIN_BODY_COLS) - 2U)
#define FORM_STARTX ((WIN_BODY_STARTX) + 1U)
#define FORM_STARTY ((WIN_BODY_STARTY) + 2U)

#define MSG_COLS ((WIN_BODY_COLS) - 2U)
#define MSG_STARTX ((WIN_BODY_STARTX) + 1U)
#define MSG_STARTY ((FORM_STARTY) + (FORM_ROWS))

#define MENU_COLS ((WIN_BODY_COLS) - 2U)
#define MENU_STARTX ((WIN_BODY_STARTX) + 1U)
#define MENU_STARTY ((MSG_STARTY) + (MSG_ROWS))

#define PCI_PT_MAX_COLS 40U
#define PT_FIELD_LEN_MAX 1024

typedef union {
	struct {
		int pad;
		int vmin;
		int vmax;
	} integer;
	const char *str;
} field_type_args_t;

typedef struct {
	int num;
	int pick_index;
	int *selected;
	const char *opts[];
} field_sub_opts_t;

typedef struct {
	FIELD *f;
	int type;
	field_type_args_t args;
	field_sub_opts_t *sub_opts;
} field_input_t;

typedef struct {
	FIELD *f;
	const char *str;
} field_label_t;

typedef struct {
	field_label_t label;
	field_input_t input;
} form_field_data_t;

static WINDOW *win_main;
static WINDOW *win_form, *win_form_sub;
static WINDOW *win_msg;
static WINDOW *win_menu, *win_menu_sub;
static FORM *form, *form_msg;
static MENU *menu;
static FIELD *field[FORM_NUM * 2 + 1];
static FIELD **disp_field = NULL;
static ITEM *items[MENU_NUM + 1];
static FIELD *msg_field[2];
static int form_start_row = 0;

static int pt_selected[PT_MAX] = { 0 };
static char pt_opts[PT_FIELD_LEN_MAX] = { 0 };

static field_sub_opts_t firmware_sub_opts = 	{ 2, -1, NULL, { FIRM_OPTS_UNIFIED_STR,  FIRM_OPTS_SPLITED_STR } };
static field_sub_opts_t graphics_sub_opts = 	{ 4, -1, NULL, { VGPU_OPTS_VIRTIO_STR, VGPU_OPTS_RAMFB_STR, VGPU_OPTS_GVTG_STR, VGPU_OPTS_GVTD_STR } };
static field_sub_opts_t gvtg_sub_opts =     	{ 4, -1, NULL, { GVTG_OPTS_V5_1_STR, GVTG_OPTS_V5_2_STR, GVTG_OPTS_V5_4_STR, GVTG_OPTS_V5_8_STR } };
static field_sub_opts_t passthrough_sub_opts = 	{ 5, -1, pt_selected, {NULL} };
static field_sub_opts_t suspend_opts =          { 2, -1, NULL, { SUSPEND_ENABLE_STR, SUSPEND_DISABLE_STR } };

static form_field_data_t g_form_field_data[FORM_NUM + 1] = {
	{ { NULL, "name            :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "flashfiles      :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "emulator        :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "memory          :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "vcpu(1~2048)    :" }, { NULL, FIELD_TYPE_INTEGER, { .integer = { 0, 1U, 2048U     } }, NULL               } },
	{ { NULL, "firmware        :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, &firmware_sub_opts } },
	{ { NULL, "                 " }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "                 " }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "disk size       :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "disk path       :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "graphics        :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, &graphics_sub_opts } },
	{ { NULL, "                 " }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "                 " }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "adb port        :" }, { NULL, FIELD_TYPE_INTEGER, { .integer = { 0, 1024U, 65535U } }, NULL               } },
	{ { NULL, "fastboot port   :" }, { NULL, FIELD_TYPE_INTEGER, { .integer = { 0, 1024U, 65535U } }, NULL               } },
	{ { NULL, "swtpm bin       :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "swtpm data dir  :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "rpmb bin        :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "rpmb data dir   :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "passthrough pci :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, &passthrough_sub_opts } },
	{ { NULL, "aaf path        :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, "suspend support :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, &suspend_opts      } },
	{ { NULL, "extra cmd(qemu) :" }, { NULL, FIELD_TYPE_NORMAL,  {                                 }, NULL               } },
	{ { NULL, NULL }, { NULL, -1, { }, NULL } }
};


enum {
	WHITE_BLACK = 1,
	WHITE_BLUE,
	RED_WHITE,
	GREEN_BLACK
};

static void set_color(void)
{
	start_color();
	init_pair(WHITE_BLACK, COLOR_WHITE, COLOR_BLACK);
	init_pair(WHITE_BLUE,  COLOR_WHITE, COLOR_BLUE);
	init_pair(RED_WHITE,  COLOR_RED, COLOR_WHITE);
	init_pair(GREEN_BLACK, COLOR_GREEN, COLOR_BLACK);
}

static int popup_sub_menu(WINDOW *parent, const char *opts[], int nb_opts, int selected[])
{
	WINDOW *win_sub, *inner;
	MENU *menu;
	ITEM **items;
	int i, ch, ret = -1;
	int max_cols = 0, temp;

	items = malloc(sizeof(ITEM *) * (nb_opts + 1));
	assert(items != NULL);

	for (i = 0; i < nb_opts; i++) {
		if (selected) {
			items[i] = new_item(opts[i], selected[i] ? "*  " : "   ");
		} else {
			items[i] = new_item(opts[i], " ");
		}
		temp = strlen(opts[i]);
		if (temp > max_cols)
			max_cols = temp;
	}
	if (selected)
		max_cols += 3;
	items[i] = NULL;

	menu = new_menu(items);
	win_sub = derwin(parent, nb_opts + 2, max_cols + 3, (getmaxy(parent) - nb_opts - 2)/2, (getmaxx(parent) - max_cols -4)/2);
	box(win_sub, 0, 0);
	set_menu_win(menu, win_sub);
	set_menu_format(menu, nb_opts, 1);

	inner = derwin(win_sub, getmaxy(win_sub) - 2, getmaxx(win_sub) - 2, 1, 1);
	assert(inner != NULL);
	set_menu_sub(menu, inner);
	set_menu_mark(menu, "");

	assert(post_menu(menu) == E_OK);
	refresh();
	wrefresh(parent);

	bool exit = false;
	while ((ch = getch()) != KEY_F(2)) {
		switch (ch) {
		case KEY_DOWN:
			menu_driver(menu, REQ_DOWN_ITEM);
			break;
		case KEY_UP:
			menu_driver(menu, REQ_UP_ITEM);
			break;
		case '\n':
			exit = true;
			for (i = 0; i < nb_opts; i++) 
				if (current_item(menu) == items[i]) {
					if (selected) {
						selected[i] = !selected[i];
					} else {
						ret = i;
					}
				}
			
			break;
		default:
			break;
		}
		current_item(menu);
		wrefresh(win_sub);
		if (exit || ch == 27)
			break;
	}

	wrefresh(win_sub);

	for (i = 0; i < nb_opts; i++) {
		free_item(items[i]);
	}
	free(menu);
	wclear(inner);
	wclear(win_sub);
	delwin(inner);
	delwin(win_sub);

	return ret;
}


void create_main(void)
{
	win_main = newwin(WIN_BODY_ROWS, WIN_BODY_COLS, WIN_BODY_STARTY, WIN_BODY_STARTX);
	assert(win_main != NULL);
	box(win_main, 0, 0);
	mvwprintw(win_main, 1, 1, "Celadon in VM Configuration");
}

static void set_opts_buffer(FIELD *f, const char *in)
{
	char buf[128] = { 0 };

	if (!f || !in)
		return;

	snprintf(buf, 128, "[%15s] ==>", in);
	set_field_buffer(f, 0, buf);
}

int set_field_data(form_index_t index, const char *in)
{
	switch (index) {
	case FORM_INDEX_FIRM:
		if (strcmp(in, FIRM_OPTS_UNIFIED_STR) == 0) {
			set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_PATH].label.f, 0, "    -> bin path :");
			set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_VARS].label.f, 0, "");
			field_opts_on(g_form_field_data[FORM_INDEX_FIRM_PATH].input.f, O_ACTIVE | O_EDIT);
			field_opts_off(g_form_field_data[FORM_INDEX_FIRM_VARS].input.f, O_EDIT);

			firmware_sub_opts.pick_index = FIRM_OPTS_UNIFIED;
		} else if (strcmp(in, FIRM_OPTS_SPLITED_STR) == 0) {
			set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_CODE].label.f, 0, "    -> code path :");
			set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_VARS].label.f, 0, "    -> vars path :");
			field_opts_on(g_form_field_data[FORM_INDEX_FIRM_CODE].input.f, O_ACTIVE | O_EDIT);
			field_opts_on(g_form_field_data[FORM_INDEX_FIRM_VARS].input.f, O_ACTIVE | O_EDIT);

			firmware_sub_opts.pick_index = FIRM_OPTS_SPLITED;
		} else {
			fprintf(stderr, "Invalid firmware type!\n");
			return -1;
		}
		set_opts_buffer(g_form_field_data[FORM_INDEX_FIRM].input.f, in);

		break;
	case FORM_INDEX_VGPU:
		if (strcmp(in, VGPU_OPTS_GVTG_STR) == 0) {
			set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].label.f, 0, " -> gvtg version:");
			set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.f, 0, "[Enter to select] ==>");
			field_opts_on(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.f, O_ACTIVE);
			field_opts_off(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.f, O_EDIT);
			set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].label.f, 0, "   -> vgpu uuid :");
			field_opts_on(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].input.f, O_ACTIVE | O_EDIT);
			graphics_sub_opts.pick_index = VGPU_OPTS_GVTG;
			g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.sub_opts = &gvtg_sub_opts;
		} else if (strcmp(in, VGPU_OPTS_GVTD_STR) == 0) {
			graphics_sub_opts.pick_index = VGPU_OPTS_GVTD;
		} else if (strcmp(in, VGPU_OPTS_VIRTIO_STR) == 0) {
			graphics_sub_opts.pick_index = VGPU_OPTS_VIRTIO;
		} else if (strcmp(in, VGPU_OPTS_RAMFB_STR) == 0) {
			graphics_sub_opts.pick_index = VGPU_OPTS_RAMFB;
		} else {
			fprintf(stderr, "Invalid virtual GPU type!\n");
			return -1;
		}
		set_opts_buffer(g_form_field_data[FORM_INDEX_VGPU].input.f, in);
		break;
	case FORM_INDEX_VGPU_GVTG_VER:
		set_opts_buffer(g_form_field_data[index].input.f, in);
		if (strcmp(in, GVTG_OPTS_V5_1_STR) == 0)
			gvtg_sub_opts.pick_index = GVTG_OPTS_V5_1;
		else if (strcmp(in, GVTG_OPTS_V5_2_STR) == 0)
			gvtg_sub_opts.pick_index = GVTG_OPTS_V5_2;
		else if (strcmp(in, GVTG_OPTS_V5_4_STR) == 0)
			gvtg_sub_opts.pick_index = GVTG_OPTS_V5_4;
		else if (strcmp(in, GVTG_OPTS_V5_8_STR) == 0)
			gvtg_sub_opts.pick_index = GVTG_OPTS_V5_8;
		break;
	case FORM_INDEX_PCI_PT: ;

		strncpy(pt_opts, in, PT_FIELD_LEN_MAX-1);
		pt_opts[PT_FIELD_LEN_MAX-1] = '\0';
		fprintf(stderr, "%s\n", pt_opts);

		char opts[64][20];
		char *temp[64];

		for (int i=0; i<64; i++) {
			temp[i] = opts[i];
		}

		
		int res_count = find_pci("", 64, temp);

		char delim[] = ",";

		char *ptr = strtok(in, delim);

		while (ptr != NULL) {
			for (int i=0; i<res_count; i++) {
				if (strcmp(ptr, temp[i]) == 0) {
					pt_selected[i] = 1;
				}
			}
			ptr = strtok(NULL, delim);
		}

		char buf[1024], out[1024];
		char *p = out;
		int size = 1024, cx;
		unsigned int tot = 0;

		for (int i=0; i < res_count; i++) {
			if (pt_selected[i]) {
				cx = snprintf(p, size, "%s,", temp[i]);
				p += cx; size -= cx; tot += cx;
			}
		}
		if (tot > PCI_PT_MAX_COLS) {
			snprintf(buf, 1024, "[%.*s ...] ==>", PCI_PT_MAX_COLS - 4, out);
		} else  {
			snprintf(buf, 1024, "[%.*s] ==>", PCI_PT_MAX_COLS, out);
		}
		set_field_buffer(g_form_field_data[index].input.f, 0, buf);
		break;
	default:
		set_field_buffer(g_form_field_data[index].input.f, 0, in);
		break;
	}
	return 0;
}

static void create_form_fields(void)
{
	int i, id_label, id_input;

	disp_field = calloc((disp_field_rows * 2 + 1), sizeof (FIELD *));

	for (i = 0; i < FORM_NUM; i++) {
		form_field_data_t *data = &g_form_field_data[i];
		field_type_args_t *args = &data->input.args;
		id_label = i * 2;
		id_input = id_label + 1;

		data->label.f = new_field(1, 18, i,  0, 0, 0);
		data->input.f = new_field(1, WIN_BODY_COLS - 18 - 6, i, 18, 0, 0);
		field[id_label] = data->label.f;
		field[id_input] = data->input.f;

		field_opts_off(data->label.f, O_ACTIVE | O_AUTOSKIP);
		set_field_buffer(data->label.f, 0, data->label.str);

		field_opts_on(data->input.f, O_ACTIVE | O_EDIT);
		field_opts_off(data->input.f, O_AUTOSKIP | O_STATIC);
		set_field_userptr(data->input.f, data);

		if (data->input.sub_opts) {
			field_opts_off(data->input.f, O_EDIT);
			set_field_buffer(data->input.f, 0, "[Enter to select] ==>");
		}

		switch (data->input.type) {
		case FIELD_TYPE_ALNUM:
			set_field_type(data->input.f, TYPE_ALNUM, data->input.f->cols);
			break;
		case FIELD_TYPE_INTEGER:
			set_field_type(data->input.f, TYPE_INTEGER, args->integer.pad, args->integer.vmin, args->integer.vmax);
			break;
		case FIELD_TYPE_NORMAL:
			break;
		case FIELD_TYPE_STATIC:
			field_opts_off(data->input.f, O_EDIT);
			set_field_buffer(data->input.f, 0, data->input.args.str);
			break;
		default:
			break;
		}
	}
}

static void update_disp_field(int start_row)
{
	int i;

	memcpy(disp_field, &field[start_row * 2], (disp_field_rows * 2) * sizeof(FIELD *));
	disp_field[disp_field_rows * 2 + 1] = NULL;

	for (i = 0; i < disp_field_rows * 2; i++) {
		disp_field[i]->frow = i/2;
	}

	unpost_form(form);
	set_form_fields(form, disp_field);
	assert(post_form(form) == E_OK);
}

static void update_scroll_symbol(void)
{
	box(win_form, 0, 0);

	if (form->field[0] == field[0]) {
		mvwhline(win_form, 0, 17, 0, 3);
	} else {
		wattron(win_form, COLOR_PAIR(GREEN_BLACK) | A_BOLD);
		mvwprintw(win_form, 0, 17, "^");
		wattroff(win_form, COLOR_PAIR(GREEN_BLACK) | A_BOLD);
	}

	if (form->field[form->maxfield - 1] == field[FORM_NUM * 2 - 1]) {
		mvwhline(win_form, form->rows + 1, 17, 0, 3);
	} else {
		wattron(win_form, COLOR_PAIR(GREEN_BLACK) | A_BOLD);
		mvwprintw(win_form, form->rows + 1, 17, "v");
		wattroff(win_form, COLOR_PAIR(GREEN_BLACK) | A_BOLD);
	}

	form_driver(form, REQ_NEXT_FIELD);
	form_driver(form, REQ_PREV_FIELD);
	form_driver(form, REQ_END_LINE);
}

static void create_form(void)
{
	create_form_fields();

	form = new_form(field);
	form_opts_off(form, O_BS_OVERLOAD);
	assert(form != NULL);
	win_form = derwin(win_main, FORM_ROWS, FORM_COLS, FORM_STARTY, FORM_STARTX);
	assert(win_form != NULL);
	box(win_form, 0, 0);
	set_form_win(form, win_form);
	win_form_sub = derwin(win_form, FORM_ROWS - 2, FORM_COLS - 2, 1, 1);
	assert(win_form_sub != NULL);
	set_form_sub(form, win_form_sub);

	update_disp_field(0);
	update_scroll_symbol();
}

static void create_menu(void)
{
	int tmp;

	items[MENU_INDEX_SAVE] = new_item("SAVE", "");
	items[MENU_INDEX_EXIT] = new_item("EXIT", "");
	items[MENU_NUM] = NULL;
	menu = new_menu(items);
	assert(menu != NULL);
	win_menu = derwin(win_main, MENU_ROWS, MENU_COLS, MENU_STARTY, MENU_STARTX);
	assert(win_menu != NULL);
	set_menu_win(menu, win_menu);
	set_menu_format(menu, 1, 2);
	tmp = menu->fcols * (menu->namelen + menu->spc_rows) - 1;
	win_menu_sub = derwin(win_menu, 1, tmp, 1, (WIN_BODY_COLS-3-tmp)/2);
	assert(win_menu_sub != NULL);
	set_menu_sub(menu, win_menu_sub);
	set_menu_mark(menu, "");
	set_menu_fore(menu, A_NORMAL);
	box(win_menu, 0, 0);

	assert(post_menu(menu) == E_OK);
}

static void create_msg_box(void)
{
	msg_field[0] = new_field(1, MSG_COLS, 0, 0, 0, 0);
	msg_field[1] = NULL;

	form_msg = new_form(msg_field);
	assert(form_msg != NULL);

	field_opts_off(msg_field[0], O_ACTIVE | O_AUTOSKIP);
	set_field_fore(msg_field[0], COLOR_PAIR(RED_WHITE));

	win_msg = derwin(win_main, MSG_ROWS, MSG_COLS, MSG_STARTY, MSG_STARTX);
	assert(win_msg != NULL);

	set_form_win(form_msg, win_msg);
	set_form_sub(form_msg, win_msg);
	assert(post_form(form_msg) == E_OK);
}

static void exit_tui(void)
{
	int i;

	unpost_form(form);
	for(i = 0; i < FORM_NUM; i++) {
		free_field(field[i * 2]);
		free_field(field[i * 2 + 1]);
	}
	free(disp_field);
	free_form(form);

	free_field(msg_field[0]);

	unpost_menu(menu);
	for (i = 0; i < 2; i++) {
		free_item(items[i]);
	}
	free_menu(menu);

	delwin(win_form_sub);
	delwin(win_form);
	delwin(win_menu_sub);
	delwin(win_menu);
	delwin(win_msg);
	delwin(win_main);
	endwin();
}

enum {
	KEY_DRIVER_NORMAL  = 0,
	KEY_DRIVER_SWITCH,
	KEY_DRIVER_BREAK
};

static int form_key_enter(void)
{
	int ret = KEY_DRIVER_NORMAL;
	form_field_data_t *ff = (form_field_data_t *)field_userptr(form->current);
	int pick_index = -1;
	char buf[512] = { 0 };

	if (ff->input.sub_opts) {
		if (form->current == g_form_field_data[FORM_INDEX_PCI_PT].input.f) {
			char opts[PT_MAX][20];
			char *temp[PT_MAX];
			char buf[1024];

			for (int i=0; i<64; i++) {
				temp[i] = opts[i];
			}

			
			int res_count = find_pci("", PT_MAX, temp);

			char out[1024];

			pick_index = popup_sub_menu(win_form, temp, res_count, pt_selected);
			char *p = out;
			int size = 1024, cx;
			unsigned int tot = 0;

			for (int i=0; i<res_count; i++) {
				if (pt_selected[i]) {
					cx = snprintf(p, size, "%s,", temp[i]);
					p += cx; size -= cx; tot += cx;
				}
			}
			if (tot > PCI_PT_MAX_COLS) {
				snprintf(buf, 1024, "[%.*s ...] ==>", PCI_PT_MAX_COLS - 4, out);
			} else  {
				snprintf(buf, 1024, "[%.*s] ==>", PCI_PT_MAX_COLS, out);
			}
			strncpy(pt_opts, out, 1023);
			pt_opts[1023] = '\0';

			set_field_buffer(form->current, 0, buf);

		} else {
			pick_index = popup_sub_menu(win_form, ff->input.sub_opts->opts, ff->input.sub_opts->num, ff->input.sub_opts->selected);
		}
		update_scroll_symbol();
		if (pick_index != -1) {
			ff->input.sub_opts->pick_index = pick_index;
			snprintf(buf, 512, "[%15s] ==>", ff->input.sub_opts->opts[pick_index]);
			set_field_buffer(form->current, 0, buf);
			if (form->current == g_form_field_data[FORM_INDEX_FIRM].input.f) {
				if (pick_index == FIRM_OPTS_UNIFIED) {
					set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_PATH].label.f, 0, "    -> bin path :");
					field_opts_on(g_form_field_data[FORM_INDEX_FIRM_PATH].input.f, O_ACTIVE | O_EDIT);
					set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_VARS].label.f, 0, "");
					set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_VARS].input.f, 0, "");
					field_opts_off(g_form_field_data[FORM_INDEX_FIRM_VARS].input.f, O_EDIT);
				} else if (pick_index == FIRM_OPTS_SPLITED) {
					set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_CODE].label.f, 0, "   -> code path :");
					field_opts_on(g_form_field_data[FORM_INDEX_FIRM_CODE].input.f, O_ACTIVE | O_EDIT);
					set_field_buffer(g_form_field_data[FORM_INDEX_FIRM_VARS].label.f, 0, "   -> vars path :");
					field_opts_on(g_form_field_data[FORM_INDEX_FIRM_VARS].input.f, O_ACTIVE | O_EDIT);
				}
			}
			if (form->current == g_form_field_data[FORM_INDEX_VGPU].input.f) {
				if (pick_index == VGPU_OPTS_GVTG) {
					g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.sub_opts = &gvtg_sub_opts;
					set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.f, 0, "[Enter to select] ==>");
					field_opts_on(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.f, O_ACTIVE);
					field_opts_off(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.f, O_EDIT);
					set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].label.f, 0, " -> gvtg version:");
					set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].label.f, 0, "   -> vgpu uuid :");
					field_opts_on(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].input.f, O_ACTIVE | O_EDIT);

					if (strlen(g_strstrip(g_strdup(field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].input.f, 0)))) == 0) {
						uuid_t uuid;
						char uuid_str[UUID_STR_LEN] = { 0 };

						uuid_generate(uuid);
						uuid_unparse(uuid, uuid_str);
						set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].input.f, 0, uuid_str);
					}
				} else {
					g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.sub_opts = NULL;
					set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].label.f, 0, "");
					set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.f, 0, "");
					field_opts_off(g_form_field_data[FORM_INDEX_VGPU_GVTG_VER].input.f, O_EDIT);
					set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].label.f, 0, "");
					set_field_buffer(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].input.f, 0, "");
					field_opts_off(g_form_field_data[FORM_INDEX_VGPU_GVTG_UUID].input.f, O_EDIT);
				}
			}
		}
		set_field_back(form->current, COLOR_PAIR(WHITE_BLUE));
		unpost_form(form);
		post_form(form);
		form_driver(form, REQ_END_LINE);
	} else {
		set_field_back(form->current, COLOR_PAIR(WHITE_BLACK));
		unpost_form(form);
		post_form(form);

		set_menu_fore(menu, COLOR_PAIR(WHITE_BLUE));
		pos_menu_cursor(menu);
		ret = KEY_DRIVER_SWITCH;
	}

	return ret;
}

static int key_form_driver(int ch)
{
	int ret = KEY_DRIVER_NORMAL;

	switch (ch) {
	case KEY_DOWN:
		if (form->current == field[FORM_NUM * 2 - 1]) {
			set_field_back(form->current, COLOR_PAIR(WHITE_BLACK));
			form_driver(form, REQ_LAST_FIELD);

			menu_driver(menu, REQ_FIRST_ITEM);
			set_menu_fore(menu, COLOR_PAIR(WHITE_BLUE));
			pos_menu_cursor(menu);
			ret = KEY_DRIVER_SWITCH;
		} else if (form->current == form->field[form->maxfield - 1]) {
			set_field_back(form->current, COLOR_PAIR(WHITE_BLACK));
			update_disp_field(++form_start_row);
			form_driver(form, REQ_LAST_FIELD);
			set_field_back(form->current, COLOR_PAIR(WHITE_BLUE));
		} else {
			set_field_back(form->current, COLOR_PAIR(WHITE_BLACK));
			form_driver(form, REQ_NEXT_FIELD);
			form_driver(form, REQ_END_LINE);
			set_field_back(form->current, COLOR_PAIR(WHITE_BLUE));
		}
		update_scroll_symbol();
		break;
	case KEY_UP:
		if (form->current == field[1]) {
			break;
		}
		if (form->current == form->field[1] && (form_start_row > 0)) {
			set_field_back(form->current, COLOR_PAIR(WHITE_BLACK));
			update_disp_field(--form_start_row);
			form_driver(form, REQ_FIRST_FIELD);
			set_field_back(form->current, COLOR_PAIR(WHITE_BLUE));
		} else {
			set_field_back(form->current, COLOR_PAIR(WHITE_BLACK));
			form_driver(form, REQ_PREV_FIELD);
			form_driver(form, REQ_END_LINE);
			set_field_back(form->current, COLOR_PAIR(WHITE_BLUE));
		}
		update_scroll_symbol();
		break;
	case KEY_LEFT:
		if (field_opts(form->current) & O_EDIT)
			form_driver(form, REQ_PREV_CHAR);
		break;

	case KEY_RIGHT:
		if (field_opts(form->current) & O_EDIT)
			form_driver(form, REQ_NEXT_CHAR);
		break;
	case KEY_BACKSPACE:
		if (field_opts(form->current) & O_EDIT)
			form_driver(form, REQ_DEL_PREV);
		break;
	case KEY_DC:
		if (field_opts(form->current) & O_EDIT)
			form_driver(form, REQ_DEL_CHAR);
		break;
	case KEY_HOME:
		if (field_opts(form->current) & O_EDIT)
			form_driver(form, REQ_BEG_FIELD);
		break;
	case KEY_END:
		if (field_opts(form->current) & O_EDIT)
			form_driver(form, REQ_END_FIELD);
		break;
	case '\n':
		ret = form_key_enter();
		break;
	default:
		/* space is not acceptable except extra field */
		if (form->current != g_form_field_data[FORM_INDEX_EXTRA_CMD].input.f)
			if (ch == ' ')
				break;

		form_driver(form, ch);
		break;
	}

	return ret;
}

void show_msg(const char *msg)
{
	set_field_buffer(msg_field[0], 0, msg);
}

int get_field_data(form_index_t index, char *out, size_t out_len)
{
	if (!out)
		return -1;

	if (g_form_field_data[index].input.sub_opts) {
		if  (g_form_field_data[index].input.sub_opts->selected) {
			strncpy(out, pt_opts, out_len);
		} else if ((g_form_field_data[index].input.sub_opts->pick_index == -1)){
			memset(out, '\0', out_len);
		} else {
			strncpy(out,field_buffer(g_form_field_data[index].input.f, 0),out_len);
			g_strdelimit(out, "[", ' ');
			g_strdelimit(out, "]", '\0');
		}
	} else {
		strncpy(out,field_buffer(g_form_field_data[index].input.f, 0),out_len);
	}

	g_strstrip(out);

	return 0;
}

static int key_menu_driver(int ch)
{
	int ret = KEY_DRIVER_NORMAL;

	switch (ch) {
	case KEY_UP:
		set_menu_fore(menu, A_NORMAL);
		pos_form_cursor(form);
		form_driver(form, REQ_LAST_FIELD);
		form_driver(form, REQ_END_LINE);
		set_field_back(form->current, COLOR_PAIR(WHITE_BLUE));
		ret = KEY_DRIVER_SWITCH;
		break;
	case KEY_LEFT:
		menu_driver(menu, REQ_LEFT_ITEM);
		break;
	case KEY_RIGHT:
		menu_driver(menu, REQ_RIGHT_ITEM);
		break;
	case '\n':
		ret = KEY_DRIVER_BREAK;
		if ((current_item(menu) == items[MENU_INDEX_SAVE])) {
			if (generate_keyfile() != 0)
				ret = KEY_DRIVER_NORMAL;
		}
		break;
	default:
		break;
	}

	return ret;
}

int prepare_layout(void)
{
	int maxy, maxx;
	getmaxyx(stdscr, maxy, maxx);

	if ((uint32_t)maxy < MIN_ROWS || (uint32_t)maxx < WIN_BODY_COLS) {
		fprintf(stderr, "The window is too small to display!\r\n"
				"Please make sure window: rows >= %d, cols >= %d\r\n", MIN_ROWS, WIN_BODY_COLS);
		return -1;
	}

	if ((uint32_t)maxy <= (FORM_NUM + STATIC_ROWS + 2))
		disp_field_rows = maxy - STATIC_ROWS - 2;

	body_width = maxx;

	return 0;
}

int create_guest_tui(char *name)
{
	int ch, ret;
	bool cursor_on_menu;

	initscr();

	if (prepare_layout() < 0) {
		endwin();
		return -1;
	}

	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	set_color();

	create_main();

	create_form();

	create_menu();

	create_msg_box();

	if (name) {
		if (load_form_data(name) != 0) {
			exit_tui();
			fprintf(stderr, "Error loading guest: %s\n", name);
			return -1;
		}
	}

	/* move cursor to first field of form */
	pos_form_cursor(form);
	form_driver(form, REQ_FIRST_FIELD);
	set_field_back(form->current, COLOR_PAIR(2));
	cursor_on_menu = false;

	refresh();
	wrefresh(win_main);

	while ((ch = getch()) != KEY_F(9)) {
		if (cursor_on_menu) {
			ret = key_menu_driver(ch);
		} else {
			ret = key_form_driver(ch);
		}

		if (ret == KEY_DRIVER_SWITCH)
			cursor_on_menu = !cursor_on_menu;

		if (ret == KEY_DRIVER_BREAK)
			break;

		refresh();
		wrefresh(win_main);
		wrefresh(win_menu);
		wrefresh(win_form);
	}


	exit_tui();

	return 0;
}
