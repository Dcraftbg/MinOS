#include "ustar.h"
#include "../../fileutils.h"
#include "../../log.h"
int octtoi(const char* str, size_t len) {
    int res = 0;
    while (len > 0) {
        if(str[0] < '0' || str[0] > '8') return -1;
        res = res * 8 + str[0]-'0';
        str++;
        len--;
    }
    return res;
}
/*
typedef char Ustar12[12]
typedef struct {
    char filename[100];
    uint64_t mode;
    uint64_t ownerid;
    uint64_t groupid;
    Ustar12 filesize;
    Ustar12 modtime;
    uint64_t checksum; 
    uint8_t type;
    char linkedf_name[100];
    char ustar_indict[6];
    char ustar_version[2];
    char owner_name[32];
    char group_name[32];
    uint64_t majornum;
    uint64_t minornum;
    char filename_prefix[155];
} UstarMeta;
*/

#define USTAR_FILESIZE_OFF 124
#define USTAR_TYPE_OFF  156
#define USTAR_MAGIC_OFF 257
#define USTAR_DATA 512
intptr_t ustar_unpack(const char* into, const char* ustar_data, size_t ustar_size) {
    const char* ustar_end = ustar_data+ustar_size;
    intptr_t e;
    size_t into_len = strlen(into);
    if(into_len >= PATH_MAX) return -LIMITS;
    char* path = kernel_malloc(PATH_MAX);
    if(!path) return -NOT_ENOUGH_MEM;
    while(memcmp(ustar_data+USTAR_MAGIC_OFF, "ustar", 5)==0 && ustar_data < ustar_end) {
        int size=octtoi(ustar_data+USTAR_FILESIZE_OFF, 11);
        if(ustar_end-ustar_data < size) return -BUFFER_OVEWFLOW;
        uint8_t type = *(((uint8_t*)ustar_data)+USTAR_TYPE_OFF);
        size_t name_len=0;
        const char* name = ustar_data;
        if(memcmp(name, "./", 2) == 0) name += 2;
        while(name[name_len] && name_len < 98) {
            name_len++;
        }
        if(name_len == 98) return -INVALID_PATH;
        if(name_len > 0 && name[name_len-1] == '/') name_len--;
        if(into_len+name_len >= PATH_MAX) return -LIMITS; 
        memcpy(path         , into, into_len);
        memcpy(path+into_len, name, name_len);
        path[into_len+name_len] = '\0';

        ktrace("ustar: %s of type %c size %zu", path, type, size);
        if(type == '5') {
            if((e = vfs_mkdir_abs(path)) < 0) {
                if(e != -ALREADY_EXISTS) {
                    kerror("ERROR: ustar: Could not mkdir %s : %s", path, status_str(e));
                    goto err; 
                }
            }
        } else {
            if((e = vfs_create_abs(path)) < 0) {
                kerror("ERROR: ustar: Could not create %s : %s", path, status_str(e));
                goto err;
            }
            VfsFile file = {0};
            if((e = vfs_open_abs(path, &file, MODE_WRITE)) < 0) {
                kerror("ERROR: ustar: Could not open %s : %s", path, status_str(e));
                goto err;
            }
            if((e = write_exact(&file, ustar_data+512, size)) < 0) {
                kerror("ERROR: ustar: Could not write data: %s : %s", path, status_str(e));
                vfs_close(&file);
                goto err;
            }
            vfs_close(&file);
        }
        ustar_data += (((size+511)/512) + 1)*512;
    }
    kernel_dealloc(path, PATH_MAX);
    return 0;
err:
    kernel_dealloc(path, PATH_MAX);
    return e;
}
