// blobby.c
// blob file archiver
// COMP1521 20T3 Assignment 2
// Written by <LEON QIAN>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// the first byte of every blobette has this value
#define BLOBETTE_MAGIC_NUMBER 0x42

// number of bytes in fixed-length blobette fields
#define BLOBETTE_MAGIC_NUMBER_BYTES 1
#define BLOBETTE_MODE_LENGTH_BYTES 3
#define BLOBETTE_PATHNAME_LENGTH_BYTES 2
#define BLOBETTE_CONTENT_LENGTH_BYTES 6
#define BLOBETTE_HASH_BYTES 1

// maximum number of bytes in variable-length blobette fields
#define BLOBETTE_MAX_PATHNAME_LENGTH 65535
#define BLOBETTE_MAX_CONTENT_LENGTH 281474976710655

// ADD YOUR #defines HERE
#define BLOBETTE_HASH_INITIAL_VALUE 0

typedef enum action { a_invalid, a_list, a_extract, a_create } action_t;

// typedef struct blobette {
//     mode_t file_mode;
//     uint16_t pathname_length;
//     uint64_t content_length;
//     uint8_t hash_byte;
// } blobette_t;

void usage(char *myname);
action_t process_arguments(int argc, char *argv[], char **blob_pathname,
                           char ***pathnames, int *compress_blob);

void list_blob(char *blob_pathname);
void extract_blob(char *blob_pathname);
void create_blob(char *blob_pathname, char *pathnames[], int compress_blob);

uint8_t blobby_hash(uint8_t hash, uint8_t byte);

// ADD YOUR FUNCTION PROTOTYPES HERE

/**
 * Create a new blobette.
 */
// blobette_t new_blobette(mode_t file_mode, uint16_t pathname_length,
//                         uint64_t content_length, uint8_t hash_byte) {
//     return (blobette_t){.file_mode = file_mode,
//                         .pathname_length = pathname_length,
//                         .content_length = content_length,
//                         .hash_byte = hash_byte};
// }

/**
 * Encode a blobette and write binary data to file stream.
 *
 * Return 0 on success,
 *        1 on failure.
 */
// int encode_blobette(FILE *stream, blobette_t blobette) {}

/**
 * Read binary data from file stream and decode a blobette.
 *
 * Return 0 on success,
 *        1 on failure.
 */
// int decode_blobette(blobette_t *blobette, FILE *stream) {}

/**
 * Read the next byte in the stream and update the hash.
 *
 * Return 0 on success,
 *        1 on failure.
 *
 * Note: Reaching EOF is considered a failure.
 */
int read_byte(FILE *stream, uint8_t *byte, uint8_t *hash) {
    int temp = fgetc(stream);
    if (temp == EOF) {
        return 1;
    }

    *byte = temp;
    if (hash) *hash = blobby_hash(*hash, *byte);
    return 0;
}

/**
 * Write the next byte to the stream and update the hash.
 *
 * Return 0 on success.
 */
int write_byte(uint8_t byte, FILE *stream, uint8_t *hash) {
    fputc(byte, stream);
    if (hash) *hash = blobby_hash(*hash, byte);
    return 0;
}

/**
 * Read a file into a blob recursively.
 * If file is dir, then call read_file on each file in dir.
 * If file is reg, then read the file into the blob.
 */
void read_file(char *pathname, FILE *blob_stream, int recursive) {
    uint8_t hash = BLOBETTE_HASH_INITIAL_VALUE;
    printf("Adding: %s\n", pathname);

    struct stat file_stat;
    if (stat(pathname, &file_stat)) {
        perror(pathname);
        exit(EXIT_FAILURE);
    }

    // put magic number
    write_byte(BLOBETTE_MAGIC_NUMBER, blob_stream, &hash);

    // put mode
    mode_t mode = file_stat.st_mode;
    write_byte(mode >> 16 & 0xFF, blob_stream, &hash);
    write_byte(mode >> 8 & 0xFF, blob_stream, &hash);
    write_byte(mode & 0xFF, blob_stream, &hash);

    // put pathname length
    uint16_t pathname_length = strlen(pathname);
    write_byte(pathname_length >> 8 & 0xFF, blob_stream, &hash);
    write_byte(pathname_length & 0xFF, blob_stream, &hash);

    // put content length
    uint64_t content_length = S_ISDIR(mode) ? 0 : file_stat.st_size;
    write_byte(content_length >> 40 & 0xFF, blob_stream, &hash);
    write_byte(content_length >> 32 & 0xFF, blob_stream, &hash);
    write_byte(content_length >> 24 & 0xFF, blob_stream, &hash);
    write_byte(content_length >> 16 & 0xFF, blob_stream, &hash);
    write_byte(content_length >> 8 & 0xFF, blob_stream, &hash);
    write_byte(content_length & 0xFF, blob_stream, &hash);

    // put pathname
    int ch;
    for (int j = 0; (ch = pathname[j]); ++j) write_byte(ch, blob_stream, &hash);

    // put content
    FILE *file_stream = fopen(pathname, "r");
    while ((ch = fgetc(file_stream)) != EOF) write_byte(ch, blob_stream, &hash);
    fclose(file_stream);

    // put hash
    write_byte(hash, blob_stream, NULL);

    // include everything inside dirs
    if (recursive && S_ISDIR(mode)) {
        // DIR *dirp = opendir(pathname);
        // if (!dirp) {
        //     perror(pathname);
        //     exit(EXIT_FAILURE);
        // }

        struct dirent **dirents;
        int n = scandir(pathname, &dirents, NULL, alphasort);
        if (n < 0) {
            perror(pathname);
            exit(EXIT_FAILURE);
        } else {
            for (int i = 0; i < n; ++i) {
                char *dir_name = dirents[i]->d_name;
                if (!strcmp(dir_name, ".")) continue;
                if (!strcmp(dir_name, "..")) continue;
                char *new_path = strdup(pathname);
                new_path = realloc(new_path, BLOBETTE_MAX_PATHNAME_LENGTH);
                if (new_path[strlen(new_path) - 1] != '/')
                    new_path = strcat(new_path, "/");
                new_path = strcat(new_path, dir_name);
                read_file(new_path, blob_stream, 1);
                free(new_path);
            }
        }

        // struct dirent *de;
        // while ((de = readdir(dirp))) {
        //     char *dir_name = de->d_name;
        //     if (!strcmp(dir_name, ".")) continue;
        //     if (!strcmp(dir_name, "..")) continue;
        //     char *new_path = strdup(pathname);
        //     new_path = realloc(new_path, BLOBETTE_MAX_PATHNAME_LENGTH);
        //     if (new_path[strlen(new_path) - 1] != '/')
        //         new_path = strcat(new_path, "/");
        //     new_path = strcat(new_path, dir_name);
        //     read_file(new_path, blob_stream, 1);
        //     free(new_path);
        // }

        // closedir(dirp);
    }
}

/**
 * Pack a blob file.
 *
 * On failure, the blob file will contain all the packed blobettes up to the
 * point of failure. Note that this file will be an invalid blob file and will
 * remain in the file system.
 */
void pack_blob(char *blob_pathname, char *pathnames[]) {
    FILE *blob_stream = fopen(blob_pathname, "w");

    char *pathname;
    for (int i = 0; (pathname = pathnames[i]); ++i) {
        char *sep_ptr = pathname;
        while ((sep_ptr = strchr(++sep_ptr, '/'))) {
            if (strlen(sep_ptr) == 1) break;
            *sep_ptr = '\0';
            read_file(pathname, blob_stream, 0);
            *sep_ptr = '/';
        }
        read_file(pathname, blob_stream, 1);
    }

    fclose(blob_stream);
}

/**
 * Write out the file.
 *
 * If the file is a directory, then call write_file on each file inside.
 */
void write_file(mode_t mode, uint16_t pathname_length, uint64_t content_length,
                char *pathname, FILE *blob_stream, uint8_t *byte,
                uint8_t *hash) {
    if (S_ISDIR(mode)) {
        struct stat dir_stat;
        if (stat(pathname, &dir_stat)) {
            if (errno == ENOENT) {
                // create directory with correct mode
                printf("Creating directory: %s\n", pathname);
                if (mkdir(pathname, mode)) {
                    perror(pathname);
                    exit(EXIT_FAILURE);
                }
            } else {
                perror(pathname);
                exit(EXIT_FAILURE);
            }
        } else {
            if (S_ISDIR(dir_stat.st_mode)) {
                // set the file mode for directory
                if (chmod(pathname, mode)) {
                    perror(pathname);
                    exit(EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "Can't create directory; file exists");
                exit(EXIT_FAILURE);
            }
        }
    } else {
        // create file and write content
        printf("Extracting: %s\n", pathname);
        FILE *file_stream = fopen(pathname, "w");

        for (int i = 0; i < content_length; ++i) {
            if (read_byte(blob_stream, byte, hash)) {
                fprintf(stderr, "ERROR: Incomplete content\n");
                exit(EXIT_FAILURE);
            }

            fputc(*byte, file_stream);
        }

        // set file mode
        if (chmod(pathname, mode)) {
            perror(pathname);
            exit(EXIT_FAILURE);
        }

        fclose(file_stream);
    }
}

/**
 * Unpack a blob file.
 *
 * At depth 0, only the metadata of the blobettes will be displayed and there
 * will be no side effects on failure.
 *
 * At depth 1, the content of the blobettes will be written to files and there
 * will be artefacts on failure, i.e any files that have already been written
 * before the point of failure will remain in the file system.
 */
void unpack_blob(char *blob_pathname, int depth) {
    // Open the blob.
    FILE *blob_stream = fopen(blob_pathname, "r");
    if (!blob_stream) {
        perror(blob_pathname);
        exit(EXIT_FAILURE);
    }

    uint8_t hash = BLOBETTE_HASH_INITIAL_VALUE;
    uint8_t byte;
    while (!read_byte(blob_stream, &byte, &hash)) {
        // check magic number
        if (byte != BLOBETTE_MAGIC_NUMBER) {
            fprintf(stderr, "ERROR: Magic byte of blobette incorrect\n");
            exit(EXIT_FAILURE);
        }

        // get mode
        mode_t mode = 0;
        for (int i = 0; i < BLOBETTE_MODE_LENGTH_BYTES; ++i) {
            if (read_byte(blob_stream, &byte, &hash)) {
                fprintf(stderr, "ERROR: Incomplete mode\n");
                exit(EXIT_FAILURE);
            }

            mode <<= 8;
            mode |= byte;
        }

        // get pathname length
        uint16_t pathname_length = 0;
        for (int i = 0; i < BLOBETTE_PATHNAME_LENGTH_BYTES; ++i) {
            if (read_byte(blob_stream, &byte, &hash)) {
                fprintf(stderr, "ERROR: Incomplete pathname length\n");
                exit(EXIT_FAILURE);
            }

            pathname_length <<= 8;
            pathname_length |= byte;
        }

        // get content length
        uint64_t content_length = 0;
        for (int i = 0; i < BLOBETTE_CONTENT_LENGTH_BYTES; ++i) {
            if (read_byte(blob_stream, &byte, &hash)) {
                fprintf(stderr, "ERROR: Incomplete content length\n");
                exit(EXIT_FAILURE);
            }

            content_length <<= 8;
            content_length |= byte;
        }

        // get pathname bytes
        char *pathname = malloc(sizeof(char) * (pathname_length + 1));
        for (int i = 0; i < pathname_length; ++i) {
            if (read_byte(blob_stream, &byte, &hash)) {
                free(pathname);
                fprintf(stderr, "ERROR: Incomplete pathname\n");
                exit(EXIT_FAILURE);
            }

            pathname[i] = byte;
        }
        pathname[pathname_length] = '\0';

        if (!depth) {
            printf("%06o %5lu %s\n", mode, content_length, pathname);
            free(pathname);
            if (fseek(blob_stream, content_length + BLOBETTE_HASH_BYTES,
                      SEEK_CUR)) {
                perror(blob_pathname);
                exit(EXIT_FAILURE);
            }
        } else {
            write_file(mode, pathname_length, content_length, pathname,
                       blob_stream, &byte, &hash);
            free(pathname);

            // get hash
            if (read_byte(blob_stream, &byte, NULL)) {
                fprintf(stderr, "ERROR: blob hash not found\n");
                exit(EXIT_FAILURE);
            }

            // check hash
            if (byte != hash) {
                fprintf(stderr, "ERROR: blob hash incorrect\n");
                exit(EXIT_FAILURE);
            }
        }

        // reset hash
        hash = BLOBETTE_HASH_INITIAL_VALUE;
    }

    fclose(blob_stream);
}

// YOU SHOULD NOT NEED TO CHANGE main, usage or process_arguments

int main(int argc, char *argv[]) {
    char *blob_pathname = NULL;
    char **pathnames = NULL;
    int compress_blob = 0;
    action_t action = process_arguments(argc, argv, &blob_pathname, &pathnames,
                                        &compress_blob);

    switch (action) {
        case a_list:
            list_blob(blob_pathname);
            break;

        case a_extract:
            extract_blob(blob_pathname);
            break;

        case a_create:
            create_blob(blob_pathname, pathnames, compress_blob);
            break;

        default:
            usage(argv[0]);
    }

    return 0;
}

// print a usage message and exit

void usage(char *myname) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\t%s -l <blob-file>\n", myname);
    fprintf(stderr, "\t%s -x <blob-file>\n", myname);
    fprintf(stderr, "\t%s [-z] -c <blob-file> pathnames [...]\n", myname);
    exit(1);
}

// process command-line arguments
// check we have a valid set of arguments
// and return appropriate action
// **blob_pathname set to pathname for blobfile
// ***pathname set to a list of pathnames for the create action
// *compress_blob set to an integer for create action

action_t process_arguments(int argc, char *argv[], char **blob_pathname,
                           char ***pathnames, int *compress_blob) {
    extern char *optarg;
    extern int optind, optopt;
    int create_blob_flag = 0;
    int extract_blob_flag = 0;
    int list_blob_flag = 0;
    int opt;
    while ((opt = getopt(argc, argv, ":l:c:x:z")) != -1) {
        switch (opt) {
            case 'c':
                create_blob_flag++;
                *blob_pathname = optarg;
                break;

            case 'x':
                extract_blob_flag++;
                *blob_pathname = optarg;
                break;

            case 'l':
                list_blob_flag++;
                *blob_pathname = optarg;
                break;

            case 'z':
                (*compress_blob)++;
                break;

            default:
                return a_invalid;
        }
    }

    if (create_blob_flag + extract_blob_flag + list_blob_flag != 1) {
        return a_invalid;
    }

    if (list_blob_flag && argv[optind] == NULL) {
        return a_list;
    } else if (extract_blob_flag && argv[optind] == NULL) {
        return a_extract;
    } else if (create_blob_flag && argv[optind] != NULL) {
        *pathnames = &argv[optind];
        return a_create;
    }

    return a_invalid;
}

/**
 * List the contents of the blob at the given path.
 * This function is called when the -l flag is set.
 *
 * blob_pathname    The path to the blob.
 */
void list_blob(char *blob_pathname) {
    // Unpack blob at depth 0, i.e. without extracting the contents.
    unpack_blob(blob_pathname, 0);
}

/**
 * Extract the contents of the blob at the given path.
 * This function is called when the -x flag is set.
 *
 * blob_pathname    The path to the blob.
 */
void extract_blob(char *blob_pathname) {
    // Unpack blob at depth 1, i.e. actually extracting the contents.
    unpack_blob(blob_pathname, 1);
}

/**
 * Create a blob at the given path, containing the specified files.
 * This function is called when the -c flag is set.
 *
 * The blob file will be compressed using xz if compression is set.
 * This function compresses blob if -z flag is set.
 *
 * blob_pathname    The path to the blob.
 * pathnames        The specified files as a NULL-terminated array of pathnames.
 * compress_blob    The compression flag: 0 off, non-0 on.
 */
void create_blob(char *blob_pathname, char *pathnames[], int compress_blob) {
    // printf("create_blob called to create %s blob '%s' containing:\n",
    //        compress_blob ? "compressed" : "non-compressed", blob_pathname);
    // for (int p = 0; pathnames[p]; p++) {
    //     printf("%s\n", pathnames[p]);
    // }

    // Pack blob.
    pack_blob(blob_pathname, pathnames);

    if (compress_blob) {
        // run xz here
    }
}

// ADD YOUR FUNCTIONS HERE

// YOU SHOULD NOT CHANGE CODE BELOW HERE

// Lookup table for a simple Pearson hash
// https://en.wikipedia.org/wiki/Pearson_hashing
// This table contains an arbitrary permutation of integers 0..255

const uint8_t blobby_hash_table[256] = {
    241, 18,  181, 164, 92,  237, 100, 216, 183, 107, 2,   12,  43,  246, 90,
    143, 251, 49,  228, 134, 215, 20,  193, 172, 140, 227, 148, 118, 57,  72,
    119, 174, 78,  14,  97,  3,   208, 252, 11,  195, 31,  28,  121, 206, 149,
    23,  83,  154, 223, 109, 89,  10,  178, 243, 42,  194, 221, 131, 212, 94,
    205, 240, 161, 7,   62,  214, 222, 219, 1,   84,  95,  58,  103, 60,  33,
    111, 188, 218, 186, 166, 146, 189, 201, 155, 68,  145, 44,  163, 69,  196,
    115, 231, 61,  157, 165, 213, 139, 112, 173, 191, 142, 88,  106, 250, 8,
    127, 26,  126, 0,   96,  52,  182, 113, 38,  242, 48,  204, 160, 15,  54,
    158, 192, 81,  125, 245, 239, 101, 17,  136, 110, 24,  53,  132, 117, 102,
    153, 226, 4,   203, 199, 16,  249, 211, 167, 55,  255, 254, 116, 122, 13,
    236, 93,  144, 86,  59,  76,  150, 162, 207, 77,  176, 32,  124, 171, 29,
    45,  30,  67,  184, 51,  22,  105, 170, 253, 180, 187, 130, 156, 98,  159,
    220, 40,  133, 135, 114, 147, 75,  73,  210, 21,  129, 39,  138, 91,  41,
    235, 47,  185, 9,   82,  64,  87,  244, 50,  74,  233, 175, 247, 120, 6,
    169, 85,  66,  104, 80,  71,  230, 152, 225, 34,  248, 198, 63,  168, 179,
    141, 137, 5,   19,  79,  232, 128, 202, 46,  70,  37,  209, 217, 123, 27,
    177, 25,  56,  65,  229, 36,  197, 234, 108, 35,  151, 238, 200, 224, 99,
    190};

// Given the current hash value and a byte
// blobby_hash returns the new hash value
//
// Call repeatedly to hash a sequence of bytes, e.g.:
// uint8_t hash = 0;
// hash = blobby_hash(hash, byte0);
// hash = blobby_hash(hash, byte1);
// hash = blobby_hash(hash, byte2);
// ...

uint8_t blobby_hash(uint8_t hash, uint8_t byte) {
    return blobby_hash_table[hash ^ byte];
}