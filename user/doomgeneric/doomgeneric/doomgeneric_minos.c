#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"
#include <stdio.h>
#include <minos/sysstd.h>
#include <minos/status.h>
uintptr_t fb;
void DG_Init() {
    intptr_t e;
    const char* fb_path = "/devices/fb0";
    if((e=open(fb_path, MODE_READ | MODE_WRITE)) < 0) {
        fprintf(stderr, "ERROR: Failed to open `%s`: %s\n", fb_path, status_str(e));
        exit(1);
    }
}

void DG_DrawFrame() {
    fprintf(stderr, "ERROR: Not implemented: DG_DrawFrame()");
    exit(1);
}

void DG_SleepMs(uint32_t ms) {
    fprintf(stderr, "ERROR: Not implemented: DG_SleepMs()");
    exit(1);
}

uint32_t DG_GetTicksMs() {
    fprintf(stderr, "ERROR: Not implemented: DG_GetTicksMs()");
    exit(1);
    return 0;
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    fprintf(stderr, "ERROR: Not implemented: DG_GetKey()");
    exit(1);
    return 0;
}

void DG_SetWindowTitle(const char * title) {
    fprintf(stderr, "ERROR: Not implemented: DG_SetWindowTitle()");
    exit(1);
}


int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    for (int i = 0; ; i++)
    {
        doomgeneric_Tick();
    }
    

    return 0;
}
