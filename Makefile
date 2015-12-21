CC=gcc
CFLAGS=-O2 -Wall
EXE=check_scsi_smart
SOURCE=check_scsi_smart.c

all: $(EXE)

$(EXE): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $<
