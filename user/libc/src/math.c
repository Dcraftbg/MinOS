#include "math.h"
// TODO: Faster fabs in assembly
float fabs(float f) {
    return f < 0.0 ? -f : f;
}
