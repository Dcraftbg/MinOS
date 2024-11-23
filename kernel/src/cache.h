#pragma once
static void __invlpg(void* address) {
    asm volatile("invlpg (%0)" ::"r"(address) : "memory");
}
