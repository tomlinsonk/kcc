CC = clang
CFILES = kcc.c lexer.c parser.c emitter.c optimize.c semantics.c symbols.c types.c
HEADERS = lexer.h parser.h emitter.h optimize.h semantics.h symbols.h types.h

KIT_EMU ?= ../kit-emu
KICKASS ?= ../KickAssembler/KickAss.jar
EMU_SOURCES = $(wildcard $(KIT_EMU)/*.java)
RUN_TESTS = KIT_EMU=$(KIT_EMU) KICKASS=$(KICKASS) python3 run_tests.py

kcc: $(CFILES) $(HEADERS)
	$(CC) -o kcc $(CFILES)

build/kit-emu/KiT.class: $(EMU_SOURCES)
	mkdir -p build/kit-emu
	javac -d build/kit-emu $(EMU_SOURCES)

test: kcc build/kit-emu/KiT.class
	$(RUN_TESTS) $(if $(VERBOSE),-v) $(TESTS)

bench: kcc build/kit-emu/KiT.class
	$(RUN_TESTS) --bench $(TESTS)

bench-save: kcc build/kit-emu/KiT.class
	$(RUN_TESTS) --bench --save-baseline $(TESTS)

.PHONY: test bench bench-save
