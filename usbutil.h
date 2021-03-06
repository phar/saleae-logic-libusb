// vim: sw=8:ts=8:noexpandtab
#include <libusb.h>
#include "log.h"
/*
 * Iterates over the usb devices on the usb busses and returns a handle to the
 * first device found that matches the predefined vendor and product id
 */
libusb_device_handle *open_device(libusb_context * ctx, int vendor_id, int product_id);

const char *usbutil_transfer_status_to_string(enum libusb_transfer_status transfer_status);

const char *usbutil_error_to_string(enum libusb_error error);

enum libusb_error claim_device(libusb_device_handle * dev, int interface);
void usbutil_dump_device_descriptor(struct libusb_device_descriptor *device_descriptor);
