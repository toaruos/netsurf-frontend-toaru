
CFLAGS += -std=c99 -g -Dnstoaru -Dsmall

CFLAGS += -DNSVERSION_MIN="$(shell sed -n '/_minor/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)"

NSTOARU_RESOURCES_DIR := $(FRONTEND_RESOURCES_DIR)

LDFLAGS += -ltoaru_menu -ltoaru_yutani -ltoaru_decorations -ltoaru_button -ltoaru_text -ltoaru_graphics -ltoaru_hashmap -ltoaru_list

EXETARGET := netsurf

MESSAGES_FILTER=toaru
MESSAGES_TARGET=$(NSTOARU_RESOURCES_DIR)

S_FRONTEND = main.c fetch.c gui.c bitmap.c clipboard.c layout.c schedule.c

SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_FRONTEND)

install-toaru:
	$(VQ)echo " INSTALL: $(DESTDIR)/$(PREFIX)"
	$(Q)$(INSTALL) -d $(DESTDIR)/$(NETSURF_TOARU_BIN)
	$(Q)$(INSTALL) -T $(EXETARGET) $(DESTDIR)/$(NETSURF_TOARU_BIN)/netsurf-toaru
	$(Q)$(INSTALL) -d $(DESTDIR)/$(NETSURF_TOARU_RESOURCES)
	$(Q)for F in $(NETSURF_TOARU_RESOURCE_LIST); do $(INSTALL) -m 644 $(FRONTEND_RESOURCES_DIR)/$$F $(DESTDIR)/$(NETSURF_TOARU_RESOURCES); done
	$(Q)$(RM) $(DESTDIR)/$(NETSURF_TOARU_RESOURCES)/Messages
	$(Q)$(SPLIT_MESSAGES) -l en -p fb -f messages -o $(DESTDIR)/$(NETSURF_TOARU_RESOURCES)/Messages -z resources/FatMessages

package-toaru:
