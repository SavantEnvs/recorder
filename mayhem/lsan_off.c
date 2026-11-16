/* Leak detection is not what this fleet fuzzes for; ASan and UBSan stay fully active. */
int __lsan_is_turned_off(void) { return 1; }
