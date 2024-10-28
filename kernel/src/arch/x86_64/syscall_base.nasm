[BITS 64]
%include "asmstd.inc"
extern sys_open
extern sys_write
extern sys_read
extern sys_ioctl
extern sys_mmap
extern sys_close
extern sys_fork
extern sys_exec
extern sys_exit
extern sys_waitpid
extern sys_heap_create
extern sys_heap_allocate
extern sys_heap_deallocate
extern sys_chdir
extern sys_getcwd
%define KERNEL_UNSUPPORTED 7
section .text
global syscall_base
section .rodata
syscall_table:
   dq sys_open
   dq sys_write
   dq sys_read
   dq sys_ioctl
   dq sys_mmap
   dq sys_close
   dq sys_fork
   dq sys_exec
   dq sys_exit
   dq sys_waitpid
   dq sys_heap_create
   dq sys_heap_allocate
   dq sys_heap_deallocate
   dq sys_chdir
   dq sys_getcwd
syscall_table_end:
section .text
syscall_base:
   cmp rax, (syscall_table_end-syscall_table)/PTR_SIZE
   jge .unsupported
   push rbx
   push rcx
   push rdx
   push rsi
   push rdi
   push rbp
   push r8
   push r9
   push r10
   push r11
   push r12
   push r13
   push r14
   push r15
   xor rbp, rbp
   call [syscall_table + rax * PTR_SIZE]
   pop r15
   pop r14
   pop r13
   pop r12
   pop r11
   pop r10
   pop r9
   pop r8
   pop rbp
   pop rdi
   pop rsi
   pop rdx
   pop rcx
   pop rbx
   iretq
.unsupported:
   mov rax, -KERNEL_UNSUPPORTED
   iretq
