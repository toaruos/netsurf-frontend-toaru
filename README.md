# ToaruOS frontend for Netsurf

This is a ToaruOS native frontend for Netsurf, which uses ToaruOS's native windowing and graphics
libraries, including `libtoaru_text`. It does not use the native image format support, as that
would have been a more involved change to Netsurf.

You should be able to clone this into `netsurf/frontends/toaru` in a Netsurf 3.11 source distribution,
make minor changes to the parent `Makefile` to ensure `toaru` is listed as a valid target, and it should
work. You can also patch the config and nsoptions sources to add `toaru`, but this frontend doesn't
use any custom configuration options so that probably isn't necessary. You will still need to have
a build of `libcurl`, `libpng`, `libjpeg`, `libexpat`, `zlib` (or `libz`, which is preferred),
and `libmbedtls` at runtime, and a real `libiconv` is needed at least at build time (a static
build of `libiconv` is preferred over a dynamic one, which may interact poorly with our libc).

This frontend is based, in part, on the **File Browser** application in ToaruOS, which provided the
template for the navigation elements.
