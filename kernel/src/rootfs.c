#include "rootfs.h"
#include "fs/ustar/ustar.h"
#include "log.h"
#include "bootutils.h"
#include "vfs.h"
#include "print.h"
#include "kernel.h"

void init_rootfs(void) {
    intptr_t e = 0;
    const char* path = NULL;
    path = "/devices";
    Inode* devices;
    if((e = vfs_creat_abs(path, O_DIRECTORY, &devices)) < 0) {
        printf("ERROR: init_rootfs: Could not create %s : %s\n", path, status_str(e));
        kabort();
    }
    idrop(devices);

    const char* initrd = "/initrd";
    BootModule module;
    if(find_bootmodule(initrd, &module)) {
        if((e=ustar_unpack("/", module.data, module.size)) < 0) {
            kerror("Failed to unpack: %s into root: %s", initrd, status_str(e));
        }
    } 
    else kwarn("Failed to find %s", initrd); 
#ifdef ENABLE_WELCOME
    path = "/Welcome.txt";
    if((e = vfs_creat_abs(path, 0)) < 0) {
        printf("ERROR: init_rootfs: Could not create %s : %s\n", path, status_str(e));
        kabort();
    }
    {
        const char* msg = "Hello and welcome to MinOS!\nThis is a mini operating system made in C.\nDate of compilation " __DATE__ ".";
        Inode* file;
        if((e = vfs_find_abs(path, &file)) < 0) {
            kwarn("init_rootfs: Could not open %s : %s\n", path, status_str(e));
            return;
        } else {
            if((e = write_exact(file, msg, strlen(msg), 0)) < 0) {
                kwarn("init_rootfs: Could not write welcome message : %s\n", status_str(e));
                idrop(file);
                return;
            }
        } 
        idrop(file);
    }
#endif
}
