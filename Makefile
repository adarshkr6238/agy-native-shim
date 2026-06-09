CC = aarch64-linux-gnu-gcc
CFLAGS = -shared -fPIC -O2
LDFLAGS = -ldl -Wl,-soname,agy_native_shim.so

.PHONY: all clean

all: bin/agy_native_shim.so

bin/agy_native_shim.so: src/agy_native_shim.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Build complete: $@"
	@file $@

clean:
	rm -f bin/agy_native_shim.so
