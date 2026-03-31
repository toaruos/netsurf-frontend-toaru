#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <nsutils/time.h>

#include <toaru/yutani.h>

#include "utils/utils.h"
#include "utils/nsoption.h"
#include "utils/filepath.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"
#include "desktop/browser_history.h"
#include "netsurf/plotters.h"
#include "netsurf/window.h"
#include "netsurf/misc.h"
#include "netsurf/netsurf.h"
#include "netsurf/cookie_db.h"
#include "content/fetch.h"

#include "toaru/gui.h"

extern struct gui_misc_table *toaru_misc_table;
extern struct gui_window_table *toaru_window_table;
extern struct gui_clipboard_table *toaru_clipboard_table;
extern struct gui_layout_table *toaru_layout_table;
extern struct gui_bitmap_table *toaru_bitmap_table;
extern struct gui_fetch_table *toaru_fetch_table;

static void die(const char *error)
{
	fprintf(stderr, "%s\n", error);
	exit(1);
}

static bool nslog_stream_configure(FILE *fptr) {
	setbuf(fptr, NULL);
	return true;
}

static nserror set_defaults(struct nsoption_s *defaults) {
	nsoption_setnull_charp(cookie_file, strdup("~/.netsurf/Cookies"));
	nsoption_setnull_charp(cookie_jar, strdup("~/.netsurf/Cookies"));
	return NSERROR_OK;
}

char ** respaths  = NULL;
static const char *feurl;

/**
 * Entry point from OS.
 *
 * /param argc The number of arguments in the string vector.
 * /param argv The argument string vector.
 * /return The return code to the OS
 */
int main(int argc, char** argv) {
	struct browser_window *bw;
	char *options;
	char *messages;
	nsurl *url;
	nserror ret;
	struct netsurf_table toaru_table = {
		.misc = toaru_misc_table,
		.window = toaru_window_table,
		.clipboard = toaru_clipboard_table,
		.fetch = toaru_fetch_table,
		.utf8 = NULL,
		.bitmap = toaru_bitmap_table,
		.layout = toaru_layout_table,
	};

	toaru_preinit();

	ret = netsurf_register(&toaru_table);
	if (ret != NSERROR_OK) die("NetSurf operation table failed registration");
	nslog_init(nslog_stream_configure, &argc, argv);

	respaths = malloc(sizeof(char*)*2);
	respaths[0] = strdup("/usr/share/netsurf");
	respaths[1] = NULL;

	/* user options setup */
	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) die("Options failed to initialise");
	options = filepath_find(respaths, "Choices");
	nsoption_read(options, nsoptions);
	free(options);
	nsoption_commandline(&argc, argv, nsoptions);

	/* message init */
	messages = filepath_find(respaths, "Messages");
	ret = messages_add_from_file(messages);
	free(messages);
	if (ret != NSERROR_OK) fprintf(stderr, "Message translations failed to load\n");

	/* common initialisation */
	ret = netsurf_init(NULL);
	if (ret != NSERROR_OK) die("NetSurf failed to initialise");

	/* Override, since we have no support for non-core SELECT menu */
	nsoption_set_bool(core_select_menu, true);


	urldb_load_cookies(nsoption_charp(cookie_file));

	/* create an initial browser window */

	NSLOG(netsurf, INFO, "calling browser_window_create");

	feurl = argc > 1 ? argv[1] : NETSURF_HOMEPAGE;

	ret = nsurl_create(feurl, &url);
	if (ret == NSERROR_OK) {
		ret = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      &bw);
		nsurl_unref(url);
	}
	if (ret != NSERROR_OK) {
		fprintf(stderr, "fuck\n");
	} else {
		/* MAIN LOOP HERE */


		toaru_mainloop();

		browser_window_destroy(bw);
	}

	netsurf_exit();

	/* finalise options */
	nsoption_finalise(nsoptions, nsoptions_default);

	/* finalise logging */
	nslog_finalise();

	return 0;
}
