#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

//altes alingment merken und auf 1 setzen, da header kein padding
#pragma pack(push, 1)

typedef struct {
    uint16_t signature;
    uint32_t fileSize;
    uint32_t reserved;
    uint32_t dataOffset;
} BitmapFileHeader;

typedef struct {
    uint32_t headerSize;
    int32_t width;
    int32_t height;
    uint16_t colorPlanes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    int32_t hResolution;
    int32_t vResolution;
    uint32_t paletteColors;
    uint32_t importantColors;
} BitmapInfoHeader;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} Pixel;

//ursprüngliches alingment zurücksetzen
#pragma pack(pop)

/* ================= FILTER ================= */

static float smooth[3][3] = {
    {1.0f/9, 1.0f/9, 1.0f/9},
    {1.0f/9, 1.0f/9, 1.0f/9},
    {1.0f/9, 1.0f/9, 1.0f/9}
};

static float sharp[3][3] = {
    { 0, -1,  0},
    {-1,  5, -1},
    { 0, -1,  0}
};

static float edge[3][3] = {
    { 0,  1,  0},
    { 1, -4,  1},
    { 0,  1,  0}
};

static float emboss[3][3] = {
    { 2,  1,  0},
    { 1,  1, -1},
    { 0, -1, -2}
};

/* ================= MAIN ================= */

int main()
{
    char filename[256];
    char filter_option[50];

    printf("Enter BMP filename:\n");
    scanf("%255s", filename);

    printf("Choose a filter: smooth, sharp, edge, emboss\n");
    scanf("%49s", filter_option);

    float (*kernel)[3] = NULL;

    if (strcmp(filter_option, "smooth") == 0)
        kernel = smooth;

    else if (strcmp(filter_option, "sharp") == 0)
        kernel = sharp;

    else if (strcmp(filter_option, "edge") == 0)
        kernel = edge;

    else if (strcmp(filter_option, "emboss") == 0)
        kernel = emboss;

    else {
        printf("Error: Unknown filter.\n");
        return 1;
    }

    /* ================= DATEI ÖFFNEN ================= */

    int fd = open(filename, O_RDONLY);

    if (fd == -1) {
        printf("Error opening file: %s\n", strerror(errno));
        return 1;
    }

    BitmapFileHeader fileHeader;
    BitmapInfoHeader infoHeader;

    /* ================= HEADER LESEN ================= */

    if (read(fd, &fileHeader, sizeof(fileHeader)) != sizeof(fileHeader)) {
        printf("Error reading file header.\n");
        close(fd);
        return 1;
    }

    if (read(fd, &infoHeader, sizeof(infoHeader)) != sizeof(infoHeader)) {
        printf("Error reading info header.\n");
        close(fd);
        return 1;
    }

    /* ================= VALIDIERUNG ================= */

    if (fileHeader.signature != 0x4D42) {
        printf("Error: Not a BMP file.\n");
        close(fd);
        return 1;
    }

    if (infoHeader.bitsPerPixel != 24) {
        printf("Error: Only 24-bit BMP supported.\n");
        close(fd);
        return 1;
    }

    if (infoHeader.compression != 0) {
        printf("Error: Compressed BMP not supported.\n");
        close(fd);
        return 1;
    }

    if (infoHeader.paletteColors != 0) {
        printf("Error: BMP with color palette not supported.\n");
        close(fd);
        return 1;
    }

    if (infoHeader.width < 3 || abs(infoHeader.height) < 3) {
        printf("Error: Image too small.\n");
        close(fd);
        return 1;
    }

    int width = infoHeader.width;
    int height = abs(infoHeader.height);

    printf("Width: %d\n", width);
    printf("Height: %d\n", height);

    /* ================= PADDING ================= */

    int padding = (4 - (width * 3) % 4) % 4;

    /* ================= SPEICHER ALLOKIEREN ================= */

    Pixel** image = malloc(height * sizeof(Pixel*));

    for (int y = 0; y < height; y++) {
        image[y] = malloc(width * sizeof(Pixel));
    }

    /* ================= ZU PIXELDATEN SPRINGEN ================= */

    lseek(fd, fileHeader.dataOffset, SEEK_SET);

    /* ================= PIXEL LESEN ================= */

    for (int y = 0; y < height; y++) {

        read(fd, image[y], width * sizeof(Pixel));

        lseek(fd, padding, SEEK_CUR);
    }

    close(fd);

    /* ================= AUSGABEBILD ================= */

    int newWidth = width - 2;
    int newHeight = height - 2;

    Pixel** output = malloc(newHeight * sizeof(Pixel*));

    for (int y = 0; y < newHeight; y++) {
        output[y] = malloc(newWidth * sizeof(Pixel));
    }

    /* ================= FILTER ANWENDEN ================= */

    for (int y = 0; y < newHeight; y++) {

        for (int x = 0; x < newWidth; x++) {

            float red = 0;
            float green = 0;
            float blue = 0;

            for (int ky = -1; ky <= 1; ky++) {

                for (int kx = -1; kx <= 1; kx++) {

                    Pixel p = image[y + 1 + ky][x + 1 + kx];

                    float factor = kernel[ky + 1][kx + 1];

                    red += p.red * factor;
                    green += p.green * factor;
                    blue += p.blue * factor;
                }
            }

            /* clamp */

            if (red < 0) red = 0;
            if (red > 255) red = 255;

            if (green < 0) green = 0;
            if (green > 255) green = 255;

            if (blue < 0) blue = 0;
            if (blue > 255) blue = 255;

            output[y][x].red = (uint8_t) red;
            output[y][x].green = (uint8_t) green;
            output[y][x].blue = (uint8_t) blue;
        }
    }

    /* ================= NEUE HEADER ================= */

    int newPadding = (4 - (newWidth * 3) % 4) % 4;

    infoHeader.width = newWidth;

    if (infoHeader.height > 0)
        infoHeader.height = newHeight;
    else
        infoHeader.height = -newHeight;

    infoHeader.imageSize =
        (newWidth * 3 + newPadding) * newHeight;

    fileHeader.fileSize =
        sizeof(BitmapFileHeader)
        + sizeof(BitmapInfoHeader)
        + infoHeader.imageSize;

    /* ================= AUSGABEDATEI ================= */

    int out = open(
        "output.bmp",
        O_CREAT | O_WRONLY | O_TRUNC,
        0644
    );

    if (out == -1) {
        printf("Error creating output file.\n");
        return 1;
    }

    /* ================= HEADER SCHREIBEN ================= */

    write(out, &fileHeader, sizeof(fileHeader));
    write(out, &infoHeader, sizeof(infoHeader));

    /* ================= PIXEL SCHREIBEN ================= */

    char pad[3] = {0, 0, 0};

    for (int y = 0; y < newHeight; y++) {

        write(out,
              output[y],
              newWidth * sizeof(Pixel));

        write(out, pad, newPadding);
    }

    close(out);

    /* ================= SPEICHER FREIGEBEN ================= */

    for (int y = 0; y < height; y++) {
        free(image[y]);
    }

    free(image);

    for (int y = 0; y < newHeight; y++) {
        free(output[y]);
    }

    free(output);

    printf("Filter applied successfully.\n");
    printf("Saved as output.bmp\n");

    return 0;
}