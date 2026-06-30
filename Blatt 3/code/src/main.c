#include "../include/rle.h"
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool use_optimized = false;

// Forward declarations
off_t get_file_size(int fd);
char *get_compressed_file_path(const char *filePath, bool optimized);
char *get_decompressed_file_path(const char *filePath);
bool is_optimized_file(const char *filePath);

// Type definitions
typedef void (*FirstOperation)(RLE *rle, const char *data, size_t size);
typedef char *(*SecondOperation)(RLE *rle, size_t *size);

// The operation enum
typedef enum { COMPRESS, DECOMPRESS, OPERATION_COUNT } Operation;

// These operations are used to fill up the RLE data structure
FirstOperation PrepareOutputOperation[OPERATION_COUNT] = {encode_rle,
                                                          deserialize_rle};

// These operations are used to create the final output data
SecondOperation FinalizeOutputOperation[OPERATION_COUNT] = {serialize_rle,
                                                            decode_rle};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <filepath> [operation] [--opt|-m]\n", argv[0]);
    printf("operation: '-d' for decompress, '-c' for compression (default)\n");
    printf("optional: '--opt' or '-m' to use optimized serialization\n");
    printf("For decompression: use --opt or -m to read optimized format\n");
    return 1;
  }

  Operation op = COMPRESS;
  bool opt_flag = false;

  // Prüfe optionale Argumente
  for (int i = 2; i < argc; ++i) {
    if (strcmp(argv[i], "-d") == 0) {
      op = DECOMPRESS;
    } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--opt") == 0) {
      opt_flag = true;
    }
  }

  char *path = argv[1];

  // Für Dekomprimierung: automatisch erkennen ob optimiert
  if (op == DECOMPRESS) {
    if (is_optimized_file(path)) {
      opt_flag = true;
      printf("Detected optimized file format\n");
    }
  }

  // Setze globales Flag für Serialisierung/Deserialisierung
  use_optimized = opt_flag;

  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    printf("Error: could not open file %s\n", path);
    printf("Error number: %d\n", errno);
    perror("Error message");
    return 1;
  }

  off_t size = get_file_size(fd);
  if (size == -1) {
    printf("Error: could not get file size\n");
    printf("Error number: %d\n", errno);
    perror("Error message");
    close(fd);
    return 1;
  }

  char *buffer = malloc(size * sizeof(char));
  if (!buffer && size > 0) {
    close(fd);
    return 1;
  }

  size_t bytes_to_read = (size_t)size;
  ssize_t bytes_read;
  bytes_read = read(fd, buffer, bytes_to_read);
  if (bytes_read != (ssize_t)bytes_to_read) {
    printf("Error: read file %s incomplete. Expected %zu, got %zd. \n", path,
           bytes_to_read, bytes_read);
    if (bytes_read == -1) {
      printf("Error number: %d\n", errno);
      perror("Error message");
    }
    free(buffer);
    close(fd);
    return 1;
  }
  close(fd);

  RLE *rle = create_rle();
  PrepareOutputOperation[op](rle, buffer, bytes_read);
  free(buffer);

  printf("RLE counts:\n");
  print_rle(rle, 1);

  // Generiere Ausgabedateinamen
  char *outPath;
  if (op == COMPRESS) {
    outPath = get_compressed_file_path(path, opt_flag);
  } else {
    outPath = get_decompressed_file_path(path);
  }

  fd = open(outPath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd == -1) {
    printf("Error: could not open output file %s\n", outPath);
    printf("Error number: %d\n", errno);
    perror("Error message");
    free(outPath);
    delete_rle(rle);
    return 1;
  }

  size_t bytes_to_write = 0;
  char *data = FinalizeOutputOperation[op](rle, &bytes_to_write);

  printf("Serialized data (%zu bytes): ", bytes_to_write);
  for (size_t i = 0; i < bytes_to_write; i++) {
    printf("%02x ", (unsigned char)data[i]);
  }
  printf("\n");

  ssize_t bytes_written;
  bytes_written = write(fd, data, bytes_to_write);
  if (bytes_written < (ssize_t)bytes_to_write) {
    printf("Error: write to file %s failed\n", outPath);
    if (bytes_written == -1) {
      printf("Error number: %d\n", errno);
      perror("Error message");
    }
    close(fd);
    free(data);
    free(outPath);
    delete_rle(rle);
    return 1;
  }
  close(fd);
  free(data);
  delete_rle(rle);

  printf("Done. Output written to: %s\n", outPath);
  printf("Mode: %s\n", opt_flag ? "OPTIMIZED" : "NORMAL");
  free(outPath);

  return 0;
}

off_t get_file_size(int fd) {
  struct stat buf;
  return (fstat(fd, &buf) < 0) ? -1 : buf.st_size;
}

char *get_compressed_file_path(const char *filePath, bool optimized) {
  const char *dot = strrchr(filePath, '.');
  size_t len = dot ? (size_t)(dot - filePath) : strlen(filePath);
  char *newPath;

  if (optimized) {
    newPath = (char *)malloc(len + 9); // + "_opt.mrl" + '\0'
    if (!newPath)
      return NULL;
    strncpy(newPath, filePath, len);
    newPath[len] = '\0';
    strcat(newPath, "_opt.mrl");
  } else {
    newPath = (char *)malloc(len + 5); // + ".mrl" + '\0'
    if (!newPath)
      return NULL;
    strncpy(newPath, filePath, len);
    newPath[len] = '\0';
    strcat(newPath, ".mrl");
  }
  return newPath;
}

char *get_decompressed_file_path(const char *filePath) {
  size_t len = strlen(filePath);

  if (len > 8 && strcmp(filePath + len - 8, "_opt.mrl") == 0) {
    size_t new_len = len - 8;
    char *newPath = (char *)malloc(new_len + 1);
    if (!newPath)
      return NULL;
    memcpy(newPath, filePath, new_len);
    newPath[new_len] = '\0';
    return newPath;
  } else if (len > 4 && strcmp(filePath + len - 4, ".mrl") == 0) {
    size_t new_len = len - 4;
    char *newPath = (char *)malloc(new_len + 1);
    if (!newPath)
      return NULL;
    memcpy(newPath, filePath, new_len);
    newPath[new_len] = '\0';
    return newPath;
  }
  return strdup(filePath);
}

bool is_optimized_file(const char *filePath) {
  size_t len = strlen(filePath);
  if (len < 8)
    return false;
  return strcmp(filePath + len - 8, "_opt.mrl") == 0;
}
