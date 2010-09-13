#include "usbutil.h"
#include <stdio.h>

/*
 * Data structure debugging.
 */

static void dump_endpoint_descriptor(int i, const struct libusb_endpoint_descriptor *endpoint_descriptor){
	char *direction = ((endpoint_descriptor->bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN) ? "in" : "out";

	log_printf( DEBUG,  "     Endpoint #%d\n", i);
	log_printf( DEBUG,  "      Address: %d, direction=%s\n", endpoint_descriptor->bEndpointAddress & 0x0f, direction);
	log_printf( DEBUG,  "      Attributes: %02x\n", endpoint_descriptor->bmAttributes);
	log_printf( DEBUG,  "      Max packet size: %u\n", endpoint_descriptor->wMaxPacketSize);
	log_printf( DEBUG,  "      Poll interval: %d\n", endpoint_descriptor->bInterval);
	log_printf( DEBUG,  "      Refresh: %d\n", endpoint_descriptor->bRefresh);
	log_printf( DEBUG,  "      Sync address: %d\n", endpoint_descriptor->bSynchAddress);
}

static void dump_interface( int i, const struct libusb_interface *interface){
	log_printf( DEBUG,  "  Interface #%d: Descriptors: (%d)\n", i, interface->num_altsetting);
	const struct libusb_interface_descriptor *interface_descriptor = interface->altsetting;
	int j, k;
	for (j = 0; j < interface->num_altsetting; j++, interface_descriptor++) {
		log_printf( DEBUG,  "   Descriptor #%d:\n", j);
		log_printf( DEBUG,  "    Interface class/sub-class: %d/%d\n",
			interface_descriptor->bInterfaceClass, interface_descriptor->bInterfaceSubClass);
		log_printf( DEBUG,  "    Protocol: %d\n", interface_descriptor->bInterfaceProtocol);
		log_printf( DEBUG,  "    Endpoints: (%d)\n", interface_descriptor->bNumEndpoints);
		const struct libusb_endpoint_descriptor
		*endpoint_descriptor = interface_descriptor->endpoint;
		for (k = 0; k < interface_descriptor->bNumEndpoints; k++, endpoint_descriptor++) {
			dump_endpoint_descriptor(k, endpoint_descriptor);
		}
	}
}

void usbutil_dump_config_descriptor(struct libusb_config_descriptor *config_descriptor){
	/* TODO: Decode bytes to strings */
	log_printf( DEBUG,  "Configuration descriptor:\n");
	log_printf( DEBUG,  " Configuration id: %d\n", config_descriptor->bConfigurationValue);
	log_printf( DEBUG,  " Interfaces (%d):\n", config_descriptor->bNumInterfaces);

	const struct libusb_interface *interface = config_descriptor->interface;
	int i;
	for (i = 0; i < config_descriptor->bNumInterfaces; i++, interface++) {
		dump_interface(i, interface);
	}
}

void usbutil_dump_device_descriptor(struct libusb_device_descriptor *device_descriptor){
	/* TODO: Decode bytes to strings */
	log_printf( DEBUG,  "Device descriptor:\n");
	log_printf( DEBUG,  " Class/Sub-class: %04x/%04x\n", device_descriptor->bDeviceClass,
		device_descriptor->bDeviceSubClass);
	log_printf( DEBUG,  " Protocol: %d\n", device_descriptor->bDeviceProtocol);
	log_printf( DEBUG,  " Vendor id / product id: %04x / %04x\n", device_descriptor->idVendor,
		device_descriptor->idProduct);
	log_printf( DEBUG,  " Number of possible configurations: %d\n", device_descriptor->bNumConfigurations);
}

/*
 * Open / Close
 */

/*
 * This method looks if the kernel already has a driver attached to the device. if so I will take over the device.
 */
enum libusb_error claim_device(libusb_device_handle * dev, int interface)
{
	enum libusb_error err;
	if (libusb_kernel_driver_active(dev, interface)) {
		log_printf( DEBUG,  "A kernel has claimed the interface, detaching it...\n");
		if ((err = libusb_detach_kernel_driver(dev, interface)) != 0) {
			log_printf( DEBUG,  "Failed to Disconnected the OS driver: %s\n", usbutil_error_to_string(err));
			return err;
		}
	}

	if ((err = libusb_set_configuration(dev, 1))) {
		log_printf( DEBUG, "libusb_set_configuration: %s\n", usbutil_error_to_string(err));
		return err;
	}
	log_printf( DEBUG,  "libusb_set_configuration: %s\n", usbutil_error_to_string(err));

	/* claim interface */
	if ((err = libusb_claim_interface(dev, interface))) {
		log_printf( DEBUG,  "Claim interface error: %s\n", usbutil_error_to_string(err));
		return err;
	}
	log_printf( DEBUG,  "libusb_claim_interface: %s\n", usbutil_error_to_string(err));

	if ((err = libusb_set_interface_alt_setting(dev, interface, 0))) {
		log_printf( DEBUG, "libusb_set_interface_alt_setting: %s\n", usbutil_error_to_string(err));
		return err;
	}
	log_printf( DEBUG,  "libusb_set_interface_alt_setting: %s\n", usbutil_error_to_string(err));

	return LIBUSB_SUCCESS;
}

/*
 * Iterates over the usb devices on the usb busses and returns a handle to the
 * first device found that matches the predefined vendor and product id
 */
libusb_device_handle *open_device(libusb_context * ctx, int vendor_id, int product_id)
{
	// discover devices
	libusb_device **list;
	libusb_device *found = NULL;
	libusb_device_handle *device_handle = NULL;
	struct libusb_device_descriptor descriptor;

	size_t cnt = libusb_get_device_list(ctx, &list);
	size_t i = 0;
	int err = 0;
	if (cnt < 0) {
		log_printf( DEBUG,  "Failed to get a list of devices\n");
		return NULL;
	}

	for (i = 0; i < cnt; i++) {
		libusb_device *device = list[i];
		err = libusb_get_device_descriptor(device, &descriptor);
		if (err) {
			log_printf( DEBUG, "libusb_get_device_descriptor: %s\n", usbutil_error_to_string(err));
			libusb_free_device_list(list, 1);
			return NULL;
		}
		if ((descriptor.idVendor == vendor_id) && (descriptor.idProduct == product_id)) {
			found = device;
			usbutil_dump_device_descriptor(&descriptor);
			break;
		}
	}

	if (!found) {
		log_printf( DEBUG,  "Device not found\n");
		libusb_free_device_list(list, 1);
		return NULL;
	}

	if ((err = libusb_open(found, &device_handle))) {
		log_printf( DEBUG,  "Failed OPEN the device: %s\n", usbutil_error_to_string(err));
		libusb_free_device_list(list, 1);
		return NULL;
	}
	log_printf( DEBUG,  "libusb_open: %s\n", usbutil_error_to_string(err));

	libusb_free_device_list(list, 1);

	if ((err = claim_device(device_handle, 0)) != 0) {
		log_printf( DEBUG, "Failed to claim the usb interface: %s\n", usbutil_error_to_string(err));
		return NULL;
	}

	struct libusb_config_descriptor *config_descriptor;
	err = libusb_get_active_config_descriptor(found, &config_descriptor);
	if (err) {
		log_printf( DEBUG,  "libusb_get_active_config_descriptor: %s\n", usbutil_error_to_string(err));
		return NULL;
	}
	log_printf( DEBUG,  "Active configuration:%d\n", config_descriptor->bConfigurationValue);
	libusb_free_config_descriptor(config_descriptor);

	log_printf( DEBUG, "Available configurations (%d):\n", descriptor.bNumConfigurations);
	for (i = 0; i < descriptor.bNumConfigurations; i++) {
		err = libusb_get_config_descriptor(found, i, &config_descriptor);
		if (err) {
			log_printf( DEBUG,  "libusb_get_config_descriptor: %s\n", usbutil_error_to_string(err));
			return NULL;
		}

		usbutil_dump_config_descriptor(config_descriptor);
		libusb_free_config_descriptor(config_descriptor);
	}

	return device_handle;
}

const char *usbutil_transfer_status_to_string(enum libusb_transfer_status transfer_status)
{
	switch (transfer_status) {
	case LIBUSB_TRANSFER_COMPLETED:
		return "Completed.";
	case LIBUSB_TRANSFER_ERROR:
		return "Error.";
	case LIBUSB_TRANSFER_TIMED_OUT:
		return "Timeout.";
	case LIBUSB_TRANSFER_CANCELLED:
		return "Cancelled.";
	case LIBUSB_TRANSFER_STALL:
		return "Stalled.";
	case LIBUSB_TRANSFER_NO_DEVICE:
		return "No device.";
	case LIBUSB_TRANSFER_OVERFLOW:
		return "Overflow.";
	default:
		return "libusb_transfer_status: Unkown.";
	}
}

const char *usbutil_error_to_string(enum libusb_error error)
{
	switch (error) {
	case LIBUSB_SUCCESS:
		return "LIBUSB_SUCCESS";
	case LIBUSB_ERROR_IO:
		return "LIBUSB_ERROR_IO";
	case LIBUSB_ERROR_INVALID_PARAM:
		return "LIBUSB_ERROR_INVALID_PARAM";
	case LIBUSB_ERROR_ACCESS:
		return "LIBUSB_ERROR_ACCESS";
	case LIBUSB_ERROR_NO_DEVICE:
		return "LIBUSB_ERROR_NO_DEVICE";
	case LIBUSB_ERROR_NOT_FOUND:
		return "LIBUSB_ERROR_NOT_FOUND";
	case LIBUSB_ERROR_BUSY:
		return "LIBUSB_ERROR_BUSY";
	case LIBUSB_ERROR_TIMEOUT:
		return "LIBUSB_ERROR_TIMEOUT";
	case LIBUSB_ERROR_OVERFLOW:
		return "LIBUSB_ERROR_OVERFLOW";
	case LIBUSB_ERROR_PIPE:
		return "LIBUSB_ERROR_PIPE";
	case LIBUSB_ERROR_INTERRUPTED:
		return "LIBUSB_ERROR_INTERRUPTED";
	case LIBUSB_ERROR_NO_MEM:
		return "LIBUSB_ERROR_NO_MEM";
	case LIBUSB_ERROR_NOT_SUPPORTED:
		return "LIBUSB_ERROR_NOT_SUPPORTED";
	case LIBUSB_ERROR_OTHER:
		return "LIBUSB_ERROR_OTHER";
	default:
		return "libusb_error: Unknown error";
	}
}
