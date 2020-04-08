#ifndef USBGADGETHELPER_H
#define USBGADGETHELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

int usbgadget_init();
int usbgadget_ms_init();
int usbgadget_ms_enable(const char *basemmcdev);
int usbgadget_ms_disable();
int usbgadget_ms_set_attrs(const char *serial, const char *productName, const uint16_t idProduct);
const char *usbgadget_ms_file();
bool usbgadget_ms_safe_to_remove();

int usbgadget_rndis_init();
int usbgadget_rndis_enable();
int usbgadget_rndis_disable();
int usbgadget_rndis_set_attrs(const char *serial, const char *productName, const uint16_t idProduct);

void usbgadget_cleanup();
const char *usbgadget_strerror();

#ifdef __cplusplus
}
#endif

#endif /* USBGADGETHELPER_H */
