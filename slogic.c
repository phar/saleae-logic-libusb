#include "slogic.h"
#include "usbutil.h"
#include "log.h"
#include "main.h"
#include "ezusb.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

/*
 * Sample Rates
 */

struct slogic_sample_rate sample_rates[] = {
	{1, "24MHz", 24000000},
	{2, "16MHz", 16000000},
	{3, "12MHz", 12000000},
	{5, "8MHz", 8000000},
	{11, "4MHz", 4000000},
	{24, "2MHz", 2000000},
	{47, "1MHz", 1000000},
	{95, "500kHz", 500000},
	{191, "250kHz", 250000},
	{239, "200kHz", 200000},
	{0, NULL, 0},
};

struct slogic_sample_rate *slogic_get_sample_rates(){
	return sample_rates;
}

struct slogic_sample_rate *slogic_parse_sample_rate(const char *str){
	struct slogic_sample_rate *sample_rate = slogic_get_sample_rates();
	while (sample_rate->text != NULL) {
		if (strcmp(sample_rate->text, str) == 0) {
			return sample_rate;
		}
		sample_rate++;
	}
	return NULL;
}


/* return 1 if the firmware is uploaded 0 if not */
bool slogic_is_firmware_uploaded(struct slogic_ctx *handle){
/* just try to perform a normal read, if this fails we assume the firmware is not uploaded */
unsigned char out_byte = 0x05;
int transferred;
int ret;
	
	ret = libusb_bulk_transfer(handle->device_handle, SALEAE_COMMAND_OUT_ENDPOINT, &out_byte, 1, &transferred, 100);
	return ret == 0;	/* probably the firmware is uploaded */
}

/*
 * TODO: An error code is probably required here as there can be many reasons
 * to why open() fails. The handle should probably be an argument passed as
 * a pointer to a pointer.
 */
struct slogic_ctx *slogic_init(){
struct slogic_ctx *handle;
	
	
	
	handle = calloc(1,sizeof(struct slogic_ctx));
	assert(handle);
	
	handle->transfer_buffer_size = DEFAULT_TRANSFER_BUFFER_SIZE;
	handle->n_transfer_buffers = DEFAULT_N_TRANSFER_BUFFERS;
	handle->transfer_timeout = DEFAULT_TRANSFER_TIMEOUT;
	handle->transfer_timeout = 1000;
	libusb_init(&handle->usb_context);	
	return handle;
}



int slogic_open(struct slogic_ctx *handle, int logic_index){
int err,i;	
size_t cnt;
libusb_device **list;
libusb_device *found = NULL;
struct libusb_device_descriptor descriptor;	
	
	cnt = libusb_get_device_list(handle->usb_context, &list);
	i = 0;
	if (cnt < 0) {
		log_printf( DEBUG,  "Failed to get a list of devices\n");
		return 0;
	}
	
	for (i = 0; i < cnt; i++) {
		libusb_device *device = list[i];
		err = libusb_get_device_descriptor(device, &descriptor);
		if (err) {
			log_printf( DEBUG,  "libusb_get_device_descriptor: %s\n", usbutil_error_to_string(err));
			libusb_free_device_list(list, 1);
			return 0;
		}
		if ((descriptor.idVendor == USB_VENDOR_ID) && (descriptor.idProduct == USB_PRODUCT_ID)) {
			found = device;
			usbutil_dump_device_descriptor(&descriptor);
			break;
		}
	}
	
	if (!found) {
		log_printf( DEBUG,  "Device not found\n");
		libusb_free_device_list(list, 1);
		return 0;
	}
	
	if ((err = libusb_open(found, &handle->device_handle))) {
		log_printf( DEBUG,  "Failed OPEN the device: %s\n", usbutil_error_to_string(err));
		libusb_free_device_list(list, 1);
		return 0;
	}
	log_printf( DEBUG,  "libusb_open: %s\n", usbutil_error_to_string(err));	
	
	if ((err = claim_device(handle->device_handle, 0)) != 0) {
		log_printf( DEBUG, "Failed to claim the usb interface: %s\n", usbutil_error_to_string(err));
		libusb_free_device_list(list, 1);
		return 0;
	}	
	
	
	if (!handle->device_handle) {
		log_printf( ERR, "Failed to open the device\n");
		libusb_free_device_list(list, 1);
		return -1;
	}
	libusb_free_device_list(list, 1);
	handle->dev = libusb_get_device(handle->device_handle);
	handle->transfers = calloc(1,sizeof(struct logic_transfers) * handle->n_transfer_buffers);

	return 0;
}

void slogic_close(struct slogic_ctx *handle){	
	libusb_close(handle->device_handle);
	libusb_exit(handle->usb_context);
	free(handle->transfers);
	free(handle);
}


int slogic_prime_data(struct slogic_ctx *handle, unsigned int transfer_id){
char * buffer;	
struct libusb_transfer *newtransfer;	

	buffer = malloc(handle->transfer_buffer_size);
	assert(buffer);
	newtransfer = libusb_alloc_transfer(0);
	newtransfer->flags |= (LIBUSB_TRANSFER_FREE_BUFFER |  LIBUSB_TRANSFER_FREE_TRANSFER );
	if (newtransfer == NULL) {
		log_printf( ERR, "libusb_alloc_transfer failed\n");
		handle->recording_state = UNKNOWN;
		return 1;
	}
	
	handle->transfers[transfer_id].logic_context = handle;
	handle->transfers[transfer_id].transfer = newtransfer;
	handle->transfers[transfer_id].transfer_id = transfer_id;
	handle->transfers[transfer_id].state = 0;
	
//FIXME return value?
	libusb_fill_bulk_transfer(newtransfer, handle->device_handle, SALEAE_STREAMING_DATA_IN_ENDPOINT, (unsigned char *)buffer, handle->transfer_buffer_size,slogic_read_samples_callback,&handle->transfers[transfer_id], handle->transfer_timeout);
	return 0;

	
}

int slogic_pump_data(struct slogic_ctx *handle, unsigned int transfer_id){
int retval;
	
	
	if((retval = libusb_submit_transfer(handle->transfers[transfer_id].transfer))){
		handle->transfers[transfer_id].state = 1;
		log_printf( ERR, "libusb_submit_transfer: %s\n", usbutil_error_to_string(retval));
		handle->recording_state = UNKNOWN;
		return 1;
	}
	return 0;
}

/*
i need to reissue another transfer here as fast as i possibly can.. 
*/

int slogic_spindown(struct slogic_ctx *handle){
	unsigned int transfer_id;
	
	for (transfer_id = 0; transfer_id < handle->n_transfer_buffers; transfer_id++) {
		if(handle->transfers[transfer_id].state == 1){
			libusb_cancel_transfer(handle->transfers[transfer_id].transfer);
			libusb_free_transfer(handle->transfers[transfer_id].transfer);
			handle->transfers[transfer_id].state = 0;
		}
	}
	return 1; //did i want to do something with this?
}

void dummy_callback(struct libusb_transfer *transfer){
	printf("moof");
}


void slogic_read_samples_callback(struct libusb_transfer *transfer){
struct logic_transfers *ltransfer = transfer->user_data;
struct slogic_ctx *handle = ltransfer->logic_context;

	switch(transfer->status){	
		case LIBUSB_TRANSFER_COMPLETED :
			handle->transfer_count--;
			handle->transfers[ltransfer->transfer_id].seq = handle->transfer_counter++;
			handle->n_samples_fulfilled += transfer->actual_length;
			handle->data_callback_write(handle,transfer->buffer,transfer->actual_length);

			
			if(handle->n_samples_fulfilled < handle->n_samples_requested){
				slogic_prime_data(handle, ltransfer->transfer_id);
				slogic_pump_data(handle, ltransfer->transfer_id);
			}else{
				handle->recording_state = SPINDOWN;
				handle->transfers[ltransfer->transfer_id].state = 0;
				slogic_spindown(handle);
				handle->recording_state = COMPLETED_SUCCESSFULLY;
			}
			if((handle->recording_state == ABORT) || (handle->recording_state == COMPLETED_SUCCESSFULLY)){
				handle->recording_state = SPINDOWN;
				handle->transfers[ltransfer->transfer_id].state = 0;
				slogic_spindown(handle);
				handle->recording_state = COMPLETED_SUCCESSFULLY;
			}
			break;
			
		case LIBUSB_TRANSFER_TIMED_OUT:
			handle->recording_state = TIMEOUT;
			break;
			
		case LIBUSB_TRANSFER_CANCELLED: //nothing to do here, its being handled
			break;
			
		case LIBUSB_TRANSFER_STALL: 	 
			handle->recording_state = STALL;
			break;
			
		case LIBUSB_TRANSFER_NO_DEVICE:
			handle->recording_state = DEVICE_GONE;
			break;
			
		case LIBUSB_TRANSFER_OVERFLOW:
			handle->recording_state = OVERFLOW;
			break;
			
		case LIBUSB_TRANSFER_ERROR:
		default:
			handle->recording_state = UNKNOWN;
	}
		
}



int slogic_set_capture(struct slogic_ctx *handle){
struct libusb_transfer *transfer;
struct slogic_command command;	
int ret,transferred;
	
	transfer = libusb_alloc_transfer(0);

	if (transfer == NULL) {
		log_printf( ERR, "libusb_alloc_transfer failed\n");
		handle->recording_state = UNKNOWN;
		ret = 1;
		return ret;
	}

	command.command = SALEAE_LOGIC_COMMAND_SET_SAMPLE_DELAY;
	command.sample_delay = handle->sample_rate->sample_delay;	

	if((ret = libusb_bulk_transfer(handle->device_handle, SALEAE_COMMAND_OUT_ENDPOINT, (unsigned char *)&command, sizeof(command), &transferred, handle->transfer_timeout))){
		log_printf( ERR, "libusb_bulk_transfer (in): %s\n", usbutil_error_to_string(ret));
		return ret;
	}
	return 0;	
	
}

int slogic_set_capture_async(struct slogic_ctx *handle){
	struct libusb_transfer *transfer;
	struct slogic_command command;	
	int ret,transferred;
	
	transfer = libusb_alloc_transfer(0);
	
	if (transfer == NULL) {
		log_printf( ERR, "libusb_alloc_transfer failed\n");
		handle->recording_state = UNKNOWN;
		ret = 1;
		return ret;
	}
	
	command.command = SALEAE_LOGIC_COMMAND_SET_SAMPLE_DELAY;
	command.sample_delay = handle->sample_rate->sample_delay;	
	
	libusb_fill_bulk_transfer(transfer, handle->device_handle, SALEAE_COMMAND_OUT_ENDPOINT, (unsigned char *)&command, sizeof(command),dummy_callback,&transferred, handle->transfer_timeout);
	libusb_submit_transfer(transfer);	
	return 0;	
	
}


int slogic_execute_recording(struct slogic_ctx *handle){
int transfer_id,retval,ret;
struct timeval timeout;	


	handle->recording_state = WARMING_UP;
	
	for (transfer_id = 0; transfer_id < handle->n_transfer_buffers; transfer_id++) {
		slogic_prime_data(handle, transfer_id);
	}
		
	handle->recording_state = RUNNING;


	for (transfer_id = 0; transfer_id < handle->n_transfer_buffers; transfer_id++) {
		if(!transfer_id){
			slogic_set_capture_async(handle);
		}
		slogic_pump_data(handle, transfer_id);
		handle->transfer_count++;
	}


	while (handle->recording_state == RUNNING) {		
		timeout.tv_sec = 1;
		if((ret = libusb_handle_events_timeout(handle->usb_context, &timeout))){
			log_printf( ERR, "libusb_handle_events: %s\n", usbutil_error_to_string(ret));
			break;
		}
	}

	//spindown!





	if (handle->recording_state == COMPLETED_SUCCESSFULLY) {
		log_printf(INFO, "Capture Success!\n");
	}else{	
		log_printf( ERR, "Capture Fail! recording_state=%d\n", handle->recording_state);
		retval = 1;
	}


	return retval;
}


int hex_data_callback_open(struct slogic_ctx *handle,char * openstring){
struct callback_test *foo;
                
//        handle->data_callback_opts = calloc(1,sizeof(struct callback_test));
//        foo = handle->data_callback_opts;

	return 1;
}       
                
int hex_data_callback_write(struct slogic_ctx *handle, uint8_t * data, size_t size){
struct callback_test *foo = handle->data_callback_opts;
                 
        return 0;
}
                 
void hex_data_callback_close(struct slogic_ctx *handle){                                        
struct callback_test *foo = handle->data_callback_opts;

//        free(foo);
        handle->data_callback_opts = 0;
        return ;
}

int ezusb_upload_firmware(struct slogic_ctx *handle, int configuration, const char *filename){

        log_printf(DEBUG, "uploading firmware to device on %d.%d\n", libusb_get_bus_number(handle->dev), libusb_get_device_address(handle->dev));
                
        if ((ezusb_reset(handle->device_handle, 1)) < 0)
                return 1;
  
        if (ezusb_install_firmware(handle->device_handle, filename) != 0)
                return 1;

        if ((ezusb_reset(handle->device_handle, 0)) < 0)
                return 1;
        
        return 0;
}

