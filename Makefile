CC=gcc
CFLAGS=-O3 -g -fomit-frame-pointer -Isrc -Itool -Itool/zlib -DNDEBUG
OBJDIR=obj
LDFLAGS=

$(OBJDIR)/%.o: src/../%.c
	@mkdir -p '$(@D)'
	$(CC) $(CFLAGS) -c $< -o $@

APP := zultra

OBJS := $(OBJDIR)/src/blockdeflate.o
OBJS += $(OBJDIR)/src/dictionary.o
OBJS += $(OBJDIR)/src/frame.o
OBJS += $(OBJDIR)/src/libzultra.o
OBJS += $(OBJDIR)/src/matchfinder.o
OBJS += $(OBJDIR)/src/huffman/bitwriter.o
OBJS += $(OBJDIR)/src/huffman/huffencoder.o
OBJS += $(OBJDIR)/src/huffman/huffutils.o
OBJS += $(OBJDIR)/src/libdivsufsort/lib/divsufsort.o
OBJS += $(OBJDIR)/src/libdivsufsort/lib/sssort.o
OBJS += $(OBJDIR)/src/libdivsufsort/lib/trsort.o
OBJS += $(OBJDIR)/src/libdivsufsort/lib/divsufsort_utils.o
OBJS += $(OBJDIR)/tool/zultra.o
OBJS += $(OBJDIR)/tool/zlib/adler32.o
OBJS += $(OBJDIR)/tool/zlib/compress.o
OBJS += $(OBJDIR)/tool/zlib/crc32.o
OBJS += $(OBJDIR)/tool/zlib/deflate.o
OBJS += $(OBJDIR)/tool/zlib/gzclose.o
OBJS += $(OBJDIR)/tool/zlib/gzlib.o
OBJS += $(OBJDIR)/tool/zlib/gzread.o
OBJS += $(OBJDIR)/tool/zlib/gzwrite.o
OBJS += $(OBJDIR)/tool/zlib/infback.o
OBJS += $(OBJDIR)/tool/zlib/inffast.o
OBJS += $(OBJDIR)/tool/zlib/inflate.o
OBJS += $(OBJDIR)/tool/zlib/inftrees.o
OBJS += $(OBJDIR)/tool/zlib/trees.o
OBJS += $(OBJDIR)/tool/zlib/uncompr.o
OBJS += $(OBJDIR)/tool/zlib/zutil.o

all: $(APP)

$(APP): $(OBJS)
	@mkdir -p ../../bin/posix
	$(CC) $^ $(LDFLAGS) -o $(APP)

clean:
	@rm -rf $(APP) $(OBJDIR)

