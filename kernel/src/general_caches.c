#include "general_caches.h"
#include "kernel.h"
void init_general_caches() {
    assert(kernel.cache64  = create_new_cache(64 , "cache64" ));
    assert(kernel.cache256 = create_new_cache(256, "cache256"));
}
