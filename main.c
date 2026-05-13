#include <errno.h>
#include <stdio.h>
#include "stdlib.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h> 


float smooth[3][3] = {{1.0/9.0, 1.0/9.0, 1.0/9.0},
                    {1.0/9.0, 1.0/9.0, 1.0/9.0}, 
                    {1.0/9.0, 1.0/9.0, 1.0/9.0}};


int main()
{
int* filter_option;
char* dataname_option;

printf("Enter a filename: \n");

scanf("%s", dataname_option);

printf("Choose a filter: 1: smooth 2: sharp 3: edge 4: emboss  \n");
scanf("%d", filter_option);

int fd = open(dataname_option , O_RDONLY);

if (fd == -1) {
printf("Error: Could not open file, code %n", errno);
exit(EXIT_FAILURE);
}

int count = 14;

char *buf = (char *) malloc((count + 1) * sizeof(char));
int ret = read(fd, buf, count);
buf[ret] = '\0';
printf("%s", buf);

// 24 Bit pro Pixel, ohne Farbpalette und ohne Komprimierung
bool validFile = true;

// Signatur: buf[0] = 0x42 ('B'), buf[1] = 0x4D ('M')
// BUGFIX: && muss || sein – einer der beiden Bytes falsch reicht aus
if (buf[0] != 0x42 || buf[1] != 0x4D) {
    printf("Error: File hat falsche Signatur.\n");
    validFile = false;
}

// Bits pro Pixel: Offset 28, 2 Bytes, Little-Endian
int bitsPerPixel = buf[28] | (buf[29] << 8);
if (bitsPerPixel != 24) {
    printf("Error: File hat nicht 24 Bit pro Pixel.\n");
    validFile = false;
}

// Kompressionsmethode: Offset 30, 4 Bytes, Little-Endian (0 = keine Komprimierung)
int compression = buf[30] | (buf[31] << 8) | (buf[32] << 16) | (buf[33] << 24);
if (compression != 0) {
    printf("Error: File ist komprimiert.\n");
    validFile = false;
}

// Farben in der Palette: Offset 46, 4 Bytes, Little-Endian (0 = keine Palette)
int paletteColors = buf[46] | (buf[47] << 8) | (buf[48] << 16) | (buf[49] << 24);
if (paletteColors != 0) {
    printf("Error: File hat Farbpalette.\n");
    validFile = false;
}

if (!validFile) {
    return 1;
}



return 0;
}