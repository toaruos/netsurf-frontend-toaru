#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "utils/log.h"
#include "netsurf/browser_window.h"
#include "netsurf/clipboard.h"

static void gui_get_clipboard(char **buffer, size_t *length) {
	/* TODO */
}

static void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles) {
	/* TODO */
}

static struct gui_clipboard_table clipboard_table = {
	.get = gui_get_clipboard,
	.set = gui_set_clipboard,
};

struct gui_clipboard_table *toaru_clipboard_table = &clipboard_table;
