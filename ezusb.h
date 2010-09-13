#ifndef __EZUSBH__
#define __EZUSBH__

int ezusb_reset(struct libusb_device_handle *hdl, int set_clear);
int ezusb_install_firmware(libusb_device_handle *hdl, const char *filename);

#endif
