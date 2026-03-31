#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <toaru/yutani.h>
#include <toaru/graphics.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "netsurf/bitmap.h"
#include "netsurf/plotters.h"
#include "netsurf/content.h"

struct extended_sprite {
	sprite_t internal;
	int converted;
};

static void *toaru_bitmap_create(int width, int height, unsigned int state) {
	struct extended_sprite * s = calloc(1, sizeof(struct extended_sprite));

	s->internal.width = width;
	s->internal.height = height;
	s->internal.bitmap = calloc(width * height, sizeof(uint32_t));
	s->internal.masks = NULL;
	s->internal.blank = 0;
	s->internal.alpha = ALPHA_EMBEDDED;

	s->converted = 0;

	return s;
}

static void toaru_bitmap_destroy(void *bitmap) {
	struct extended_sprite * s = bitmap;
	free(s->internal.bitmap);
	free(s);
}

static unsigned char *toaru_bitmap_get_buffer(void *bitmap) {
	struct extended_sprite * s = bitmap;
	if (!s->converted) return (unsigned char *)s->internal.bitmap;

	/* We only run on little-endian platforms */
	for (int y = 0; y < s->internal.height; ++y) {
		for (int x = 0; x < s->internal.width; ++x) {
			uint32_t c = s->internal.bitmap[x + y * s->internal.width];
			uint32_t r = _RED(c);
			uint32_t g = _GRE(c);
			uint32_t b = _BLU(c);
			uint32_t a = _ALP(c);

			if (a == 0) {
				r = 0;
				g = 0;
				b = 0;
			} else {
				/* unpremultiply */
				r = (r << 8) / a;
				g = (g << 8) / a;
				b = (b << 8) / a;
			}

			s->internal.bitmap[x + y * s->internal.width] = rgba(b,g,r,a);
		}
	}

	s->converted = 1;

	return (unsigned char*)s->internal.bitmap;
}

static size_t toaru_bitmap_get_rowstride(void *bitmap) {
	struct extended_sprite * s = bitmap;
	return 4 * s->internal.width;
}

static bool toaru_bitmap_save(void *bitmap, const char *path, unsigned flags) { return true; } /* unsupported */

static void toaru_bitmap_modified(void *bitmap) {
	struct extended_sprite * s = bitmap;
	if (s->converted) return; /* oopsy */

	for (int y = 0; y < s->internal.height; ++y) {
		for (int x = 0; x < s->internal.width; ++x) {
			uint32_t c = s->internal.bitmap[x + y * s->internal.width];
			uint32_t r = _RED(c);
			uint32_t g = _GRE(c);
			uint32_t b = _BLU(c);
			uint32_t a = _ALP(c);

			s->internal.bitmap[x + y * s->internal.width] = premultiply(rgba(b,g,r,a));
		}
	}
	s->converted = 0;
}

static void toaru_bitmap_set_opaque(void *bitmap, bool opaque) {
	struct extended_sprite * s = bitmap;
	s->internal.alpha = opaque ? ALPHA_OPAQUE : ALPHA_EMBEDDED;
}

static bool toaru_bitmap_test_opaque(void *bitmap) {
	struct extended_sprite * s = bitmap;
	for (int y = 0; y < s->internal.height; ++y) {
		for (int x = 0; x < s->internal.width; ++x) {
			uint32_t c = s->internal.bitmap[x + y * s->internal.width];
			if (_ALP(c) != 0xFF) return false;
		}
	}

	return true;
}

static bool toaru_bitmap_get_opaque(void *bitmap) {
	struct extended_sprite * s = bitmap;
	if (s->internal.alpha == ALPHA_OPAQUE) return true;
	return false;
}

static int toaru_bitmap_get_width(void *bitmap) {
	struct extended_sprite * s = bitmap;
	return s->internal.width;
}

static int toaru_bitmap_get_height(void *bitmap) {
	struct extended_sprite * s = bitmap;
	return s->internal.height;
}

static size_t toaru_bitmap_get_bpp(void *bitmap) {
	return 4;
}

static nserror toaru_bitmap_render(struct bitmap *bitmap, struct hlcache_handle *content) {
	/* TODO */
	return NSERROR_OK;
}

static struct gui_bitmap_table _toaru_bitmap_table = {
	.create = toaru_bitmap_create,
	.destroy = toaru_bitmap_destroy,
	.set_opaque = toaru_bitmap_set_opaque,
	.get_opaque = toaru_bitmap_get_opaque,
	.get_buffer = toaru_bitmap_get_buffer,
	.get_rowstride = toaru_bitmap_get_rowstride,
	.get_width = toaru_bitmap_get_width,
	.get_height = toaru_bitmap_get_height,
	.modified = toaru_bitmap_modified,
	.render = toaru_bitmap_render,
#if (NSVERSION_MIN < 11)
	.test_opaque = toaru_bitmap_test_opaque,
	.get_bpp = toaru_bitmap_get_bpp,
	.save = toaru_bitmap_save,
#endif
};

struct gui_bitmap_table *toaru_bitmap_table = &_toaru_bitmap_table;
