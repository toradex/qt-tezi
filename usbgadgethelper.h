#ifndef USBGADGETHELPER_H
#define USBGADGETHELPER_H

#include <stdbool.h>

int usbgadget_init();
int usbgadget_ms_init();
int usbgadget_ms_enable();
int usbgadget_ms_disable();
const char *usbgadget_ms_file();

bool usbgadget_ms_safe_to_remove();

void usbgadget_cleanup();
const char *usbgadget_strerror();

#endif /* USBGADGETHELPER_H */
