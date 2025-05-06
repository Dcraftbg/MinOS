#include "tty.h"
#include "../../log.h"
#include "../../debug.h"
#include "../../kernel.h"
#include "../../cmdline.h"
#include "../../fbwriter.h"

#include "../../term/fb/fb.h"
#include "../../term/chr/chr.h"
bool ttyscratch_reserve(TtyScratch* scratch, size_t extra) {
    if(scratch->len + extra > scratch->cap) {
        size_t ncap = scratch->cap*2 + extra;
        char* data = kernel_malloc(ncap);
        if(!data) return false;
        memcpy(data, scratch->data, scratch->len);
        if(scratch->data != scratch->small_inline) kernel_dealloc(scratch->data, scratch->cap);
        scratch->data = data;
        scratch->cap = ncap;
    }
    return true;
}
void ttyscratch_shrink(TtyScratch* scratch) {
    if(scratch->len <= sizeof(scratch->small_inline)) {
        memcpy(scratch->small_inline, scratch->data, scratch->len);
        if(scratch->data != scratch->small_inline) kernel_dealloc(scratch->data, scratch->cap);
        scratch->cap = sizeof(scratch->small_inline);
        scratch->data = scratch->small_inline;
        return;
    } 
    // TODO: Could technically do PAGE_ALIGN_UP so then it would have a little extra
    // capacity, but idrk
    char* data = kernel_malloc(scratch->len);
    if(!data) return;
    memcpy(data, scratch->data, scratch->len);
    kernel_dealloc(scratch->data, scratch->cap);
    scratch->data = data;
    scratch->cap = scratch->len;
}
static Cache* tty_cache = NULL;
Tty* tty_new(void) {
    Tty* tty = cache_alloc(tty_cache);
    if(!tty) return NULL;
    memset(tty, 0, sizeof(*tty));
    ttyscratch_init(&tty->scratch);
    tty->flags = TTY_ECHO;
    return tty;
}
static Inode* get_init_keyboard() {
    intptr_t e;
    const char* path="/devices/keyboard";
    Inode* inode;
    if((e=vfs_find_abs(path, &inode)) < 0) {
        kwarn("(tty) Failed to find `%s` (initial tty keyboard): %s", path, status_str(e));
        return NULL;
    }
    return inode;
}
static Tty* fallback_environ() {
    // TODO: Actually fallback to serial. I'm too lazy to do that right now
    return NULL;
}
static Tty* default_environ() {
    intptr_t e;
    Tty* tty;
    if((e=init_fbtty()) < 0) {
        kwarn("(tty) Failed to initialise fbtty. Falling back to serial: %s", status_str(e));
        return fallback_environ();
    }
    Inode* keyboard=get_init_keyboard();
    if(!keyboard) {
        kwarn("(tty) Failed to get initial keyboard. Falling back to serial");
        deinit_fbtty();
        return fallback_environ();
    }
    tty = fbtty_new(keyboard, 0);
    if(!tty) {
        kwarn("(tty) Failed to create fbtty");
        idrop(keyboard);
        deinit_fbtty();
        return fallback_environ();
    }
    return tty;
}
static Tty* get_init_tty() {
    intptr_t e;
    char* ttycmd = cmdline_get("tty");
    if(!ttycmd) 
        return default_environ();
    
    if(strcmp(ttycmd, "fb") == 0) {
        char* keyboard_path      = cmdline_get("tty:keyboard");
        char* framebuffer_id_str = cmdline_get("tty:fbid");
        if(!keyboard_path || !framebuffer_id_str) {
            kwarn("(tty) Missing option tty:keyboard OR tty:fbid for tty=fb. Falling back to default environment...");
            return default_environ();
        }
        size_t framebuffer_id;
        {
            const char* end=NULL;
            framebuffer_id = atosz(framebuffer_id_str, &end);
            if(!end || end==framebuffer_id_str) {
                kerror("tty:fbid is NOT a valid integer. Falling back to default environment");
                return default_environ();
            }
        }
        if((e=init_fbtty()) < 0) {
            kwarn("(tty) Failed to initialise fbtty. Falling back to serial: %s", status_str(e));
            return fallback_environ();
        }
        Inode* keyboard;
        if((e=vfs_find_abs(keyboard_path, &keyboard))) {
            kwarn("(tty) Failed to get keyboard `%s`. Falling back to default environment...", keyboard_path);
            deinit_fbtty();
            return default_environ();
        }
        Tty* tty = fbtty_new(keyboard, framebuffer_id);
        if(!tty) {
            idrop(keyboard);
            deinit_fbtty();
            return fallback_environ();
        }
        return tty;
    } else if (strcmp(ttycmd, "chr") == 0) {
        char* chrdevice_path = cmdline_get("tty:chrdevice");
        if(!chrdevice_path) {
            kwarn("(tty) Missing `chr` option in tty:chrdevice. Falling back to default environment...");
            return default_environ();
        }
        Inode* inode;
        if((e=vfs_find_abs(chrdevice_path, &inode)) < 0) {
            kwarn("(tty) Failed to find `%s` chrdevice: %s. Falling back to default_environ...", chrdevice_path, status_str(e));
            return default_environ();
        }
        Tty* tty = chrtty_new(inode);
        if(!tty) {
            idrop(inode);
            return default_environ();
        }
        return tty;
    } else {
        kwarn("Undefined tty type `%s` in commandline. Falling back to default environment...", ttycmd);
        return default_environ();
    }
    return NULL;
}
static intptr_t tty_write(Inode* file, const void* buf, size_t size, off_t offset) {
    (void)offset;
    Tty* tty = file->priv;
    if(!tty->putchar) return -UNSUPPORTED;
    for(size_t i = 0; i < size; ++i) {
        tty->putchar(tty, ((uint8_t*)buf)[i]);
    }
    return size;
}
static void tty_putchar(Tty* tty, uint32_t code) {
    if(tty->putchar) tty->putchar(tty, code);
}
static void tty_echo(Tty* tty, uint32_t code) {
    if(tty->flags & TTY_ECHO) tty_putchar(tty, code);
}
static intptr_t tty_read(Inode* file, void* buf, size_t size, off_t offset) {
    Tty* tty = file->priv;
    if(!tty->getchar) return -UNSUPPORTED;
    if(tty->scratch.len) goto end;
    for(;;) {
        uint32_t code = tty->getchar(tty);
        if(code && (tty->flags & TTY_INSTANT)) {
            if(!ttyscratch_push(&tty->scratch, code)) return -NOT_ENOUGH_MEM;
            goto end;
        }
        switch(code) {
        case '\b':
            if(tty->scratch.len) {
                tty_echo(tty, '\b');
                tty->scratch.len--;
            }
            break;
        case '\n':
            if(!ttyscratch_push(&tty->scratch, '\n')) return -NOT_ENOUGH_MEM;
            tty_echo(tty, '\n');
            goto end;
        case '\0':
            break;
        default: {
            if(!ttyscratch_push(&tty->scratch, code)) return -NOT_ENOUGH_MEM;
            tty_echo(tty, code);
        } break;
        }
    }
end:
    size_t to_copy = tty->scratch.len < size ? tty->scratch.len : size;
    memcpy(buf, tty->scratch.data, to_copy);
    memmove(tty->scratch.data, tty->scratch.data + to_copy, tty->scratch.len - to_copy);
    tty->scratch.len -= to_copy;
    ttyscratch_shrink(&tty->scratch);
    return to_copy;
}
static intptr_t tty_ioctl(Inode* file, Iop op, void* arg) {
    Tty* tty = file->priv;
    switch(op) {
    case TTY_IOCTL_SET_FLAGS:
        tty->flags = (ttyflags_t)(uintptr_t)arg;
        break;
    case TTY_IOCTL_GET_FLAGS:
        *((ttyflags_t*)arg) = tty->flags;
        break;
    default:
        return -UNSUPPORTED;
    }
    return 0;
}
static InodeOps inodeOps = {
    .write = tty_write,
    .read = tty_read,
    .ioctl = tty_ioctl
};
Inode* create_tty_device(Tty* tty) {
    Inode* device = new_inode();
    if(!device) return NULL;
    device->ops = &inodeOps;
    device->priv = tty;
    return device;
}
void init_tty(void) {
    tty_cache = create_new_cache(sizeof(Tty), "Tty");
    if(!tty_cache) {
        kerror("Failed to create tty_cache. Not Enough Memory");
        return;
    }
    Tty* tty = get_init_tty();
    if(!tty) {
        // FIXME: cache_destory on tty_cache
        kerror("(tty) Missing initial tty");
        return;
    }
    Inode* device = create_tty_device(tty);
    if(!device) {
        // FIXME: deinit the tty
        // FIXME: cache_destory on tty_cache
        kerror("(tty) Failed to create tty device. Not Enough Memory");
        return;
    }
    intptr_t e;
    if((e=vfs_register_device("tty0", device)) < 0) {
        // FIXME: deallocate the device
        // FIXME: deinit the tty
        // FIXME: cache_destory on tty_cache
        kerror("(tty) Failed to register tty device: %s", status_str(e));
        return;
    }
}
