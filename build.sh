#!/bin/bash

echo "Building libblake3.so"

gcc -shared -O3 -o libblake3.so blake3.c blake3_dispatch.c blake3_portable.c blake3_sse2_x86-64_unix.S blake3_sse41_x86-64_unix.S blake3_avx2_x86-64_unix.S blake3_avx512_x86-64_unix.S

echo "copy libblake3.so to user lib as root"

sudo cp libblake3.so /usr/local/lib/

echo "reload library config as root"

sudo ldconfig

echo "Building wombatfileforensics"

g++ -O3 -o wombatfileforensics common.cpp xfs.cpp ntfs.cpp fatfs.cpp extfs.cpp wombatfileforensics.cpp -lpthread -L. -lblake3
