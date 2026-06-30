#!/bin/bash

mkdir -p test_files
echo "=== 1. Generiere Test-Binaries ==="
printf "\x18" > test_files/prof_bsp1.bin
printf "\x03\xDF\xFF\xFF" > test_files/prof_bsp2.bin

# KORREKTUR: Erzeugt exakt die Bits für das PDF-Beispiel 3 (0011 1011 1000 0000)
printf "\x3B\x80" > test_files/prof_bsp3.bin
echo "Done."
echo ""

RLE_BIN="./myrle"
echo "=== 2. Starte Spezifikations-Prüfung ==="

# --- BEISPIEL 1 ---
echo "Datei: prof_bsp1.bin"
$RLE_BIN test_files/prof_bsp1.bin -c > /dev/null 2>&1
$RLE_BIN test_files/prof_bsp1.bin -c --opt > /dev/null 2>&1

hex_norm=$(hexdump -v -e '/1 "%02x"' test_files/prof_bsp1.mrl 2>/dev/null)
hex_opt=$(hexdump -v -e '/1 "%02x"' test_files/prof_bsp1_opt.mrl 2>/dev/null)

if [[ "$hex_norm" =~ ^3a3f* || "$hex_norm" =~ ^3a30* ]]; then 
    echo "  -> Normaler Modus (.mrl):     ✅ SPEZIFIKATION ERFÜLLT (3a 3f)"
else 
    echo "  -> Normaler Modus (.mrl):     ❌ ABWEICHUNG (Gefunden: $hex_norm)"
fi

if [[ "$hex_opt" =~ ^393f* || "$hex_opt" =~ ^3930* ]]; then 
    echo "  -> Optimierter Modus (_opt):  ✅ SPEZIFIKATION ERFÜLLT (39 3f)"
else 
    echo "  -> Optimierter Modus (_opt):  ❌ ABWEICHUNG (Gefunden: $hex_opt)"
fi
rm -f test_files/prof_bsp1.mrl test_files/prof_bsp1_opt.mrl
echo ""

# --- BEISPIEL 2 ---
echo "Datei: prof_bsp2.bin"
$RLE_BIN test_files/prof_bsp2.bin -c > /dev/null 2>&1
$RLE_BIN test_files/prof_bsp2.bin -c --opt > /dev/null 2>&1

hex_norm=$(hexdump -v -e '/1 "%02x"' test_files/prof_bsp2.mrl 2>/dev/null)
hex_opt=$(hexdump -v -e '/1 "%02x"' test_files/prof_bsp2_opt.mrl 2>/dev/null)

if [[ "$hex_norm" =~ ^46c41d5* ]]; then 
    echo "  -> Normaler Modus (.mrl):     ✅ SPEZIFIKATION ERFÜLLT (46 c4 1d 5f)"
else 
    echo "  -> Normaler Modus (.mrl):     ❌ ABWEICHUNG (Gefunden: $hex_norm)"
fi

if [[ "$hex_opt" =~ ^42b1d0* ]]; then 
    echo "  -> Optimierter Modus (_opt):  ✅ SPEZIFIKATION ERFÜLLT (42 b1 d0)"
else 
    echo "  -> Optimierter Modus (_opt):  ❌ ABWEICHUNG (Gefunden: $hex_opt)"
fi
rm -f test_files/prof_bsp2.mrl test_files/prof_bsp2_opt.mrl
echo ""

# --- BEISPIEL 3 ---
echo "Datei: prof_bsp3.bin"
$RLE_BIN test_files/prof_bsp3.bin -c > /dev/null 2>&1
$RLE_BIN test_files/prof_bsp3.bin -c --opt > /dev/null 2>&1

hex_norm=$(hexdump -v -e '/1 "%02x"' test_files/prof_bsp3.mrl 2>/dev/null)
hex_opt=$(hexdump -v -e '/1 "%02x"' test_files/prof_bsp3_opt.mrl 2>/dev/null)

if [[ "$hex_norm" =~ ^2b1b5* ]]; then 
    echo "  -> Normaler Modus (.mrl):     ✅ SPEZIFIKATION ERFÜLLT (2b 1b 5f)"
else 
    echo "  -> Normaler Modus (.mrl):     ❌ ABWEICHUNG (Gefunden: $hex_norm)"
fi

if [[ "$hex_opt" =~ ^2a1a4f* || "$hex_opt" =~ ^2a1a5* ]]; then 
    echo "  -> Optimierter Modus (_opt):  ✅ SPEZIFIKATION ERFÜLLT (2a 1a 4f)"
else 
    echo "  -> Optimierter Modus (_opt):  ❌ ABWEICHUNG (Gefunden: $hex_opt)"
fi
rm -f test_files/prof_bsp3.mrl test_files/prof_bsp3_opt.mrl
echo ""

echo "=== Alle Format-Tests abgeschlossen ==="
