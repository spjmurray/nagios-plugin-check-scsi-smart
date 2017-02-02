CXX=g++
CXXFLAGS=-O2 -Wall
LDFLAGS=-O2 -Wall
EXE=check_scsi_smart
SOURCE=$(wildcard *.cc)
OBJECT=$(patsubst %.cc,%.o,$(SOURCE))
PREFIX=/usr
LIBDIR=lib


all: $(EXE)

$(EXE): $(OBJECT)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECT)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

install:
	mkdir -p ${DESTDIR}${PREFIX}/${LIBDIR}/nagios/plugins
	install -m 0755 ${EXE} ${DESTDIR}${PREFIX}/${LIBDIR}/nagios/plugins

.PHONY: clean
clean:
	rm -f *.o
	rm -f $(EXE)

# vi: noet:
