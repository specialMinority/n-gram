#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAR_COUNT 256

/*
 * This program counts 1-grams by C character byte.
 *
 * Important limitation:
 * - English letters and ASCII symbols are counted as expected.
 * - UTF-8 text such as Korean is stored as multiple bytes, so one Korean
 *   syllable is counted as several byte values, not as one visual character.
 */

int main(int argc, char *argv[])
{
    FILE *input_file;
    size_t counts[CHAR_COUNT] = {0};
    int order[CHAR_COUNT];
    int used_count = 0;
    int ch;
    int i;
    int j;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    input_file = fopen(argv[1], "rb");
    if (input_file == NULL) {
        fprintf(stderr, "error: cannot open '%s': %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    while ((ch = fgetc(input_file)) != EOF) {
        unsigned char byte = (unsigned char)ch;
        counts[byte]++;
    }

    if (ferror(input_file)) {
        fprintf(stderr, "error: failed to read '%s'\n", argv[1]);
        fclose(input_file);
        return EXIT_FAILURE;
    }

    fclose(input_file);

    for (i = 0; i < CHAR_COUNT; i++) {
        if (counts[i] > 0) {
            order[used_count] = i;
            used_count++;
        }
    }

    for (i = 0; i < used_count - 1; i++) {
        for (j = i + 1; j < used_count; j++) {
            int left = order[i];
            int right = order[j];

            if (counts[right] > counts[left] ||
                (counts[right] == counts[left] && right < left)) {
                int temp = order[i];
                order[i] = order[j];
                order[j] = temp;
            }
        }
    }

    printf("%-12s %10s\n", "CHAR", "COUNT");
    printf("%-12s %10s\n", "----", "-----");

    for (i = 0; i < used_count; i++) {
        unsigned char byte = (unsigned char)order[i];

        if (byte == ' ') {
            printf("%-12s %10zu\n", "space", counts[byte]);
        } else if (byte == '\n') {
            printf("%-12s %10zu\n", "\\n", counts[byte]);
        } else if (byte == '\r') {
            printf("%-12s %10zu\n", "\\r", counts[byte]);
        } else if (byte == '\t') {
            printf("%-12s %10zu\n", "\\t", counts[byte]);
        } else if (isprint(byte)) {
            printf("'%c'          %10zu\n", byte, counts[byte]);
        } else {
            printf("0x%02X         %10zu\n", byte, counts[byte]);
        }
    }

    return EXIT_SUCCESS;
}
