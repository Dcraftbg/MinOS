// NOTE: not SYS-V compliant.
typedef int (*main_t)(int argc, const char** argv, const char** envp);
void __libc_start_main(int argc, char** argv, char** envp, void** reserved, main_t main) {
    _libc_init_environ(envp);
    _libc_init_streams();
    exit(main(argc, argv, envp));
}
