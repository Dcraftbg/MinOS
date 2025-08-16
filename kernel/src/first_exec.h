#pragma once
// Architecture dependent.
void* setup_user_first_exec(void* stack_page, uintptr_t user_entry, uintptr_t user_stack, uintptr_t argc, uintptr_t user_argv, uintptr_t envc, uintptr_t user_envv);
