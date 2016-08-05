#include "usbgadgethelper.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/usb/ch9.h>
#include <usbg/usbg.h>
#include <usbg/function/ms.h>
#include <usbg/function/net.h>

/* For mass storage, we can use Toradex product ids... */
usbg_gadget_attrs g_attrs_ms = {
    .bcdUSB = 0x0200,
    .bDeviceClass =	USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64, /* Max allowed ep0 packet size */
    .idVendor = 0x1B67,
    .idProduct = 0x401B,
    .bcdDevice = 0x0001, /* Verson of device */
};

/* For RNDIS, use NetChip RNDIS Gadget */
const  usbg_gadget_attrs g_attrs_rndis = {
    .bcdUSB = 0x0200,
    .bDeviceClass =	USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = 64, /* Max allowed ep0 packet size */
    .idVendor = 0x0525,
    .idProduct = 0xa4a2,
    .bcdDevice = 0x0001, /* Verson of device */
};

usbg_gadget_strs g_strs = {
    .str_ser = "00000000", /* Serial number */
    .str_mnf = "Toradex", /* Manufacturer */
    .str_prd = "Apalis iMX6" /* Product string */
};

struct usbg_f_ms_lun_attrs f_ms_luns_array[] = {
    {
        .id = -1,
        .cdrom = 0,
        .ro = 0,
        .nofua = 0,
        .removable = 1,
        .file = "/dev/mmcblk0",
    },
    {
        .id = -1,
        .cdrom = 0,
        .ro = 1,
        .nofua = 0,
        .removable = 1,
        .file = "/dev/mmcblk0boot0",
    },
    {
        .id = -1,
        .cdrom = 0,
        .ro = 1,
        .nofua = 0,
        .removable = 1,
        .file = "/dev/mmcblk0boot1",
    }
};

struct usbg_f_ms_lun_attrs *f_ms_luns[] = {
    &f_ms_luns_array[0],
    &f_ms_luns_array[1],
    &f_ms_luns_array[2],
    NULL,
};

struct usbg_f_ms_attrs f_ms_attrs = {
    .stall = 0,
    .nluns = 2,
    .luns = f_ms_luns,
};

usbg_config_strs c_strs_ms = {
        "Mass Storage"
};

/* RNDIS */
struct usbg_f_net_attrs f_net_attrs = {
    .dev_addr = { .ether_addr_octet = { 0x00, 0x14, 0x2d, 0xff, 0xff, 0xff } },
    .host_addr = { .ether_addr_octet = { 0x00, 0x14, 0x2d, 0xff, 0xff, 0xfe } },
    .qmult = 5,
};

usbg_config_strs c_strs_rndis = {
    .configuration = "RNDIS"
};

usbg_state *s;
usbg_gadget *g_ms;
usbg_function *f_ms;

usbg_gadget *g_rndis;
usbg_function *f_rndis;

usbg_error usbg_ret;

int usbgadget_init(const char *serial, const char *productName, uint16_t idProduct)
{
    strncpy(g_strs.str_ser, serial, USBG_MAX_STR_LENGTH);
    strncpy(g_strs.str_prd, productName, USBG_MAX_STR_LENGTH);
    g_attrs_ms.idProduct = idProduct;

    usbg_ret = (usbg_error)usbg_init("/sys/kernel/config", &s);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    /* Delete old states if there... */
    /*
     * This is supposed to work, but with more complex configurations there
     * seems to be a problem in rereading the configuration currenlty:
     * usbg_parse_state()  unable to parse /sys/kernel/config/usb_gadget
     *
     * This is only problematic in development, just get rid of config
     * manually by using:
     * rm -rf /sys/kernel/config/usb_gadget/
     */
/*
    g_ms = usbg_get_gadget(s, "gms");
    if (g_ms)
        usbg_rm_gadget(g_ms, USBG_RM_RECURSE);
    g_rndis = usbg_get_gadget(s, "grndis");
    if (g_rndis)
        usbg_rm_gadget(g_rndis, USBG_RM_RECURSE);
*/
    return 0;
}

int usbgadget_ms_init()
{
    usbg_config *c;

    usbg_ret = (usbg_error)usbg_create_gadget(s, "gms", &g_attrs_ms, &g_strs, &g_ms);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    usbg_ret = (usbg_error)usbg_create_function(g_ms, F_MASS_STORAGE, "",
                    &f_ms_attrs, &f_ms);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    /* NULL can be passed to use kernel defaults */
    usbg_ret = usbg_create_config(g_ms, 1, "Mass Storage", NULL, &c_strs_ms, &c);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    usbg_ret = usbg_add_config_function(c, "Mass Storage Function", f_ms);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    return 0;
}

/*
 * Only assume disconnect if state is "not attached",
 * assume connected in all other/error cases
 */
static bool usbgadget_udc_connected(usbg_udc *udc)
{
    int fd;
    char file[256];
    char state[128] = {};

    if (snprintf(file, 256, "/sys/class/udc/%s/state", usbg_get_udc_name(udc)) < 0)
        return true;

    fd = open(file, O_RDONLY);
    if (!fd)
        return true;

    if (read(fd, state, 128)) {
        if (!strncmp(state, "not attached", strlen("not attached")))
            return false;
    }

    return true;
}

bool usbgadget_ms_safe_to_remove()
{
    usbg_f_ms *mf = usbg_to_ms_function(f_ms);
    int lun = 0;
    bool ro;
    char *file;
    usbg_udc *udc;

    /* Check if USB gadget is connected at all */
    udc = usbg_get_gadget_udc(g_ms);
    if (!udc)
        return false;

    if (!usbgadget_udc_connected(udc))
        return true;

    /* Check all disks... */
    for (lun = 0; lun < 3; lun++) {
        /* ...they either need to be ro... */
        usbg_f_ms_get_lun_ro(mf, lun, &ro);
        if (ro)
            continue;

        /* ...or file need to be empty (=> removed from host side) */
        usbg_f_ms_get_lun_file(mf, lun, &file);
        if (file[0] != '\0') {
            free(file);
            return false;
        }
        free(file);
    }

    return true;
}

int usbgadget_ms_enable()
{
    usbg_f_ms *mf = usbg_to_ms_function(f_ms);
    usbg_f_ms_set_lun_file(mf, 0, "/dev/mmcblk0");
    usbg_f_ms_set_lun_file(mf, 1, "/dev/mmcblk0boot0");
    usbg_f_ms_set_lun_file(mf, 2, "/dev/mmcblk0boot1");

    usbg_ret = usbg_enable_gadget(g_ms, DEFAULT_UDC);
    if (usbg_ret != USBG_SUCCESS)
        return -1;
    return 0;
}

int usbgadget_ms_disable()
{
    usbg_ret = usbg_disable_gadget(g_ms);
    if (usbg_ret != USBG_SUCCESS)
        return -1;
    return 0;
}

const char *usbgadget_ms_file()
{
    char *file;

    usbg_f_ms *mf = usbg_to_ms_function(f_ms);
    usbg_f_ms_get_lun_file(mf, 0, &file);

    return file;
}

void usbgadget_cleanup()
{
    usbg_cleanup(s);
}

const char *usbgadget_strerror()
{
    return usbg_strerror(usbg_ret);
}

int usbgadget_rndis_init()
{
    usbg_config *c;

    usbg_ret = (usbg_error)usbg_create_gadget(s, "grndis", &g_attrs_rndis, &g_strs, &g_rndis);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    usbg_ret = (usbg_error)usbg_create_function(g_rndis, F_RNDIS, "usb0",
                    &f_net_attrs, &f_rndis);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    /* NULL can be passed to use kernel defaults */
    usbg_ret = usbg_create_config(g_rndis, 1, "RNDIS", NULL, &c_strs_rndis, &c);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    usbg_ret = usbg_add_config_function(c, "RNDIS Function", f_rndis);
    if (usbg_ret != USBG_SUCCESS)
        return -1;

    return 0;
}

int usbgadget_rndis_enable()
{
    usbg_ret = usbg_enable_gadget(g_rndis, DEFAULT_UDC);
    if (usbg_ret != USBG_SUCCESS)
        return -1;
    return 0;
}

int usbgadget_rndis_disable()
{
    usbg_ret = usbg_disable_gadget(g_rndis);
    if (usbg_ret != USBG_SUCCESS)
        return -1;
    return 0;
}

