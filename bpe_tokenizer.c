#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <wchar.h>
#include <locale.h>
#include <assert.h>
#include <wctype.h>

#define MAX_TOKENS 100000
#define MAX_TOKEN_LEN 128
#define MAX_PAIR_LEN 256
#define HASH_SIZE 10000
#define MAX_VOCAB_SIZE 50000
#define MIN_TOKEN_FREQ 2
#define MAX_THREADS 8

// Vocabulary entry (word or subword)
typedef struct {
    wchar_t *token;
    uint32_t id;
    int freq;
} VocabEntry;

// BPE pair structure
typedef struct BPE_Pair {
    wchar_t pair[MAX_PAIR_LEN];
    int count;
    uint32_t id;
    struct BPE_Pair *next;
} BPE_Pair;

// Hashmap structure for storing BPE pairs
typedef struct {
    BPE_Pair **table;
    pthread_mutex_t *mutexes;
} BPE_HashMap;

VocabEntry *vocabulary = NULL;
int vocab_size = 0;

// Function prototypes
unsigned int hash(const wchar_t *pair);
BPE_HashMap* create_bpe_hashmap();
void free_bpe_hashmap(BPE_HashMap *map);
void add_pair(BPE_HashMap *map, const wchar_t *pair, uint32_t id);
void to_lowercase(wchar_t *str);
void add_to_vocabulary(const wchar_t *token);
wchar_t **tokenize(const char *text, int *token_count);
void free_tokens(wchar_t **tokens, int token_count);
int equal_pair(const wchar_t *token1, const wchar_t *token2, const wchar_t *pair);
void find_most_frequent_pair(BPE_HashMap *map, wchar_t *best_pair, int *best_count);
void save_vocab();
void save_vocab_to_file(const char *filename);
void convert_vocab_to_subwords();
void bpe_subword_merge(int num_merges);

// Compute djb2 hash for a wide string
unsigned int hash(const wchar_t *pair) {
    unsigned int hash_val = 5381;
    while (*pair) {
        hash_val = ((hash_val << 5) + hash_val) + *pair++;
    }
    return hash_val % HASH_SIZE;
}

// Create and initialize BPE hash map
BPE_HashMap* create_bpe_hashmap() {
    BPE_HashMap *map = malloc(sizeof(BPE_HashMap));
    if (!map) { fprintf(stderr, "Error: malloc failed for BPE_HashMap\n"); exit(1); }
    map->table = calloc(HASH_SIZE, sizeof(BPE_Pair *));
    if (!map->table) { fprintf(stderr, "Error: calloc failed for hash table\n"); free(map); exit(1); }
    map->mutexes = malloc(HASH_SIZE * sizeof(pthread_mutex_t));
    if (!map->mutexes) { fprintf(stderr, "Error: malloc failed for mutexes\n"); free(map->table); free(map); exit(1); }
    for (int i = 0; i < HASH_SIZE; i++) {
        if (pthread_mutex_init(&map->mutexes[i], NULL) != 0) {
            fprintf(stderr, "Error: pthread_mutex_init failed\n");
            for (int j = 0; j < i; j++) { pthread_mutex_destroy(&map->mutexes[j]); }
            free(map->mutexes); free(map->table); free(map); exit(1);
        }
    }
    return map;
}

// Free BPE hash map resources
void free_bpe_hashmap(BPE_HashMap *map) {
    for (int i = 0; i < HASH_SIZE; i++) {
        pthread_mutex_destroy(&map->mutexes[i]);
        BPE_Pair *entry = map->table[i];
        while (entry) { BPE_Pair *tmp = entry; entry = entry->next; free(tmp); }
    }
    free(map->mutexes); free(map->table); free(map);
}

// Add a BPE pair to hash map
void add_pair(BPE_HashMap *map, const wchar_t *pair, uint32_t id) {
    unsigned int index = hash(pair);
    pthread_mutex_lock(&map->mutexes[index]);
    BPE_Pair *entry = map->table[index];
    while (entry) {
        if (wcscmp(entry->pair, pair) == 0) { entry->count++; pthread_mutex_unlock(&map->mutexes[index]); return; }
        entry = entry->next;
    }
    BPE_Pair *new_pair = malloc(sizeof(BPE_Pair));
    if (!new_pair) { fprintf(stderr, "Error: malloc failed in add_pair\n"); pthread_mutex_unlock(&map->mutexes[index]); return; }
    wcscpy(new_pair->pair, pair);
    new_pair->count = 1;
    new_pair->id = id;
    new_pair->next = map->table[index];
    map->table[index] = new_pair;
    pthread_mutex_unlock(&map->mutexes[index]);
}

// Convert a wide string to lowercase
void to_lowercase(wchar_t *str) {
    for (; *str; ++str) *str = towlower(*str);
}

// Add token to the vocabulary (or update frequency)
void add_to_vocabulary(const wchar_t *token) {
    for (int i = 0; i < vocab_size; i++) {
        if (wcscmp(vocabulary[i].token, token) == 0) { vocabulary[i].freq++; return; }
    }
    if (vocab_size < MAX_VOCAB_SIZE) {
        VocabEntry *tmp = realloc(vocabulary, (vocab_size + 1) * sizeof(VocabEntry));
        if (!tmp) { fprintf(stderr, "Error: realloc failed in add_to_vocabulary\n"); return; }
        vocabulary = tmp;
        wchar_t *dup_token = wcsdup(token);
        if (!dup_token) { fprintf(stderr, "Error: wcsdup failed in add_to_vocabulary\n"); return; }
        vocabulary[vocab_size].token = dup_token;
        vocabulary[vocab_size].id = vocab_size;
        vocabulary[vocab_size].freq = 1;
        vocab_size++;
    }
}

// Tokenize input text (split by delimiters) and build initial vocabulary
wchar_t **tokenize(const char *text, int *token_count) {
    setlocale(LC_ALL, "en_US.UTF-8");
    size_t req_len = mbstowcs(NULL, text, 0);
    if (req_len == (size_t)-1) { fprintf(stderr, "Error calculating required length\n"); *token_count = 0; return NULL; }
    wchar_t *wtext = malloc((req_len + 1) * sizeof(wchar_t));
    if (!wtext) { fprintf(stderr, "Memory allocation failed for wide text\n"); *token_count = 0; return NULL; }
    if (mbstowcs(wtext, text, req_len + 1) == (size_t)-1) { fprintf(stderr, "Error converting text\n"); free(wtext); *token_count = 0; return NULL; }

    wchar_t **tokens = malloc(MAX_TOKENS * sizeof(wchar_t *));
    if (!tokens) { fprintf(stderr, "Memory allocation failed for tokens array\n"); free(wtext); *token_count = 0; return NULL; }
    for (int i = 0; i < MAX_TOKENS; i++) {
        tokens[i] = malloc(MAX_TOKEN_LEN * sizeof(wchar_t));
        if (!tokens[i]) {
            fprintf(stderr, "Memory allocation failed for token %d\n", i);
            for (int j = 0; j < i; j++) free(tokens[j]);
            free(tokens); free(wtext); *token_count = 0;
            return NULL;
        }
    }
    const wchar_t *delims = L" .,!?;:()\n";
    wchar_t *token = wcstok(wtext, delims, NULL);
    int count = 0;
    while (token && count < MAX_TOKENS) {
        to_lowercase(token);
        wcsncpy(tokens[count], token, MAX_TOKEN_LEN - 1);
        tokens[count][MAX_TOKEN_LEN - 1] = L'\0';
        add_to_vocabulary(token);
        count++;
        token = wcstok(NULL, delims, NULL);
    }
    free(wtext);
    *token_count = count;
    return tokens;
}

// Free tokens array
void free_tokens(wchar_t **tokens, int token_count) {
    for (int i = 0; i < token_count; i++) free(tokens[i]);
    free(tokens);
}

// Check if two tokens combined (with space) equal the given pair
int equal_pair(const wchar_t *token1, const wchar_t *token2, const wchar_t *pair) {
    wchar_t tmp[MAX_PAIR_LEN];
    swprintf(tmp, MAX_PAIR_LEN, L"%ls %ls", token1, token2);
    return (wcscmp(tmp, pair) == 0);
}

// Find most frequent pair in the hash map
void find_most_frequent_pair(BPE_HashMap *map, wchar_t *best_pair, int *best_count) {
    *best_count = 0;
    best_pair[0] = L'\0';
    for (int i = 0; i < HASH_SIZE; i++) {
        BPE_Pair *entry = map->table[i];
        while (entry) {
            if (entry->count > *best_count) {
                *best_count = entry->count;
                wcscpy(best_pair, entry->pair);
            }
            entry = entry->next;
        }
    }
}

// Print vocabulary to console
void save_vocab() {
    printf("\n[INFO] Vocabulary:\n");
    for (int i = 0; i < vocab_size; i++) {
        if (vocabulary[i].token != NULL) {
            char buffer[512];
            wcstombs(buffer, vocabulary[i].token, sizeof(buffer));
            printf("%s (freq=%d)\n", buffer, vocabulary[i].freq);
        } else {
            printf("[NULL] (freq=%d)\n", vocabulary[i].freq);
        }
    }
}

// Save vocabulary to file (tab-separated)
void save_vocab_to_file(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) { fprintf(stderr, "Error: Could not open %s for writing\n", filename); return; }
    for (int i = 0; i < vocab_size; i++) {
        if (vocabulary[i].token != NULL) {
            char buffer[512];
            wcstombs(buffer, vocabulary[i].token, sizeof(buffer));
            fprintf(fp, "%s\t%d\n", buffer, vocabulary[i].freq);
        } else {
            fprintf(fp, "[NULL]\t%d\n", vocabulary[i].freq);
        }
    }
    fclose(fp);
}

// Convert vocabulary words to subword representation (insert space between characters)
void convert_vocab_to_subwords() {
    for (int i = 0; i < vocab_size; i++) {
        wchar_t *word = vocabulary[i].token;
        int len = wcslen(word);
        int new_size = (len * 2);
        wchar_t *new_str = malloc(new_size * sizeof(wchar_t));
        if (!new_str) { fprintf(stderr, "Memory allocation failed in convert_vocab_to_subwords\n"); continue; }
        int pos = 0;
        for (int j = 0; j < len; j++) {
            new_str[pos++] = word[j];
            if (j < len - 1) new_str[pos++] = L' ';
        }
        new_str[pos] = L'\0';
        free(vocabulary[i].token);
        vocabulary[i].token = new_str;
    }
}

// Advanced BPE merge at subword level
void bpe_subword_merge(int num_merges) {
    for (int merge_iter = 0; merge_iter < num_merges; merge_iter++) {
        BPE_HashMap *map = create_bpe_hashmap();
        // Count adjacent subword pairs over entire vocabulary
        for (int i = 0; i < vocab_size; i++) {
            wchar_t *word_copy = wcsdup(vocabulary[i].token);
            if (!word_copy) continue;
            wchar_t *symbols[256];
            int symbol_count = 0;
            wchar_t *token = wcstok(word_copy, L" ", NULL);
            while (token && symbol_count < 256) {
                symbols[symbol_count++] = token;
                token = wcstok(NULL, L" ", NULL);
            }
            for (int j = 0; j < symbol_count - 1; j++) {
                wchar_t pair[MAX_PAIR_LEN];
                swprintf(pair, MAX_PAIR_LEN, L"%ls %ls", symbols[j], symbols[j+1]);
                for (int k = 0; k < vocabulary[i].freq; k++) {
                    add_pair(map, pair, i);
                }
            }
            free(word_copy);
        }
        // Find the most frequent pair
        wchar_t best_pair[MAX_PAIR_LEN];
        int best_count = 0;
        find_most_frequent_pair(map, best_pair, &best_count);
        free_bpe_hashmap(map);
        if (best_count < 1) {
            wprintf(L"[INFO] No more pairs to merge. Stopping merges.\n");
            break;
        }
        char best_pair_buffer[512];
        wcstombs(best_pair_buffer, best_pair, sizeof(best_pair_buffer));
        printf("[INFO] Subword Merge %d: Pair \"%s\" with frequency %d\n", merge_iter+1, best_pair_buffer, best_count);

        // Update vocabulary by merging best_pair in each word
        for (int i = 0; i < vocab_size; i++) {
            wchar_t *old_token = vocabulary[i].token;
            wchar_t new_token[MAX_TOKEN_LEN];
            new_token[0] = L'\0';

            wchar_t *copy = wcsdup(old_token);
            if (!copy) continue;
            wchar_t *sym = wcstok(copy, L" ", NULL);
            int first = 1;
            while (sym != NULL) {
                wchar_t *next = wcstok(NULL, L" ", NULL);
                if (next != NULL) {
                    wchar_t tmp_pair[MAX_PAIR_LEN];
                    swprintf(tmp_pair, MAX_PAIR_LEN, L"%ls %ls", sym, next);
                    if (wcscmp(tmp_pair, best_pair) == 0) {
                        wchar_t merged[MAX_TOKEN_LEN];
                        swprintf(merged, MAX_TOKEN_LEN, L"%ls%ls", sym, next);
                        if (!first) wcscat(new_token, L" ");
                        wcscat(new_token, merged);
                        first = 0;
                        sym = wcstok(NULL, L" ", NULL);
                        continue;
                    }
                }
                if (!first) wcscat(new_token, L" ");
                wcscat(new_token, sym);
                first = 0;
                sym = next;
            }
            free(copy);
            wchar_t *updated = wcsdup(new_token);
            if (updated != NULL) {
                free(vocabulary[i].token);
                vocabulary[i].token = updated;
            }
        }
    }
}

//
// main: اجرای توکنایزر، تبدیل به زیرواژه و ادغام BPE پیشرفته و ذخیره واژگان در فایل
//
int main() {
    setlocale(LC_ALL, "en_US.UTF-8");

    const char *text =
        "Although post-structuralist critiques have problematized the notion of objective epistemology, especially within the context of late modernity’s fragmented narratives, the intertextual entanglement of discourse, power, and subjectivity remains a locus of theoretical contestation. Consequently, any hermeneutic attempt at deconstructing the meta-narratives embedded within institutionalized knowledge systems necessitates a nuanced understanding of semiotic multiplicity and ontological ambiguity.";
    
    printf("Original text length: %zu\n\n", strlen(text));

    int token_count = 0;
    wchar_t **tokens = tokenize(text, &token_count);
    if (!tokens) { fprintf(stderr, "Tokenization failed.\n"); return 1; }
    printf("[INFO] Found %d tokens\n", token_count);
    printf("[INFO] Initial Vocabulary size: %d\n", vocab_size);

    free_tokens(tokens, token_count);

    save_vocab();
    save_vocab_to_file("init_vocab.txt");

    convert_vocab_to_subwords();
    printf("\n[INFO] Vocabulary after conversion to subwords:\n");
    save_vocab();

    bpe_subword_merge(50);

    printf("\n[INFO] Final Vocabulary (after subword merges):\n");
    save_vocab();

    save_vocab_to_file("vocab.txt");
    printf("[INFO] Vocabulary saved to 'vocab.txt'\n");

    for (int i = 0; i < vocab_size; i++) {
        free(vocabulary[i].token);
    }
    free(vocabulary);

    return 0;
}
