void _start(void) {
    // Zero BSS
    extern char _bss_start, _bss_end;
    for (char *p = &_bss_start; p < &_bss_end; p++) {
        *p = 0;
    }
    
    // Call main
    extern int main(void);
    main();
    
    // Hang if main returns
    while(1);
}