CC = gcc
CFLAGS = -W -Wall -O2 -g
CPPFLAGS = -I.
LDLIBS = -lz
PROGS  = firmimg

CRAMFS_DIR = ${PWD}/cramfs
CRAMFSCK = $(CRAMFS_DIR)/cramfsck
MKCRAMFS = $(CRAMFS_DIR)/mkcramfs

DATA_DIR = ${PWD}/data
IDRACFS_DIR = ${PWD}/idracfs

all: $(PROGS)

cramfs:
	$(MAKE) -C ${PWD}/cramfs

pack: all
	@${PWD}/firmimg pack firmimg.d6

unpack: all cramfs
	@${PWD}/firmimg unpack firmimg.d6
	sudo rm -Rf $(IDRACFS_DIR)
	sudo $(CRAMFSCK) -x $(IDRACFS_DIR) $(DATA_DIR)/cramfs
	@echo "Firmware is unpacked !"

info: all
	@${PWD}/firmimg info firmimg.d6

help:
	@echo "make [COMMAND]"
	@echo ""
	@echo "command :"
	@echo "	all		Make firmimg program"
	@echo "	clean		Clean firmimg program"
	@echo "	pack		Pack cramfs of firmware image"
	@echo "	unpack		Unpack cramfs of firmware image"
	@echo "	info 		Show information of firmware image"
	@echo "	help		Show help"

distclean clean:
	rm -f $(PROGS)

.PHONY: all clean

