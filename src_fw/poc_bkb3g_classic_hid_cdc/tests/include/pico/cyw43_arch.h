// Host-test declarations only; never included by the firmware build.
#ifndef POC_TEST_CYW43_ARCH_H
#define POC_TEST_CYW43_ARCH_H
#define PICO_OK 0
int cyw43_arch_init(void);
void tight_loop_contents(void);
#endif
