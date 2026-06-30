#!/bin/bash

mkdir -p test_files
echo "=== 1. Generiere Test-Binaries ==="
printf "\x18" > test_files/prof_bsp1.bin
printf "\x03\xDF\xFF\xFF" > test_files/prof_bsp2.bin
printf "\x3B\x80\x00\x00" > test_files/prof_bsp3.bin

if command -v python3 &>/dev/null; then
    python3 -c 'print("\x00" * 250, end="")' > test_files/2000_zeros.bin
    python3 -c 'print("\xFF" * 250, end="")' > test_files/2000_ones.bin
else
    perl -e 'print "\x00" x 250' > test_files/2000_zeros.bin
    perl -e 'print "\xFF" x 250' > test_files/2000_ones.bin
fi
printf "\x55\x55\x55\x55" > test_files/pattern_55.bin
echo "Done."
echo ""

RLE_BIN="./myrle"
echo "=== 2. Starte Kompression & Validierung ==="

for orig in test_files/*.bin; do
    filename=$(basename "$orig")
    base_no_ext="test_files/${filename%.bin}"
    
    echo "Datei: $filename"
    orig_md5=$(md5sum "$orig" | awk '{print $1}')
    
    # --- TEST NORMAL-MODUS ---
    $RLE_BIN "$orig" -c
    
    # Deine main dekomprimiert "file.mrl" zu "file" (ohne .bin)
    $RLE_BIN "${base_no_ext}.mrl" -d
    
    dec_normal_md5=$(md5sum "${base_no_ext}" 2>/dev/null | awk '{print $1}')
    rm -f "${base_no_ext}" "${base_no_ext}.mrl"
    
    if [ "$orig_md5" == "$dec_normal_md5" ] && [ -n "$dec_normal_md5" ]; then
        echo "  -> Normal-Modus (.mrl):      ✅ ERFOLGREICH"
    else
        echo "  -> Normal-Modus (.mrl):      ❌ FEHLGESCHLAGEN!"
    fi
    
    # --- TEST OPTIMIERT-MODUS ---
    $RLE_BIN "$orig" -c --opt
    
    $RLE_BIN "${base_no_ext}_opt.mrl" -d
    
    dec_opt_md5=$(md5sum "${base_no_ext}" 2>/dev/null | awk '{print $1}')
    rm -f "${base_no_ext}" "${base_no_ext}_opt.mrl"
    
    if [ "$orig_md5" == "$dec_opt_md5" ] && [ -n "$dec_opt_md5" ]; then
        echo "  -> Optimierter Modus (_opt):  ✅ ERFOLGREICH"
    else
        echo "  -> Optimierter Modus (_opt):  ❌ FEHLGESCHLAGEN!"
    fi
    echo ""
done
echo "=== Alle Tests abgeschlossen ==="
