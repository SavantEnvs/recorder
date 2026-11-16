# Out-of-tree builds of recorder's objects plus the harnesses. Reuses upstream's Makefile for the
# feature flags, pkg-config libraries and object lists, so this follows whatever config.mk selects.
#   make -f mayhem/fuzz.mk OBJ=mayhem/obj-fuzz OUT=/mayhem EXTRA="<sanitizer+SanCov flags>" fuzz
#   make -f mayhem/fuzz.mk OBJ=mayhem/obj-test selftest
include Makefile

.DEFAULT_GOAL := fuzz

OBJ ?= mayhem/obj-fuzz
OUT ?= .
EXTRA ?=
LSAN_OFF_SRC ?= mayhem/lsan_off.c
HARNESSES := fuzz_json fuzz_handle_message fuzz_rec_file fuzz_codec

# recorder.c holds main(); the harnesses and the selftest bring their own entry point.
PROJ_OBJS := $(addprefix $(OBJ)/,$(OTR_OBJS) $(OTR_EXTRA_OBJS) recorder.o)

$(OBJ):
	mkdir -p $@

$(OBJ)/recorder.o: recorder.c config.mk | $(OBJ)
	$(CC) $(CFLAGS) $(EXTRA) -Dmain=recorder_main -c $< -o $@

$(OBJ)/%.o: %.c config.mk | $(OBJ)
	$(CC) $(CFLAGS) $(EXTRA) -c $< -o $@

$(OBJ)/h_%.o: mayhem/%.c mayhem/otr_fuzz.h config.mk | $(OBJ)
	$(CC) $(CFLAGS) $(EXTRA) -I. -c $< -o $@

$(OBJ)/lsan_off.o: $(LSAN_OFF_SRC) | $(OBJ)
	$(CC) $(EXTRA) -c $< -o $@

$(OBJ)/standalone_main.o: | $(OBJ)
	$(CC) $(EXTRA) -x c -c $(STANDALONE_FUZZ_MAIN) -o $@

HBINS := $(addprefix $(OUT)/,$(HARNESSES))
SBINS := $(addsuffix -standalone,$(HBINS))

$(HBINS): $(OUT)/%: $(OBJ)/h_%.o $(PROJ_OBJS) $(OBJ)/lsan_off.o
	$(CC) $(CFLAGS) $(EXTRA) $(LIB_FUZZING_ENGINE) $^ $(LIBS) -o $@

$(SBINS): $(OUT)/%-standalone: $(OBJ)/h_%.o $(PROJ_OBJS) $(OBJ)/lsan_off.o $(OBJ)/standalone_main.o
	$(CC) $(CFLAGS) $(EXTRA) $^ $(LIBS) -o $@

fuzz: $(HBINS) $(SBINS)

$(OBJ)/h_selftest.o: mayhem/selftest.c mayhem/otr_fuzz.h config.mk | $(OBJ)
	$(CC) $(CFLAGS) $(EXTRA) -I. -c $< -o $@

selftest: $(OBJ)/h_selftest.o $(PROJ_OBJS)
	$(CC) $(CFLAGS) $(EXTRA) $^ $(LIBS) -o $(OBJ)/selftest

.PHONY: fuzz selftest
