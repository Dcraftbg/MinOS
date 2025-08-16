[BITS 64]
section .text
extern task_switch
_task_switch:
    sub rsp, 512
    FXSAVE64 [rsp]
    call task_switch
    FXRSTOR64 [rsp]
    add rsp, 512
    ret

