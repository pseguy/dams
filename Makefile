#
#  DAMS - An integrated Editor/Assembler/debugger for the Amstrad CPC.
#  Copyright (C) 1984-2015  Pascal Séguy
#

CXXFLAGS= -g -Wall

all: damsdecode damsman-fr.html damsman-fr.html



install: all
	sudo install -m775 damsdecode /usr/local/bin/

D1.BIN: damsdecode dams.dams
	./damsdecode -e -o D1.BIN <dams.dams

bin: D1.BIN

dams.dsk: D1.BIN D2.BIN D3.BIN DAMS.BAS DAMS.BIN
	iDSK $@ -n
	iDSK $@ -i DAMS.BAS -t 1
	iDSK $@ -i DAMS.BIN -t 1
	iDSK $@ -i D1.BIN -t 1
	iDSK $@ -i D2.BIN -t 1
	iDSK $@ -i D3.BIN -t 1


damsman-fr.html: damsman-fr.adoc
	asciidoc --unsafe --attribute=sgml -a icons damsman-fr.adoc

damsman-en.html: damsman-en.adoc
	asciidoc --unsafe --attribute=sgml -a icons damsman-en.adoc

clean:
	rm -f damsdecode

clobber: clean
	rm -f D1.BIN D2.BIN D3.BIN
	rm -f dams.dsk
	rm -f damsman-fr.html
	rm -f damsman-en.html
