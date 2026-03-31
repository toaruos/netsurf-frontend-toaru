#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "utils/nsoption.h"
#include "utils/utf8.h"
#include "netsurf/utf8.h"
#include "netsurf/layout.h"
#include "netsurf/plot_style.h"

#include <toaru/graphics.h>
#include <toaru/text.h>
#include <toaru/decodeutf8.h>

#include "toaru/gui.h"

struct TT_Table {
	off_t offset;
	size_t length;
};

struct TT_Coord {
	float x;
	float y;
};

struct TT_Line {
	struct TT_Coord start;
	struct TT_Coord end;
};

struct TT_Contour {
	size_t edgeCount;
	size_t nextAlloc;
	size_t flags;
	size_t last_start;
	struct TT_Line edges[];
};

struct TT_Intersection {
	float x;
	int affect;
};

struct TT_Edge {
	struct TT_Coord start;
	struct TT_Coord end;
	int direction;
};

struct TT_Shape {
	size_t edgeCount;
	int lastY;
	int startY;
	int lastX;
	int startX;
	struct TT_Edge edges[];
};

struct TT_Vertex {
	unsigned char flags;
	int x;
	int y;
};

struct TT_Font {
	int privFlags;
	FILE * filePtr;
	uint8_t * buffer;
	uint8_t * memPtr;

	struct TT_Table head_ptr;
	struct TT_Table cmap_ptr;
	struct TT_Table loca_ptr;
	struct TT_Table glyf_ptr;
	struct TT_Table hhea_ptr;
	struct TT_Table hmtx_ptr;
	struct TT_Table name_ptr;
	struct TT_Table os_2_ptr;

	off_t cmap_start;

	size_t cmap_maxInd;

	float scale;
	float emSize;

	int cmap_type;
	int loca_type;
};

extern struct TT_Font * tt_font_thin;
extern struct TT_Font * tt_font_bold;
extern struct TT_Font * tt_font_italic;
extern struct TT_Font * tt_font_bold_italic;
extern struct TT_Font * tt_font_mono;
extern struct TT_Font * tt_font_mono_bold;
extern struct TT_Font * tt_font_mono_italic;
extern struct TT_Font * tt_font_mono_bold_italic;

static struct TT_Font * choose_font(const plot_font_style_t *fstyle) {
	switch (fstyle->family) {
		case PLOT_FONT_FAMILY_MONOSPACE:
			if (fstyle->weight >= 700) {
				if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE)) return tt_font_mono_bold_italic;
				return tt_font_mono_bold;
			}
			if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE)) return tt_font_mono_italic;
			return tt_font_mono;

		default:
			if (fstyle->weight >= 700) {
				if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE)) return tt_font_bold_italic;
				return tt_font_bold;
			}
			if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE)) return tt_font_italic;
			return tt_font_thin;
	}
}

int tt_stringn_width(struct TT_Font * font, const char * s, size_t l);
struct TT_Contour * tt_prepare_stringn_into(struct TT_Contour * contour, struct TT_Font * font, float x, float y, const char * s, size_t l, float * out_width);
struct TT_Contour * tt_prepare_stringn(struct TT_Font * font, float x, float y, const char * s, size_t l, float * out_width);
int tt_draw_stringn(gfx_context_t * ctx, struct TT_Font * font, int x, int y, const char * s, size_t l, uint32_t color);

int tt_stringn_width(struct TT_Font * font, const char * s, size_t l) {
	float x_offset = 0;
	uint32_t cp = 0;
	uint32_t istate = 0;

	for (size_t i = 0; i < l; ++i) {
		if (s[i] == 0) continue;
		if (!decode(&istate, &cp, s[i])) {
			unsigned int glyph = tt_glyph_for_codepoint(font, cp);
			x_offset += tt_xadvance_for_glyph(font, glyph) * font->scale;
		}
	}

	return x_offset;
}


static nserror gui_font_width(const plot_font_style_t *fstyle, const char *string, size_t length, int *width) {
	if (!length) {
		*width = 0;
		return NSERROR_OK;
	}

	struct TT_Font * font = choose_font(fstyle);

	tt_set_size(font, plot_style_fixed_to_float(fstyle->size) * 1.2);
	*width = tt_stringn_width(font, string, length);

	return NSERROR_OK;
}

static nserror gui_font_position(const plot_font_style_t *fstyle,
		 const char *s,
		 size_t l,
		 int x,
		 size_t *char_offset,
		 int *actual_x)
{
	float x_offset = 0;
	uint32_t cp = 0;
	uint32_t istate = 0;

	struct TT_Font * font = choose_font(fstyle);

	tt_set_size(font, plot_style_fixed_to_float(fstyle->size) * 1.2);

	size_t i = 0;
	for (; i < l; ++i) {
		if (s[i] == 0) continue;
		if (!decode(&istate, &cp, s[i])) {
			unsigned int glyph = tt_glyph_for_codepoint(font, cp);
			float new_x = tt_xadvance_for_glyph(font, glyph) * font->scale;

			x_offset += new_x;

			if (x_offset >= x) {
				break;
			}
		}
	}

	*actual_x = (int)x_offset;
	*char_offset = i;

	return NSERROR_OK;
}

static nserror
gui_font_split(const plot_font_style_t *fstyle,
	      const char *s,
	      size_t l,
	      int x,
	      size_t *char_offset,
	      int *actual_x)
{

	float x_offset = 0;
	uint32_t cp = 0;
	uint32_t istate = 0;

	struct TT_Font * font = choose_font(fstyle);

	tt_set_size(font, plot_style_fixed_to_float(fstyle->size) * 1.2);

	float last_space = 0;
	int last_space_i = 0;

	for (size_t i = 0; i < l; ++i) {
		if (s[i] == 0) continue;
		if (!decode(&istate, &cp, s[i])) {
			unsigned int glyph = tt_glyph_for_codepoint(font, cp);
			float new_x = tt_xadvance_for_glyph(font, glyph) * font->scale;

			if (cp == ' ') {
				last_space = x_offset;
				last_space_i = i;
			}

			if (x_offset + new_x > x) {
				break;
			}

			x_offset += new_x;
		}
	}

	*actual_x = (int)last_space;
	*char_offset = last_space_i;

	return NSERROR_OK;
}

__attribute__((visibility("protected")))
struct TT_Contour * tt_prepare_stringn_into(struct TT_Contour * contour, struct TT_Font * font, float x, float y, const char * s, size_t l, float * out_width) {
	if (contour == NULL) {
		contour = tt_contour_start(0, 0);
	}

	float x_offset = x;
	uint32_t cp = 0;
	uint32_t istate = 0;

	for (size_t i = 0; i < l; ++i) {
		if (s[i] == 0) continue;
		if (!decode(&istate, &cp, s[i])) {
			unsigned int glyph = tt_glyph_for_codepoint(font, cp);
			contour = tt_draw_glyph_into(contour,font,x_offset,y,glyph);
			x_offset += tt_xadvance_for_glyph(font, glyph) * font->scale;
		}
	}

	if (out_width) *out_width = x_offset - x;

	return contour;
}

__attribute__((visibility("protected")))
struct TT_Contour * tt_prepare_stringn(struct TT_Font * font, float x, float y, const char * s, size_t l, float * out_width) {
	return tt_prepare_stringn_into(NULL, font, x, y, s, l, out_width);
}

int tt_draw_stringn(gfx_context_t * ctx, struct TT_Font * font, int x, int y, const char * s, size_t l, uint32_t color) {
	float width;
	struct TT_Contour * contour = tt_prepare_stringn(font,x,y,s,l,&width);
	if (contour->edgeCount) {
		struct TT_Shape * shape = tt_contour_finish(contour);
		tt_path_paint(ctx, shape, color);
		free(shape);
	}
	free(contour);
	return width;
}

void toaru_draw_text(gfx_context_t * gctx,
		const struct plot_font_style *fstyle,
		int x,
		int y,
		const char *text,
		size_t length) {

	struct TT_Font * font = choose_font(fstyle);

	tt_set_size(font, plot_style_fixed_to_float(fstyle->size) * 1.2);
	tt_draw_stringn(gctx, font,  x, y, text, length, convert_ns_color(fstyle->foreground));
}

static struct gui_layout_table layout_table = {
	.width = gui_font_width,
	.position = gui_font_position,
	.split = gui_font_split,
};

struct gui_layout_table *toaru_layout_table = &layout_table;
