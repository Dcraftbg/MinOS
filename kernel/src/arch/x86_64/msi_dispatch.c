#include <stdint.h>
#include <msi.h>
#include <assert.h>
typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t msi;
} MSIFrame; // IDT Exception Frame
extern void msi_dispatch(MSIFrame* frame) {
    debug_assert(frame->msi < MSI_COUNT);
    debug_assert(msi_manager.pci_devices[frame->msi]);
    msi_manager.pci_devices[frame->msi]->handler(msi_manager.pci_devices[frame->msi]);
}
