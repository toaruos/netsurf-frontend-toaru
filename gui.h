#pragma once

#include <toaru/yutani.h>
#include <toaru/decorations.h>

struct gui_window {
	struct browser_window *bw;
	yutani_window_t *win;
	gfx_context_t *ctx;

	char * title;
	char * window_status;

	struct decor_bounds bounds;

	int scroll_x;
	int scroll_y;

	int viewport_x;
	int viewport_y;
	int viewport_w;
	int viewport_h;

	int caret_x;
	int caret_y;
	int caret_h;
};

void toaru_preinit(void);
void toaru_mainloop(void);
extern const struct plotter_table toaru_plotters;

void toaru_draw_text(gfx_context_t * ctx,
		const struct plot_font_style *fstyle,
		int x,
		int y,
		const char *text,
		size_t length);

static inline uint32_t convert_ns_color(uint32_t c) {
	return rgb(_BLU(c),_GRE(c),_RED(c));
}
