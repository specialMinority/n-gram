#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BUCKET_COUNT 4096u
#define MAX_LOAD_DENOMINATOR 4u

typedef struct WordCount {
    char *word;
    size_t count;
    struct WordCount *next;
} WordCount;

typedef struct {
    WordCount **buckets;
    size_t bucket_count;
    size_t size;
} WordTable;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} TokenBuffer;

typedef enum {
    READ_OK = 0,
    READ_OUT_OF_MEMORY,
    READ_IO_ERROR
} ReadStatus;

static uint64_t hash_word(const char *word)
{
    uint64_t hash = 1469598103934665603ULL;

    while (*word != '\0') {
        hash ^= (unsigned char)*word;
        hash *= 1099511628211ULL;
        word++;
    }

    return hash;
}

static char *duplicate_string(const char *source)
{
    size_t length = strlen(source);
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, source, length + 1);
    return copy;
}

static int word_table_init(WordTable *table, size_t bucket_count)
{
    table->buckets = calloc(bucket_count, sizeof(table->buckets[0]));
    if (table->buckets == NULL) {
        table->bucket_count = 0;
        table->size = 0;
        return -1;
    }

    table->bucket_count = bucket_count;
    table->size = 0;
    return 0;
}

static void word_table_free(WordTable *table)
{
    size_t i;

    if (table->buckets == NULL) {
        return;
    }

    for (i = 0; i < table->bucket_count; i++) {
        WordCount *node = table->buckets[i];
        while (node != NULL) {
            WordCount *next = node->next;
            free(node->word);
            free(node);
            node = next;
        }
    }

    free(table->buckets);
    table->buckets = NULL;
    table->bucket_count = 0;
    table->size = 0;
}

static int word_table_resize(WordTable *table, size_t new_bucket_count)
{
    WordCount **new_buckets;
    size_t i;

    new_buckets = calloc(new_bucket_count, sizeof(new_buckets[0]));
    if (new_buckets == NULL) {
        return -1;
    }

    for (i = 0; i < table->bucket_count; i++) {
        WordCount *node = table->buckets[i];
        while (node != NULL) {
            WordCount *next = node->next;
            size_t index = (size_t)(hash_word(node->word) % new_bucket_count);
            node->next = new_buckets[index];
            new_buckets[index] = node;
            node = next;
        }
    }

    free(table->buckets);
    table->buckets = new_buckets;
    table->bucket_count = new_bucket_count;
    return 0;
}

static int word_table_maybe_grow(WordTable *table)
{
    size_t threshold = table->bucket_count - (table->bucket_count / MAX_LOAD_DENOMINATOR);

    if (table->size < threshold) {
        return 0;
    }

    if (table->bucket_count > SIZE_MAX / 2u) {
        return -1;
    }

    return word_table_resize(table, table->bucket_count * 2u);
}

static int word_table_add(WordTable *table, const char *word)
{
    uint64_t hash = hash_word(word);
    size_t index = (size_t)(hash % table->bucket_count);
    WordCount *node = table->buckets[index];

    while (node != NULL) {
        if (strcmp(node->word, word) == 0) {
            node->count++;
            return 0;
        }
        node = node->next;
    }

    if (word_table_maybe_grow(table) != 0) {
        return -1;
    }

    hash = hash_word(word);
    index = (size_t)(hash % table->bucket_count);

    node = malloc(sizeof(*node));
    if (node == NULL) {
        return -1;
    }

    node->word = duplicate_string(word);
    if (node->word == NULL) {
        free(node);
        return -1;
    }

    node->count = 1;
    node->next = table->buckets[index];
    table->buckets[index] = node;
    table->size++;
    return 0;
}

static int token_buffer_init(TokenBuffer *buffer)
{
    buffer->capacity = 64u;
    buffer->length = 0;
    buffer->data = malloc(buffer->capacity);

    if (buffer->data == NULL) {
        buffer->capacity = 0;
        return -1;
    }

    buffer->data[0] = '\0';
    return 0;
}

static void token_buffer_free(TokenBuffer *buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static int token_buffer_append(TokenBuffer *buffer, unsigned char byte)
{
    char *grown;
    size_t new_capacity;

    if (buffer->length < buffer->capacity - 1u) {
        buffer->data[buffer->length++] = (char)byte;
        buffer->data[buffer->length] = '\0';
        return 0;
    }

    if (buffer->capacity > SIZE_MAX / 2u) {
        return -1;
    }

    new_capacity = buffer->capacity * 2u;
    grown = realloc(buffer->data, new_capacity);
    if (grown == NULL) {
        return -1;
    }

    buffer->data = grown;
    buffer->capacity = new_capacity;
    buffer->data[buffer->length++] = (char)byte;
    buffer->data[buffer->length] = '\0';
    return 0;
}

static int flush_token(TokenBuffer *buffer, WordTable *table)
{
    int result;

    if (buffer->length == 0) {
        return 0;
    }

    result = word_table_add(table, buffer->data);
    buffer->length = 0;
    buffer->data[0] = '\0';
    return result;
}

static int is_ascii_delimiter(unsigned char byte)
{
    if (byte >= 128u) {
        return 0;
    }

    return isspace(byte) || ispunct(byte) || iscntrl(byte);
}

static unsigned char normalize_byte(unsigned char byte)
{
    if (byte < 128u) {
        return (unsigned char)tolower(byte);
    }

    return byte;
}

static ReadStatus read_words(FILE *input, WordTable *table, TokenBuffer *buffer)
{
    int ch;

    while ((ch = fgetc(input)) != EOF) {
        unsigned char byte = (unsigned char)ch;

        if (is_ascii_delimiter(byte)) {
            if (flush_token(buffer, table) != 0) {
                return READ_OUT_OF_MEMORY;
            }
        } else if (token_buffer_append(buffer, normalize_byte(byte)) != 0) {
            return READ_OUT_OF_MEMORY;
        }
    }

    if (ferror(input)) {
        return READ_IO_ERROR;
    }

    if (flush_token(buffer, table) != 0) {
        return READ_OUT_OF_MEMORY;
    }

    return READ_OK;
}

static WordCount **word_table_items(const WordTable *table)
{
    WordCount **items;
    size_t item_index = 0;
    size_t i;

    if (table->size == 0) {
        return NULL;
    }

    if (table->size > SIZE_MAX / sizeof(items[0])) {
        return NULL;
    }

    items = malloc(table->size * sizeof(items[0]));
    if (items == NULL) {
        return NULL;
    }

    for (i = 0; i < table->bucket_count; i++) {
        WordCount *node = table->buckets[i];
        while (node != NULL) {
            items[item_index++] = node;
            node = node->next;
        }
    }

    return items;
}

static int compare_word_counts(const void *left, const void *right)
{
    const WordCount *left_item = *(const WordCount *const *)left;
    const WordCount *right_item = *(const WordCount *const *)right;

    if (left_item->count < right_item->count) {
        return 1;
    }
    if (left_item->count > right_item->count) {
        return -1;
    }

    return strcmp(left_item->word, right_item->word);
}

static void print_results(WordCount **items, size_t item_count)
{
    size_t i;
    size_t word_width = strlen("WORD");
    int field_width;

    for (i = 0; i < item_count; i++) {
        size_t length = strlen(items[i]->word);
        if (length > word_width) {
            word_width = length;
        }
    }

    field_width = word_width > (size_t)INT_MAX ? INT_MAX : (int)word_width;

    printf("%-*s %10s\n", field_width, "WORD", "COUNT");
    printf("%-*s %10s\n", field_width, "----", "-----");

    for (i = 0; i < item_count; i++) {
        printf("%-*s %10zu\n", field_width, items[i]->word, items[i]->count);
    }
}

static void print_usage(const char *program_name)
{
    fprintf(stderr, "Usage: %s <input-file>\n", program_name);
}

int main(int argc, char **argv)
{
    FILE *input;
    WordTable table;
    TokenBuffer buffer;
    WordCount **items = NULL;
    ReadStatus read_status;
    int exit_code = EXIT_FAILURE;

    table.buckets = NULL;
    table.bucket_count = 0;
    table.size = 0;
    buffer.data = NULL;
    buffer.length = 0;
    buffer.capacity = 0;

    if (argc != 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    input = fopen(argv[1], "rb");
    if (input == NULL) {
        fprintf(stderr, "error: cannot open '%s': %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    if (word_table_init(&table, INITIAL_BUCKET_COUNT) != 0) {
        fprintf(stderr, "error: out of memory\n");
        fclose(input);
        return EXIT_FAILURE;
    }

    if (token_buffer_init(&buffer) != 0) {
        fprintf(stderr, "error: out of memory\n");
        fclose(input);
        word_table_free(&table);
        return EXIT_FAILURE;
    }

    read_status = read_words(input, &table, &buffer);
    if (fclose(input) != 0 && read_status == READ_OK) {
        read_status = READ_IO_ERROR;
    }

    if (read_status == READ_OUT_OF_MEMORY) {
        fprintf(stderr, "error: out of memory\n");
        goto cleanup;
    }
    if (read_status == READ_IO_ERROR) {
        fprintf(stderr, "error: failed to read '%s'\n", argv[1]);
        goto cleanup;
    }

    items = word_table_items(&table);
    if (table.size > 0 && items == NULL) {
        fprintf(stderr, "error: out of memory\n");
        goto cleanup;
    }

    if (table.size > 1) {
        qsort(items, table.size, sizeof(items[0]), compare_word_counts);
    }
    print_results(items, table.size);
    exit_code = EXIT_SUCCESS;

cleanup:
    free(items);
    token_buffer_free(&buffer);
    word_table_free(&table);
    return exit_code;
}
