CC=g++
CFLAGS=-O2 -Wall
EXE=check_scsi_smart
SOURCE=check_scsi_smart.cc
PREFIX=/usr
LIBDIR=lib


all: $(EXE)

$(EXE): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $<

install:
	mkdir -p ${DESTDIR}${PREFIX}/${LIBDIR}/nagios/plugins
	install -m 0755 ${EXE} ${DESTDIR}${PREFIX}/${LIBDIR}/nagios/plugins

.PHONY: clean
clean:
	rm -f $(EXE)
