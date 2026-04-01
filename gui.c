#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>

#include <sys/fswait.h>

#include <nsutils/time.h>

#include <toaru/yutani.h>
#include <toaru/decorations.h>
#include <toaru/menu.h>
#include <toaru/hashmap.h>
#include <toaru/list.h>
#include <toaru/text.h>
#include <toaru/button.h>

#include "utils/utils.h"
#include "utils/sys_time.h"
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
#include "toaru/schedule.h"

yutani_t * yctx = NULL;
hashmap_t * my_windows = NULL;

static int redraw_next = 0;

struct TT_Font * tt_font_thin = NULL;
struct TT_Font * tt_font_bold = NULL;
struct TT_Font * tt_font_italic = NULL;
struct TT_Font * tt_font_bold_italic = NULL;

struct TT_Font * tt_font_mono = NULL;
struct TT_Font * tt_font_mono_bold = NULL;
struct TT_Font * tt_font_mono_italic = NULL;
struct TT_Font * tt_font_mono_bold_italic = NULL;

static struct menu_bar menu_bar = {0};
static struct menu_bar_entries menu_entries[] = {
	{(char*)"File", (char*)"file"}, /* 0 */
	{(char*)"Help", (char*)"help"}, /* 4 */
	{NULL, NULL},
};

static void redraw_window_callback(struct menu_bar * self) {
	(void)self;
	redraw_next = 1;
}

static void _menu_action_exit(struct MenuEntry * entry) {
	exit(0);
}

static void _menu_action_help(struct MenuEntry * entry) {
	system("help-browser netsurf.trt &");
	redraw_next = 1;
}

struct MenuList_Real {
	list_t * entries;
	gfx_context_t * ctx;
	yutani_window_t * window;
	struct MenuSet * set;
	struct MenuList * child;
	struct MenuList * parent;
	struct menu_bar * _bar;
	int closed;
	int flags;
	int tail_offset;
	yutani_window_t * main_window;
};

static void _menu_action_about(struct MenuEntry * entry) {
	char about_cmd[1024] = "\0";
	strcat(about_cmd, "about \"About Netsurf\" /usr/share/icons/48/netsurf.png \"Netsurf 3.11\" \"© The NetSurf Developers\n\nReleased under the\nGNU Public License version 2\n\n%hhttps://www.netsurf-browser.org/\" ");
	char coords[100];
	sprintf(coords, "%d %d &", (int)yctx->display_width / 2, (int)yctx->display_height / 2);
	strcat(about_cmd, coords);
	system(about_cmd);
	redraw_next = 1;
}

static void create_shared_menubar(void) {
	menu_bar.entries = menu_entries;
	menu_bar.redraw_callback = redraw_window_callback;

	menu_bar.set = menu_set_create();

	struct MenuList * m = menu_create(); /* File */
	menu_insert(m, menu_create_normal("exit",NULL,"Exit", _menu_action_exit));
	menu_set_insert(menu_bar.set, (char*)"file", m);

	m = menu_create();
	menu_insert(m, menu_create_normal("help",NULL,"Contents",_menu_action_help));
	menu_insert(m, menu_create_separator());
	menu_insert(m, menu_create_normal("star",NULL,"About Netsurf",_menu_action_about));
	menu_set_insert(menu_bar.set, (char*)"help", m);
}

void toaru_preinit(void) {
	yctx = yutani_init();
	init_decorations();
	my_windows = hashmap_create_int(10);

	/* Sans serif fonts */
	tt_font_thin = tt_font_from_shm("sans-serif");
	tt_font_bold = tt_font_from_shm("sans-serif.bold");
	tt_font_italic = tt_font_from_shm("sans-serif.italic");
	tt_font_bold_italic = tt_font_from_shm("sans-serif.bolditalic");

	/* Monospace fonts */
	tt_font_mono = tt_font_from_shm("monospace");
	tt_font_mono_bold = tt_font_from_shm("monospace.bold");
	tt_font_mono_italic = tt_font_from_shm("monospace.italic");
	tt_font_mono_bold_italic = tt_font_from_shm("monospace.bolditalic");


	create_shared_menubar();
}

#define STATUS_HEIGHT 24
static void _draw_status(struct gui_window *gw) {

	/* Background gradient */
	uint32_t gradient_top = rgb(80,80,80);
	uint32_t gradient_bot = rgb(59,59,59);
	draw_rectangle(gw->ctx, gw->bounds.left_width, gw->ctx->height - gw->bounds.bottom_height - STATUS_HEIGHT,
			gw->ctx->width - gw->bounds.width, 1, rgb(110,110,110) );
	for (int i = 1; i < STATUS_HEIGHT; ++i) {
		uint32_t c = interp_colors(gradient_top, gradient_bot, i * 255 / STATUS_HEIGHT);
		draw_rectangle(gw->ctx, gw->bounds.left_width, gw->ctx->height - gw->bounds.bottom_height - STATUS_HEIGHT + i,
				gw->ctx->width - gw->bounds.width, 1, c );
	}


	/* Text with draw shadow */
	{
		sprite_t * _tmp_s = create_sprite(gw->ctx->width - gw->bounds.width - 4, STATUS_HEIGHT-3, ALPHA_EMBEDDED);
		gfx_context_t * _tmp = init_graphics_sprite(_tmp_s);

		draw_fill(_tmp, rgba(0,0,0,0));
		tt_set_size(tt_font_thin, 13);
		tt_draw_string(_tmp, tt_font_thin, 1, 14, gw->window_status, rgb(0,0,0));
		blur_context_box(_tmp, 4);

		tt_draw_string(_tmp, tt_font_thin, 0, 13, gw->window_status, rgb(255,255,255));

		free(_tmp);
		draw_sprite(gw->ctx, _tmp_s, gw->bounds.left_width + 4, gw->ctx->height - gw->bounds.bottom_height - STATUS_HEIGHT + 3);
		sprite_free(_tmp_s);
	}
}

/* Button row visibility statuses */

#define BUTTON_SPACE 34
#define BUTTON_COUNT 5
static int _button_hilights[BUTTON_COUNT] = {3,3,3,3,3};
static int _button_disabled[BUTTON_COUNT] = {1,1,0,0,0};
static int _button_hover = -1;
static int nav_bar_height = 36;

static char * nav_bar = NULL;
static int  nav_bar_cursor = 0;
static int  nav_bar_cursor_x = 0;
static int  nav_bar_focused = 0;
static int  nav_bar_blink = 0;
static struct timeval nav_bar_last_blinked;

/**
 * Render toolbar buttons
 */
static void _draw_buttons(gfx_context_t * ctx, struct decor_bounds bounds) {

	/* Draws the toolbar background as a gradient; XXX hardcoded theme details */
	uint32_t gradient_top = rgb(59,59,59);
	uint32_t gradient_bot = rgb(40,40,40);
	for (int i = 0; i < 36; ++i) {
		uint32_t c = interp_colors(gradient_top, gradient_bot, i * 255 / 36);
		draw_rectangle(ctx, bounds.left_width, bounds.top_height + MENU_BAR_HEIGHT + i,
				BUTTON_SPACE * BUTTON_COUNT, 1, c);
	}

	int x = 0;
	int i = 0;
#define draw_button(label) do { \
	struct TTKButton _up = {bounds.left_width + 2 + x,bounds.top_height + MENU_BAR_HEIGHT + 2,32,32,(char*)"\033" label,_button_hilights[i] | (_button_disabled[i] << 8)}; \
	ttk_button_draw(ctx, &_up); \
	x += BUTTON_SPACE; i++; } while (0)

	/* Draw actual buttons */
	draw_button("back");
	draw_button("forward");
	draw_button("star");
	draw_button("home");
	draw_button("refresh");
}

static void _draw_nav_bar(struct gui_window * gw, struct decor_bounds bounds) {

	gfx_context_t * ctx = gw->ctx;

	/* Draw toolbar background */
	uint32_t gradient_top = rgb(59,59,59);
	uint32_t gradient_bot = rgb(40,40,40);
	int x = BUTTON_SPACE * BUTTON_COUNT;

	for (int i = 0; i < 36; ++i) {
		uint32_t c = interp_colors(gradient_top, gradient_bot, i * 255 / 36);
		draw_rectangle(ctx, bounds.left_width + BUTTON_SPACE * BUTTON_COUNT, bounds.top_height + MENU_BAR_HEIGHT + i,
				ctx->width - bounds.width - BUTTON_SPACE * BUTTON_COUNT, 1, c);
	}

	/* Draw input box */
	if (nav_bar_focused && gw->win->focused) {
		struct gradient_definition edge = {28, bounds.top_height + MENU_BAR_HEIGHT + 3, rgb(0,120,220), rgb(0,120,220)};
		draw_rounded_rectangle_pattern(ctx, bounds.left_width + 2 + x + 1, bounds.top_height + MENU_BAR_HEIGHT + 4, gw->win->width - bounds.width - x - 6, 26, 4, gfx_vertical_gradient_pattern, &edge);
		draw_rounded_rectangle(ctx, bounds.left_width + 2 + x + 3, bounds.top_height + MENU_BAR_HEIGHT + 6, gw->win->width - bounds.width - x - 10, 22, 2, rgb(250,250,250));
	} else {
		struct gradient_definition edge = {28, bounds.top_height + MENU_BAR_HEIGHT + 3, rgb(90,90,90), rgb(110,110,110)};
		draw_rounded_rectangle_pattern(ctx, bounds.left_width + 2 + x + 1, bounds.top_height + MENU_BAR_HEIGHT + 4, gw->win->width - bounds.width - x - 6, 26, 4, gfx_vertical_gradient_pattern, &edge);
		draw_rounded_rectangle(ctx, bounds.left_width + 2 + x + 2, bounds.top_height + MENU_BAR_HEIGHT + 5, gw->win->width - bounds.width - x - 8, 24, 3, rgb(250,250,250));
	}

	if (!nav_bar) nav_bar = strdup("");

	/* Draw the nav bar text, ellipsified if needed */
	int max_width = gw->win->width - bounds.width - x - 12;
	char * name = tt_ellipsify(nav_bar, 13, tt_font_thin, max_width, NULL);
	tt_draw_string(ctx, tt_font_thin, bounds.left_width + 2 + x + 5, bounds.top_height + MENU_BAR_HEIGHT + 8 + 13, name, rgb(0,0,0));
	free(name);

	if (nav_bar_focused && gw->win->focused && !nav_bar_blink) {
		/* Draw cursor indicator at cursor_x */
		draw_line(ctx,
				bounds.left_width + 2 + x + 5 + nav_bar_cursor_x,
				bounds.left_width + 2 + x + 5 + nav_bar_cursor_x,
				bounds.top_height + MENU_BAR_HEIGHT + 8,
				bounds.top_height + MENU_BAR_HEIGHT + 8 + 15,
				rgb(0,0,0));
	}
}

static void redraw_placeholder(struct gui_window *gw) {
	draw_fill(gw->ctx, rgb(255,255,255));
	decor_get_bounds(gw->win, &gw->bounds);

	menu_bar.x = gw->bounds.left_width;
	menu_bar.y = gw->bounds.top_height;
	menu_bar.width = gw->ctx->width - gw->bounds.width;
	menu_bar.window = gw->win;
	menu_bar_render(&menu_bar, gw->ctx);
	_draw_buttons(gw->ctx, gw->bounds);
	_draw_nav_bar(gw, gw->bounds);
	_draw_status(gw);

	render_decorations(gw->win, gw->ctx, gw->title);

	gfx_context_t * sub = init_graphics_subregion(gw->ctx, gw->viewport_x, gw->viewport_y, gw->viewport_w, gw->viewport_h);

	struct redraw_context ctrx = {
		.priv = sub,
		.interactive = true,
		.background_images = true,
		.plot = &toaru_plotters
	};

	struct rect clip;
	clip.x0 = 0;
	clip.y0 = 0;
	clip.x1 = sub->width;
	clip.y1 = sub->height;

	browser_window_redraw(gw->bw, -gw->scroll_x, -gw->scroll_y, &clip, &ctrx);
	if (gw->caret_h) {
		draw_line(sub,
			gw->caret_x - gw->scroll_x,
			gw->caret_x - gw->scroll_x,
			gw->caret_y - gw->scroll_y,
			gw->caret_y - gw->scroll_y + gw->caret_h, rgb(0,0,0));
	}

	free(sub);

	flip(gw->ctx);
	yutani_flip(yctx, gw->win);
	yutani_window_advertise_icon(yctx, gw->win, gw->title, (char*)"netsurf");
}

static int _down_button = -1;
static void _set_hilight(int index, int hilight) {
	int _update = 0;
	if (_button_hover != index || (_button_hover == index && index != -1 && _button_hilights[index] != hilight)) {
		if (_button_hover != -1 && _button_hilights[_button_hover] != 3) {
			_button_hilights[_button_hover] = 3;
			_update = 1;
		}
		_button_hover = index;
		if (index != -1 && !_button_disabled[index]) {
			_button_hilights[_button_hover] = hilight;
			_update = 1;
		}
		if (_update) {
			redraw_next = 1;
		}
	}
}

/**
 * Handle toolbar button clicking
 */
static void _handle_button_press(int index, yutani_wid_t wid) {
	if (index != -1 && _button_disabled[index]) return; /* can't click disabled buttons */
	struct gui_window * gw = hashmap_get(my_windows, (void*)(uintptr_t)wid);
	if (!gw) return;

	switch (index) {
		case 0:
			/* Back */
			if (browser_window_back_available(gw->bw)) browser_window_history_back(gw->bw, false);
			break;
		case 1:
			/* Forward */
			if (browser_window_forward_available(gw->bw)) browser_window_history_forward(gw->bw, false);
			break;
		case 2:
			/* Favorite */
			break;
		case 3:
			/* Home */
			{
				nsurl *url;
				nserror error = nsurl_create("https://en.wikipedia.org/wiki/?useskin=monobook", &url);
				if (error == NSERROR_OK) {
					browser_window_navigate(gw->bw, url, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
					nsurl_unref(url);
				}
			}
			break;
		case 4:
			/* refresh */
			browser_window_reload(gw->bw, true);
			break;
		default:
			/* ??? */
			break;
	}
}


static struct gui_window * gui_window_create(struct browser_window *bw, struct gui_window *existing, gui_window_create_flags flags) {
	struct gui_window * gw = calloc(1, sizeof(struct gui_window));
	gw->bw = bw;

	gw->win = yutani_window_create(yctx, yctx->display_width - 40, yctx->display_height - 60);
	gw->ctx = init_graphics_yutani_double_buffer(gw->win);
	gw->title = strdup("Netsurf");
	gw->window_status = strdup("");
	decor_get_bounds(gw->win, &gw->bounds);

	yutani_window_move(yctx, gw->win, 20, 40);

	hashmap_set(my_windows, (void*)(uintptr_t)gw->win->wid, gw);

	redraw_placeholder(gw);

	return gw;
}

static void gui_window_destroy(struct gui_window *gw) {
	/* TODO: close window */
	free(gw);
}

static gfx_context_t * clip_context = NULL;
static int clip_x = 0;
static int clip_y = 0;
static int clip_w = 0;
static int clip_h = 0;

static nserror toaru_plot_clip(const struct redraw_context *ctx, const struct rect *clip) {
	gfx_context_t * real = ctx->priv;
	if (!clip) {
		fprintf(stderr, "uh, what?\n");
		return NSERROR_INVALID;
	}
	if (clip_context) free(clip_context);

	clip_context = init_graphics_subregion(real, clip->x0, clip->y0, clip->x1 - clip->x0, clip->y1 - clip->y0);
	clip_x = clip->x0;
	clip_y = clip->y0;
	clip_w = clip->x1 - clip_x;
	clip_h = clip->y1 - clip_y;

	return NSERROR_OK;
}

static nserror toaru_plot_arc(const struct redraw_context *ctx, const plot_style_t *style, int x, int y, int radius, int angle1, int angle2) {
	fprintf(stderr, "plot arc TDOO\n");
	return NSERROR_OK;
}

static nserror toaru_plot_disc(const struct redraw_context *ctx, const plot_style_t *style, int x, int y, int radius) {
	fprintf(stderr, "plot disc TDOO\n");
	return NSERROR_OK;
}

static nserror toaru_plot_line(const struct redraw_context *ctx, const plot_style_t *style, const struct rect *line) {
	gfx_context_t * gctx = clip_context ? clip_context : ctx->priv;

	#if 0
	fprintf(stderr, "netsurf: plot line %d %d %d %d\n", line->x0, line->y0, line->x1, line->y1);
	fprintf(stderr, "style = %d, width=%d (converted as %f)\n",
		style->stroke_type,
		style->stroke_width,
		plot_style_fixed_to_float(style->stroke_width));
	fprintf(stderr, "Unconverted color is %#x\n", style->stroke_colour);
	#endif

	if (line->x0 == line->x1 && line->y0 == line->y1) return NSERROR_OK;

	float x0 = (line->x0 == line->x1) ? line->x0 + 0.5 : line->x0;
	float y0 = (line->y0 == line->y1) ? line->y0 + 0.5 : line->y0;
	float x1 = (line->x0 == line->x1) ? line->x1 + 0.5 : line->x1;
	float y1 = (line->y0 == line->y1) ? line->y1 + 0.5 : line->y1;

	float stroke_width = style->stroke_width == 0 ? 0.5 : plot_style_fixed_to_float(style->stroke_width) / 2.0;

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		struct TT_Contour * contour = tt_contour_start(x0 - clip_x, y0 - clip_y);

		if (style->stroke_type == PLOT_OP_TYPE_DOT || style->stroke_type == PLOT_OP_TYPE_DASH) {
			float x = x0 - clip_x;
			float y = y0 - clip_y;
			float dx = x1 - x0;
			float dy = y1 - y0;
			float distance = sqrt(dx * dx + dy * dy);
			float traveled = 0.0;
			int divisions = distance / stroke_width;
			dx /= divisions;
			dy /= divisions;

			if (style->stroke_type == PLOT_OP_TYPE_DOT) {
				while (traveled < distance) {
					contour = tt_contour_line_to(contour, x + 0.001, y + 0.001);
					x += dx;
					y += dy;
					traveled += sqrt(dx * dx + dy * dy);
					if (traveled > distance) break;
					contour = tt_contour_move_to(contour, x, y);
				}
			} else if (style->stroke_type == PLOT_OP_TYPE_DASH) {
				/* March lines */
				while (traveled < distance) {
					x += dx;
					y += dy;
					traveled += sqrt(dx * dx + dy * dy);
					if (traveled > distance) {
						contour = tt_contour_line_to(contour, x1 - clip_x, y1 - clip_y);
						break;
					}
					contour = tt_contour_line_to(contour, x, y);
					x += dx;
					y += dy;
					traveled += sqrt(dx * dx + dy * dy);
					if (traveled > distance) break;
					contour = tt_contour_move_to(contour, x, y);
				}
			}
		} else {
			contour = tt_contour_line_to(contour, x1 - clip_x, y1 - clip_y);
		}

		struct TT_Shape * stroke = tt_contour_stroke_shape(contour, stroke_width);
		tt_path_paint(gctx, stroke, convert_ns_color(style->stroke_colour));
		free(stroke);
		free(contour);
	}

	return NSERROR_OK;
}

static nserror toaru_plot_rectangle(const struct redraw_context *ctx, const plot_style_t *style, const struct rect *nsrect) {
	gfx_context_t * gctx = clip_context ? clip_context : ctx->priv;
	//fprintf(stderr, "draw with fill color %#x\n", style->fill_colour);
	draw_rectangle(gctx, nsrect->x0 - clip_x, nsrect->y0 - clip_y, nsrect->x1 - nsrect->x0, nsrect->y1 - nsrect->y0, convert_ns_color(style->fill_colour));
	return NSERROR_OK;
}

static nserror toaru_plot_polygon(const struct redraw_context *ctx, const plot_style_t *style, const int *p, unsigned int n) {
	gfx_context_t * gctx = clip_context ? clip_context : ctx->priv;
	struct TT_Contour * contour = NULL;

	for (unsigned int i = 0; i < n; i += 2) {
		if (!contour) {
			contour = tt_contour_start(p[i] - clip_x,p[i+1] - clip_y);
		} else {
			contour = tt_contour_line_to(contour, p[i] - clip_x, p[i+1] - clip_y);
		}
	}

	struct TT_Shape * shape = tt_contour_finish(contour);

	tt_path_paint(gctx, shape, convert_ns_color(style->fill_colour));

	free(shape);
	free(contour);

	return NSERROR_OK;
}

static nserror toaru_plot_path(const struct redraw_context *ctx, const plot_style_t *pstyle, const float *p, unsigned int n, const float transform[6]) {
	fprintf(stderr, "plot path TODO\n");
	return NSERROR_OK;
}

struct bitmap {
	sprite_t internal;
	int converted;
};

static nserror toaru_plot_bitmap(const struct redraw_context *ctx,
		  struct bitmap *bitmap,
		  int x, int y,
		  int width,
		  int height,
		  colour bg,
		  bitmap_flags_t flags) {

	if (width <= 0 || height <= 0) return NSERROR_OK;

	gfx_context_t * gctx = clip_context ? clip_context : ctx->priv;

	bool repeat_x = (flags & BITMAPF_REPEAT_X);
	bool repeat_y = (flags & BITMAPF_REPEAT_Y);

	for (int i = y - clip_y; i < clip_h; i += height) {
		if (i + height < 0) continue;
		for (int j = x - clip_x; j < clip_w; j += width) {
			if (j + width < 0) continue;
			if (width == bitmap->internal.width && height == bitmap->internal.height) {
				draw_sprite(gctx, &bitmap->internal, j, i);
			} else {
				draw_sprite_scaled(gctx, &bitmap->internal, j, i, width, height);
			}
			if (!repeat_x) break;
		}
		if (!repeat_y) break;
	}

	return NSERROR_OK;
}

static nserror toaru_plot_text(const struct redraw_context *ctx,
		const struct plot_font_style *fstyle,
		int x,
		int y,
		const char *text,
		size_t length)
{
	gfx_context_t * gctx = clip_context ? clip_context : ctx->priv;

	toaru_draw_text(gctx, fstyle, x - clip_x, y - clip_y, text, length);

	return NSERROR_OK;
}


const struct plotter_table toaru_plotters = {
	.clip = toaru_plot_clip,
	.arc = toaru_plot_arc,
	.disc = toaru_plot_disc,
	.line = toaru_plot_line,
	.rectangle = toaru_plot_rectangle,
	.polygon = toaru_plot_polygon,
	.path = toaru_plot_path,
	.bitmap = toaru_plot_bitmap,
	.text = toaru_plot_text,
	.option_knockout = true,
};

static nserror gui_window_invalidate_area(struct gui_window *g, const struct rect *rect) {
	/* TODO: Flip */
	#if 0
	if (rect) {
		fprintf(stderr, "netsurf: invalidate window {x0=%d,y0=%d,x1=%d,y1=%d}\n", rect->x0, rect->y0, rect->x1, rect->y1);
	} else {
		fprintf(stderr, "netsurf: invalidate whole window?\n");
	}
	#endif

	redraw_next = 1;

	return NSERROR_OK;
}

static bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy) {
	/* TODO get scroll offset */
	*sx = g->scroll_x;
	*sy = g->scroll_y;

	return true;
}

static nserror gui_window_set_scroll(struct gui_window *g, const struct rect *rect) {
	if (rect) {
		g->scroll_x = rect->x0;
		g->scroll_y = rect->y0;

		int w, h;
		browser_window_get_extents(g->bw, true, &w, &h);

		if (g->scroll_x < 0) g->scroll_x = 0;
		if (g->scroll_y < 0) g->scroll_y = 0;

		if (g->scroll_x > w - (int)g->viewport_w) {
			g->scroll_x = w - (int)g->viewport_w;
		}

		if (g->scroll_y > h - (int)g->viewport_h) {
			g->scroll_y = h - (int)g->viewport_h;
		}

	}

	redraw_next = 1;

	return NSERROR_OK;
}

static void update_viewport (struct gui_window *gw) {
	decor_get_bounds(gw->win, &gw->bounds);

	gw->viewport_x = gw->bounds.left_width;
	gw->viewport_y = gw->bounds.top_height + MENU_BAR_HEIGHT + nav_bar_height;
	gw->viewport_w = gw->win->width - gw->bounds.width;
	gw->viewport_h = gw->win->height - gw->bounds.height - MENU_BAR_HEIGHT - nav_bar_height - STATUS_HEIGHT;
}

static nserror gui_window_get_dimensions(struct gui_window *gw, int *width, int *height) {
	/* TODO window size */

	update_viewport(gw);

	*width = gw->viewport_w;
	*height = gw->viewport_h;

	return NSERROR_OK;
}

static nserror gui_window_event(struct gui_window *gw, enum gui_window_event event) {
	/* TODO handle events from browser core */
	//fprintf(stderr, "netsurf: window event %d\n", (int)event);

	switch (event) {
		case GW_EVENT_UPDATE_EXTENT:
			/* Update the scroll sizes */
			break;
		case GW_EVENT_START_THROBBER:
			break;
		case GW_EVENT_STOP_THROBBER:
			break;
		case GW_EVENT_NEW_CONTENT:
			_button_disabled[0] = !browser_window_back_available(gw->bw);
			_button_disabled[1] = !browser_window_forward_available(gw->bw);
			break;
		default:
			break;
	}

	return NSERROR_OK;
}

static void gui_window_set_title(struct gui_window *g, const char *title) {
	/* TODO */
	free(g->title);
	asprintf(&g->title, "%s - Netsurf", title);
	redraw_next = 1;
}

static nserror gui_window_set_url(struct gui_window *g, nsurl *url) {
	/* TODO update toolbar */
	if (nav_bar) free(nav_bar);
	nav_bar = strdup(nsurl_access(url));
	return NSERROR_OK;
}

static void gui_window_set_status(struct gui_window *g, const char *text) {
	/* TODO status bar */
	free(g->window_status);
	g->window_status = strdup(text);
	redraw_next = 1;
}

static void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape) {
	int cursor_type = YUTANI_CURSOR_TYPE_RESET;
	switch (shape) {
	case GUI_POINTER_POINT:
		cursor_type = YUTANI_CURSOR_TYPE_POINT;
		break;

	case GUI_POINTER_CARET:
		cursor_type = YUTANI_CURSOR_TYPE_IBEAM;
		break;

	case GUI_POINTER_MENU:
		cursor_type = YUTANI_CURSOR_TYPE_RESET;
		break;

	case GUI_POINTER_PROGRESS:
		cursor_type = YUTANI_CURSOR_TYPE_RESET;
		break;

	case GUI_POINTER_MOVE:
		cursor_type = YUTANI_CURSOR_TYPE_DRAG;
		break;

	default:
		cursor_type = YUTANI_CURSOR_TYPE_RESET;
		break;
	}

	yutani_window_show_mouse(yctx, g->win, cursor_type);
}

static void gui_window_place_caret(struct gui_window *g, int x, int y, int height, const struct rect *clip) {
	/* TODO set caret position in browser? */
	g->caret_x = x;
	g->caret_y = y;
	g->caret_h = height;
}

static void gui_quit(void) {
	/* TODO */
}

static void resize_finish(struct gui_window *gw, int w, int h) {
	yutani_window_resize_accept(yctx, gw->win, w, h);
	reinit_graphics_yutani(gw->ctx, gw->win);
	if (clip_context) {
		free(clip_context);
		clip_context = NULL;
	}
	clip_x = 0;
	clip_y = 0;
	update_viewport(gw);
	browser_window_schedule_reformat(gw->bw);
	redraw_placeholder(gw);
	yutani_window_resize_done(yctx, gw->win);
	redraw_next = 0;
}

static void _figure_out_navbar_cursor(int x, struct decor_bounds bounds) {
	x = x - bounds.left_width - 2 - BUTTON_SPACE * BUTTON_COUNT - 5;
	if (x <= 0) {
		nav_bar_cursor_x = 0;
		return;
	}

	char * tmp = strdup(nav_bar);
	int candidate = 0;
	tt_set_size(tt_font_thin, 13);
	while (*tmp && x + 2 < (candidate = tt_string_width(tt_font_thin, tmp))) {
		tmp[strlen(tmp)-1] = '\0';
	}
	nav_bar_cursor_x = candidate;
	nav_bar_cursor = strlen(tmp);
	free(tmp);
}

static void nav_bar_set_focused(void) {
	nav_bar_focused = 1;
	nav_bar_blink = 0;
	gettimeofday(&nav_bar_last_blinked, NULL);
}

static void _recalculate_nav_bar_cursor(void) {
	if (nav_bar_cursor < 0) {
		nav_bar_cursor = 0;
	}
	if (nav_bar_cursor > (int)strlen(nav_bar)) {
		nav_bar_cursor = strlen(nav_bar);
	}
	char * tmp = strdup(nav_bar);
	tmp[nav_bar_cursor] = '\0';
	tt_set_size(tt_font_thin, 13);
	nav_bar_cursor_x = tt_string_width(tt_font_thin, tmp);
	free(tmp);
}

static void _redraw_nav_bar(void) {
	/* uh */
	redraw_next = 1;
}

/**
 * navbar: Text editing helpers for ^W, deletes one directory element
 */
static void nav_bar_backspace_word(void) {
	if (!*nav_bar) return;
	if (nav_bar_cursor == 0) return;

	char * after = strdup(&nav_bar[nav_bar_cursor]);

	if (nav_bar[nav_bar_cursor-1] == '/') {
		nav_bar[nav_bar_cursor-1] = '\0';
		nav_bar_cursor--;
	}
	while (nav_bar_cursor && nav_bar[nav_bar_cursor-1] != '/') {
		nav_bar[nav_bar_cursor-1] = '\0';
		nav_bar_cursor--;
	}

	strcat(nav_bar, after);
	free(after);

	_recalculate_nav_bar_cursor();
	nav_bar_set_focused();
	_redraw_nav_bar();
}

/**
 * navbar: Text editing helper for backspace, deletes one character
 */
static void nav_bar_backspace(void) {
	if (nav_bar_cursor == 0) return;

	char * after = strdup(&nav_bar[nav_bar_cursor]);

	nav_bar[nav_bar_cursor-1] = '\0';
	nav_bar_cursor--;

	strcat(nav_bar, after);
	free(after);

	_recalculate_nav_bar_cursor();
	nav_bar_set_focused();
	_redraw_nav_bar();
}

static void nav_bar_delete(void) {
	if (!nav_bar[nav_bar_cursor]) return;

	char * after = strdup(&nav_bar[nav_bar_cursor+1]);
	nav_bar[nav_bar_cursor] = '\0';

	strcat(nav_bar, after);
	free(after);

	_recalculate_nav_bar_cursor();
	nav_bar_set_focused();
	_redraw_nav_bar();
}

/**
 * navbar: Text editing helper for inserting characters
 */
static void nav_bar_insert_char(char c) {
	char * tmp;
	asprintf(&tmp, "%.*s%c%s", nav_bar_cursor, nav_bar, c, &nav_bar[nav_bar_cursor]);
	free(nav_bar);
	nav_bar = tmp;

	nav_bar_cursor += 1;
	_recalculate_nav_bar_cursor();
	nav_bar_set_focused();
	_redraw_nav_bar();
}

/**
 * navbar: Move editing cursor one character left
 */
static void nav_bar_cursor_left(int modifiers) {
	if (!*nav_bar) return;
	if (nav_bar_cursor == 0) return;

	if (modifiers & (KEY_MOD_LEFT_CTRL | KEY_MOD_RIGHT_CTRL)) {
		if (nav_bar[nav_bar_cursor-1] == '/') {
			nav_bar_cursor--;
		}
		while (nav_bar_cursor && nav_bar[nav_bar_cursor-1] != '/') {
			nav_bar_cursor--;
		}
	} else {
		nav_bar_cursor--;
	}
	_recalculate_nav_bar_cursor();
	nav_bar_set_focused();
	_redraw_nav_bar();
}

/**
 * navbar: Move editing cursor one character right
 */
static void nav_bar_cursor_right(int modifiers) {
	if (!*nav_bar) return;

	if (modifiers & (KEY_MOD_LEFT_CTRL | KEY_MOD_RIGHT_CTRL)) {
		if (nav_bar[nav_bar_cursor] == '/') {
			nav_bar_cursor++;
		}
		while (nav_bar[nav_bar_cursor] && nav_bar[nav_bar_cursor] != '/') {
			nav_bar_cursor++;
		}
	} else {
		nav_bar_cursor++;
	}

	_recalculate_nav_bar_cursor();
	nav_bar_set_focused();
	_redraw_nav_bar();
}


static void nav_bar_handle_keyboard(struct yutani_msg_key_event * ke) {
	struct gui_window * gw = hashmap_get(my_windows, (void*)(uintptr_t)ke->wid);
	switch (ke->event.key) {
		case KEY_ESCAPE:
			nav_bar_focused = 0;
			redraw_next = 1;
			break;
		case KEY_BACKSPACE:
			if (ke->event.modifiers & (KEY_MOD_LEFT_CTRL | KEY_MOD_RIGHT_CTRL)) {
				nav_bar_backspace_word();
			} else {
				nav_bar_backspace();
			}
			break;
		case KEY_CTRL_W:
			nav_bar_backspace_word();
			break;
		case '\n': {
			nsurl *url;
			nserror error = nsurl_create(nav_bar, &url);
			if (error == NSERROR_OK) {
				browser_window_navigate(gw->bw, url, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
				nsurl_unref(url);
			}
			nav_bar_focused = 0;
		}
		break;
		default:
			if (isgraph(ke->event.key)) {
				nav_bar_insert_char(ke->event.key);
			} else {
				switch (ke->event.keycode) {
					case KEY_ARROW_LEFT:
						nav_bar_cursor_left(ke->event.modifiers);
						break;
					case KEY_ARROW_RIGHT:
						nav_bar_cursor_right(ke->event.modifiers);
						break;
					case KEY_DEL:
						nav_bar_delete();
						break;
				}
			}
			break;
	}
}

static int inbounds(struct yutani_msg_window_mouse_event * me, struct gui_window *gw, int old) {
	int x = old ? me->old_x : me->new_x;
	int y = old ? me->old_y : me->new_y;
	if (x < gw->viewport_x || x >= gw->viewport_x + gw->viewport_w) return 0;
	if (y < gw->viewport_y || y >= gw->viewport_y + gw->viewport_h) return 0;
	return 1;
}

static int mouse_event(struct yutani_msg_window_mouse_event * me) {
	struct gui_window * gw = hashmap_get(my_windows, (void*)(uintptr_t)me->wid);
	if (!gw) return 0;

	browser_mouse_state mouse = 0;
	if (me->command == YUTANI_MOUSE_EVENT_DOWN) {
		if (!inbounds(me,gw,0)) return 0;
		mouse = BROWSER_MOUSE_PRESS_1;
	} else if (me->command == YUTANI_MOUSE_EVENT_CLICK) {
		if (!inbounds(me,gw,0)) return 0;
		mouse = BROWSER_MOUSE_CLICK_1;
	} else if (me->command == YUTANI_MOUSE_EVENT_DRAG) {
		if (!inbounds(me,gw,1)) return 0;
		mouse = BROWSER_MOUSE_DRAG_1;
	} else {
		if (!inbounds(me,gw,0)) return 0;
	}

	browser_window_mouse_track(gw->bw, mouse, me->new_x - gw->viewport_x + gw->scroll_x, me->new_y - gw->viewport_y + gw->scroll_y);
	return 1;
}

void toaru_mainloop(void) {

	while (1) {
		redraw_next = 0;
		int fds[1] = {fileno(yctx->sock)};
		int when = schedule_run();
		int index = fswait2(1,fds,redraw_next ? 0 : when);
		int redraw = 0;

		if (index == 0) {
			yutani_msg_t * m = yutani_poll(yctx);

			while (m) {
				if (menu_process_event(yctx, m)) redraw = 1;
				switch (m->type) {
					case YUTANI_MSG_WINDOW_FOCUS_CHANGE:
					{
						struct yutani_msg_window_focus_change * wf = (void*)m->data;
						yutani_window_t * win = hashmap_get(yctx->windows, (void*)(uintptr_t)wf->wid);
						if (hashmap_has(my_windows, (void*)(uintptr_t)wf->wid)) {
							win->focused = wf->focused;
							redraw = 1;
						}
					}
					break;
					case YUTANI_MSG_RESIZE_OFFER:
					{
						struct yutani_msg_window_resize * wr = (void*)m->data;
						if (hashmap_has(my_windows, (void*)(uintptr_t)wr->wid)) {
							resize_finish(hashmap_get(my_windows, (void*)(uintptr_t)wr->wid), wr->width, wr->height);
						}
					}
					break;
					case YUTANI_MSG_KEY_EVENT:
					{
						struct yutani_msg_key_event * ke = (void*)m->data;
						if (ke->event.action == KEY_ACTION_DOWN && hashmap_has(my_windows, (void*)(uintptr_t)ke->wid)) {
							if (nav_bar_focused) {
								nav_bar_handle_keyboard(ke);
								break;
							} else {
								struct gui_window * gw = hashmap_get(my_windows, (void*)(uintptr_t)ke->wid);
								switch (ke->event.keycode) {
									case KEY_ARROW_LEFT:
										if (browser_window_key_press(gw->bw, NS_KEY_LEFT) == false)
											gui_window_set_scroll(gw, &(struct rect){gw->scroll_x - 100, gw->scroll_y, gw->scroll_x - 100, gw->scroll_y});
										break;
									case KEY_ARROW_RIGHT:
										if (browser_window_key_press(gw->bw, NS_KEY_RIGHT) == false)
											gui_window_set_scroll(gw, &(struct rect){gw->scroll_x + 100, gw->scroll_y, gw->scroll_x + 100, gw->scroll_y});
										break;
									case KEY_ARROW_UP:
										if (browser_window_key_press(gw->bw, NS_KEY_UP) == false)
											gui_window_set_scroll(gw, &(struct rect){gw->scroll_x, gw->scroll_y - 100, gw->scroll_x, gw->scroll_y - 100});
										break;
									case KEY_ARROW_DOWN:
										if (browser_window_key_press(gw->bw, NS_KEY_DOWN) == false)
											gui_window_set_scroll(gw, &(struct rect){gw->scroll_x, gw->scroll_y + 100, gw->scroll_x, gw->scroll_y + 100});
										break;
									/* TODO input */
									default:
										if (ke->event.key) {
											browser_window_key_press(gw->bw, ke->event.key);
										}
										break;
								}
								redraw = 1;
							}
						}
					}
					break;
					case YUTANI_MSG_WINDOW_MOUSE_EVENT:
					{
						struct yutani_msg_window_mouse_event * me = (void*)m->data;
						yutani_window_t * win = hashmap_get(yctx->windows, (void*)(uintptr_t)me->wid);

						if (hashmap_has(my_windows, (void*)(uintptr_t)me->wid)) {
							int result = decor_handle_event(yctx, m);
							switch (result) {
								case DECOR_CLOSE:
									_menu_action_exit(NULL);
									break;
								case DECOR_RIGHT:
									/* right click in decoration, show appropriate menu */
									decor_show_default_menu(win, win->x + me->new_x, win->y + me->new_y);
									break;
								case DECOR_REDRAW:
									redraw = 1;
									break;
								default:
									/* Other actions */
									break;
							}

							menu_bar_mouse_event(yctx, win, &menu_bar, me, me->new_x, me->new_y);

							struct decor_bounds bounds;
							decor_get_bounds(win, &bounds);

							/* TODO other mouse events */
							if (nav_bar_height &&
								me->new_y > (int)(bounds.top_height + MENU_BAR_HEIGHT) &&
								me->new_y < (int)(bounds.top_height + MENU_BAR_HEIGHT + nav_bar_height) &&
								me->new_x > (int)(bounds.left_width) &&
								me->new_x < (int)(win->width - bounds.right_width)) {

								int x = me->new_x - bounds.left_width - 2;
								if (x >= 0) {
									int i = x / BUTTON_SPACE;
									if (i < BUTTON_COUNT) {
										if (me->command == YUTANI_MOUSE_EVENT_DOWN) {
											_set_hilight(i, 2);
											nav_bar_focused = 0;
											_down_button = i;
										} else if (me->command == YUTANI_MOUSE_EVENT_RAISE || me->command == YUTANI_MOUSE_EVENT_CLICK) {
											if (_down_button != -1 && _down_button == i) {
												_handle_button_press(i, me->wid);
												_set_hilight(i, 1);
											}
											_down_button = -1;
										} else {
											if (!(me->buttons & YUTANI_MOUSE_BUTTON_LEFT)) {
												_set_hilight(i, 1);
											} else {
												if (_down_button == i) {
													_set_hilight(i, 2);
												} else if (_down_button != -1) {
													_set_hilight(_down_button, 3);
												}
											}
										}
										if (win->mouse_state == YUTANI_CURSOR_TYPE_IBEAM) {
											yutani_window_show_mouse(yctx, win, YUTANI_CURSOR_TYPE_RESET);
										}
									} else {
										_set_hilight(-1,0);
										if (me->command == YUTANI_MOUSE_EVENT_DOWN) {
											nav_bar_set_focused();
											_figure_out_navbar_cursor(me->new_x, bounds);
											redraw = 1;
										}
										if (win->mouse_state == YUTANI_CURSOR_TYPE_RESET) {
											yutani_window_show_mouse(yctx, win, YUTANI_CURSOR_TYPE_IBEAM);
										}
									}
								}
							} else {
								if (win->mouse_state == YUTANI_CURSOR_TYPE_IBEAM) {
									yutani_window_show_mouse(yctx, win, YUTANI_CURSOR_TYPE_RESET);
								}
								if (me->command == YUTANI_MOUSE_EVENT_DOWN) {
									if (nav_bar_focused) {
										nav_bar_focused = 0;
										redraw = 1;
									}
								}
								if (_button_hover != -1) {
									_button_hilights[_button_hover] = 3;
									_button_hover = -1;
									redraw = 1; /* Double redraw ??? */
								}

								if (mouse_event(me)) {
									redraw = 1;
								}
							}
						}
					}
					break;

				}
				free(m);
				m = yutani_poll_async(yctx);
			}
		}


		if (redraw || redraw_next) {
			/* Uh */
			list_t * win_keys = hashmap_keys(my_windows);
			foreach(_key, win_keys) {
				struct gui_window * win = hashmap_get(my_windows, (void*)(uintptr_t)_key->value);
				redraw_placeholder(win);

			}

			list_free(win_keys);
			free(win_keys);
		}
	}
}

static struct gui_window_table _toaru_window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.invalidate = gui_window_invalidate_area,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.event = gui_window_event,

	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.set_status = gui_window_set_status,
	.set_pointer = gui_window_set_pointer,
	.place_caret = gui_window_place_caret,
};

struct gui_window_table *toaru_window_table = &_toaru_window_table;

static struct gui_misc_table _toaru_misc_table = {
	.schedule = toaru_schedule,

	.quit = gui_quit,
};

struct gui_misc_table *toaru_misc_table = &_toaru_misc_table;
