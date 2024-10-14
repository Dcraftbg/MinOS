#pragma once
typedef struct {
    const char* name;
    size_t offset;
    size_t size;
    size_t kind;
} EmbedEntry;
typedef enum {
    EMBED_DIR,
    EMBED_FILE
} EmbedFsKind;
typedef struct {
    EmbedEntry* items;
    size_t count;
    size_t capacity;
    Nob_String_Builder data;
} EmbedFs;
bool embed_mkdir(EmbedFs* fs, const char* fspath);
bool embed(EmbedFs* fs, const char* file, const char* fspath);
void embed_fs_clean(EmbedFs* fs);
bool embed_fs();

