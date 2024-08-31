#include "rootfs.h"
#include "../embed.h"
void init_rootfs() {
    intptr_t e = 0;
    const char* path = NULL;
    path = "/devices";
    if((e = vfs_mkdir(path)) < 0) {
        printf("ERROR: init_rootfs: Could not create %s : %s\n",path,status_str(e));
        kabort();
    }
    for(size_t i = 0; i < embed_entries_count; ++i) {
        EmbedEntry* entry = &embed_entries[i];
        switch(entry->kind) {
        case EMBED_FILE: {
            if((e = vfs_create(entry->name)) < 0) {
               printf("ERROR: init_rootfs: Could not create %s : %s\n",entry->name,status_str(e));
               kabort();
            }
            VfsFile file = {0};
            if((e = vfs_open(entry->name, &file, MODE_WRITE)) < 0) {
                printf("ERROR: init_rootfs: Could not open %s : %s\n",entry->name,status_str(e));
                kabort();
            }
            if((e = write_exact(&file, embed_data+entry->offset, entry->size)) < 0) {
                printf("ERROR: init_rootfs: Could not write data: %s : %s\n",entry->name, status_str(e));
                vfs_close(&file);
                kabort();
            }
            vfs_close(&file);
        } break;
        case EMBED_DIR: {
            if((e = vfs_mkdir(entry->name)) < 0) {
                printf("ERROR: init_rootfs: Could not create %s : %s\n",entry->name,status_str(e));
                kabort();
            }
        } break;
        }
    }
#if 1
    path = "/Welcome.txt";
    if((e = vfs_create(path)) < 0) {
        printf("ERROR: init_rootfs: Could not create %s : %s\n",path,status_str(e));
        kabort();
    }
    {
        const char* msg = "Hello and welcome to MinOS!\nThis is a mini operating system made in C.\nDate of compilation " __DATE__ ".";
        VfsFile file = {0};
        if((e = vfs_open(path, &file, MODE_WRITE)) < 0) {
            printf("WARN: init_rootfs: Could not open %s : %s\n",path,status_str(e));
            log_cache(kernel.inode_cache);
            kabort();
        } else {
            if((e = write_exact(&file, msg, strlen(msg))) < 0) {
                vfs_close(&file);
                printf("WARN: init_rootfs: Could not write welcome message : %s\n",status_str(e));
            }
        } 
        vfs_close(&file);
    }
#endif
}
