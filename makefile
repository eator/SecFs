CC = gcc
CFLAGS = -g

INT = interface
SRCS = $(wildcard src/*.c) $(wildcard src/interface/*.c) 
BDIR = build

all:$(BDIR)/secfs

$(BDIR)/secfs: $(BDIR) $(BDIR)/mkfs $(BDIR)/fs.img $(SRCS)    
		$(CC) $(CFLAGS) -o $@ $(SRCS)    
		
$(BDIR):
	mkdir build

$(BDIR)/mkfs: src/mkfs/mkfs.c 
		$(CC) $(CFLAGS) -o $@ $^

$(BDIR)/fs.img: $(BDIR)/mkfs
		$< $@

import: 
	cp README.md build
	cp resource/* build
	python test/gen_test_seek_file.py
	mv Jerry build

.PHONY: clean
clean:
	rm -rf build
