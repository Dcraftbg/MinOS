[BITS 64]
PTR_SIZE equ 8
extern sys_open
extern sys_write
extern sys_read
extern sys_ioctl
extern sys_mmap
extern sys_seek
extern sys_close
extern sys_fork
extern sys_exec
extern sys_exit
extern sys_waitpid
extern sys_chdir
extern sys_getcwd
extern sys_diropen
extern sys_fstatx
extern sys_sleepfor
extern sys_gettime
extern sys_truncate
extern sys_epoll_create1
extern sys_epoll_ctl
extern sys_epoll_wait
extern sys_socket
extern sys_send
extern sys_recv
extern sys_accept
extern sys_bind
extern sys_listen
extern sys_connect
extern sys_get_dir_entries
extern sys_shmcreate
extern sys_shmmap
extern sys_shmrem
extern sys_sysctl
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
   dq sys_seek
   dq sys_close
   dq sys_fork
   dq sys_exec
   dq sys_exit
   dq sys_waitpid
   dq sys_chdir
   dq sys_getcwd
   dq sys_fstatx
   dq sys_sleepfor
   dq sys_gettime
   dq sys_truncate
   dq sys_epoll_create1
   dq sys_epoll_ctl
   dq sys_epoll_wait
   dq sys_socket
   dq sys_send
   dq sys_recv
   dq sys_accept
   dq sys_bind
   dq sys_listen
   dq sys_connect
   dq sys_get_dir_entries
   dq sys_shmcreate
   dq sys_shmmap
   dq sys_shmrem
   dq sys_sysctl
syscall_table_end:
section .text
global _irq_128
_irq_128:
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
