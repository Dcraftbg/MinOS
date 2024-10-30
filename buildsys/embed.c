#include "compile.h"
#include "embed.h"
bool embed_mkdir(EmbedFs* fs, const char* fspath) {
    EmbedEntry entry = {
        fspath,
        0,
        0,
        EMBED_DIR,
    };
    nob_da_append(fs, entry);
    return true;
}
bool embed(EmbedFs* fs, const char* file, const char* fspath) {
    Nob_String_Builder sb={0};
    if(!nob_read_entire_file(file, &sb)) return false;
    EmbedEntry entry = {
        fspath,
        fs->data.count,
        sb.count,
        EMBED_FILE, 
    };
    nob_sb_append_buf(&fs->data, sb.items, sb.count);
    nob_da_append(fs, entry);
    nob_sb_free(sb);
    return true;
}
void embed_fs_clean(EmbedFs* fs) {
    nob_sb_free(fs->data);
    nob_da_free(*fs);
}
bool embed_fs() {
    bool result = true;
    EmbedFs fs = {0};
#if 0
    FILE* f = NULL;
    if(!embed_mkdir(&fs, "/user")) nob_return_defer(false);
    if(!embed(&fs, "./bin/user/nothing/nothing", "/user/nothing")) nob_return_defer(false);
    if(!embed(&fs, "./bin/user/init/init", "/user/init")) nob_return_defer(false);
    if(!embed(&fs, "./bin/user/shell/shell", "/user/shell")) nob_return_defer(false);
    if(!embed(&fs, "./bin/user/hello/hello", "/user/hello")) nob_return_defer(false);
    if(!embed(&fs, "./bin/user/fbtest/fbtest", "/user/fbtest")) nob_return_defer(false);
    const char* opath = "./kernel/embed.h";
    f = fopen(opath, "wb");
    if(!f) {
        nob_log(NOB_ERROR, "Failed to open file %s: %s", opath, strerror(errno));
        embed_fs_clean(&fs);
        return false;
    }
    fprintf(f, "#pragma once\n");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "#include <stddef.h>\n");
    fprintf(f, "typedef struct {\n");
    fprintf(f, "    const char* name;\n");
    fprintf(f, "    size_t offset;\n");
    fprintf(f, "    size_t size;\n");
    fprintf(f, "    size_t kind;\n");
    fprintf(f, "} EmbedEntry;\n");
    fprintf(f, "typedef enum {\n");
    fprintf(f, "    EMBED_DIR,\n");
    fprintf(f, "    EMBED_FILE\n");
    fprintf(f, "} EmbedFsKind;\n");
    fprintf(f, "#define embed_entries_count %zu\n", fs.count);
    fprintf(f, "EmbedEntry embed_entries[%zu] = {\n", fs.count);
    for(size_t i = 0; i < fs.count; ++i) {
        if(i != 0) fprintf(f, ",\n");
        EmbedEntry* e = &fs.items[i];
        fprintf(f, "   { \"%s\", %zu, %zu, %zu }",e->name,e-> offset,e->size,e->kind);
    }
    fprintf(f, "\n};\n");
    fprintf(f, "size_t embed_data_size = %zu;\n",fs.data.count);
    fprintf(f, "uint8_t embed_data[] = {");
    for(size_t i = 0; i < fs.data.count; ++i) {
        if(i != 0) {
            fprintf(f, ", ");
        }
        if(i % 8 == 0) fprintf(f, "\n   ");
        uint8_t byte = fs.data.items[i];
        fprintf(f, "0x%02X", byte);
    }
    fprintf(f, "\n};");
defer:
    embed_fs_clean(&fs);
    if(f) fclose(f);
#endif
    nob_log(NOB_WARNING, "embedfs is deprecated in favour of initrd");
    return result;
}
