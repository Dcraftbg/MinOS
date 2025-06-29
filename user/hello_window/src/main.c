#include <pluto.h>
#include <assert.h>
#include <minos/sysstd.h>

int main(void) {
    PlutoInstance instance;
    pluto_create_instance(&instance);
    size_t width = 500, height = 500;
    int win = pluto_create_window(&instance, &(WmCreateWindowInfo) {
        .width = width,
        .height = height,
        .title = "Hello bro"
    }, 16);
    int shm = pluto_create_shm_region(&instance, &(WmCreateSHMRegion) {
        .size = 500*500*sizeof(uint32_t)
    });
    uint32_t* addr = NULL;
    assert(_shmmap(shm, (void**)&addr) >= 0);
    int dvd_x = 0, dvd_y = 0;
    int dvd_dx = 1, dvd_dy = 1;
    int dvd_w = 100;
    int dvd_h = 30;
    for(;;) {
        dvd_x += dvd_dx;
        dvd_y += dvd_dy;
        if(dvd_x < 0) {
            dvd_x = 0;
            dvd_dx = -dvd_dx;
        }
        if(dvd_x + dvd_w > (int)width) {
            dvd_x = width-dvd_w;
            dvd_dx = -dvd_dx;
        }
        if(dvd_y < 0) {
            dvd_y = 0;
            dvd_dy = -dvd_dy;
        }
        if(dvd_y + dvd_h > (int)height) {
            dvd_y = height-dvd_h;
            dvd_dy = -dvd_dy;
        }
        for(size_t y = 0; y < height; ++y) {
            for(size_t x = 0; x < width; ++x) {
                addr[y * width + x] = 0xFF111111;
            }
        }
        for(size_t y = dvd_y; y < (size_t)dvd_y + dvd_h; ++y) {
            for(size_t x = dvd_x; x < (size_t)dvd_x + dvd_w; ++x) {
                addr[y * width + x] = 0xFFFF0000;
            }
        }
        pluto_draw_shm_region(&instance, &(WmDrawSHMRegion){
            .window = win,
            .shm_key = shm,
            .width = width,
            .height = height,
            .pitch_bytes = width * sizeof(uint32_t),
        });
    }
}
