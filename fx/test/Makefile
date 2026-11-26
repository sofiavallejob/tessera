# offline tests, no Pd needed:  make -C test
CFLAGS ?= -O2
test: test_core test_fx
	./test_core
	./test_fx
test_core: test_core.c ../src/tspec_core.c ../src/tspec_core.h
	$(CC) -std=c99 $(CFLAGS) -Wall -D_POSIX_C_SOURCE=200809L -o $@ test_core.c ../src/tspec_core.c -lm
test_fx: test_fx.c ../src/fxkit.h ../src/grain.h ../src/modal.h ../src/datamod.h
	$(CC) -std=gnu11 $(CFLAGS) -Wall -Wno-unused-function -o $@ test_fx.c -lm
# the same tests under AddressSanitizer and UBSan
asan:
	$(CC) -std=gnu11 -g -fsanitize=address,undefined -Wno-unused-function -o test_fx_asan test_fx.c -lm
	./test_fx_asan
# the Pd wrapper without Pd (needs the Pd headers):  make pdstub PDINCLUDEDIR=/path/to/pd/src
pdstub:
	$(CC) -std=gnu11 -g -fsanitize=address,undefined -Wno-unused-function -I$(PDINCLUDEDIR) -DSETUP_FN=grain_tilde_setup -o fxpd_grain fxpd_test.c -lm
	ASAN_OPTIONS=detect_leaks=0 ./fxpd_grain
clean:
	rm -f test_core test_fx test_fx_asan fxpd_grain
