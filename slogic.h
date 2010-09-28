// vim: sw=8:ts=8:noexpandtab
#ifndef __SLOGIC_H__
#define __SLOGIC_H__
#include <libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <zlib.h>


#define SLOGIC_COMPRESS_LEVEL 9
#define CHUNK  4096

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif


#define DEFAULT_N_TRANSFER_BUFFERS 4096
#define DEFAULT_TRANSFER_BUFFER_SIZE 4096 //(4 * 1024)
#define DEFAULT_TRANSFER_TIMEOUT 1000

/*
 * define EP1 OUT , EP1 IN, EP2 IN and EP6 OUT
 */
#define SALEAE_COMMAND_OUT_ENDPOINT 0x01
#define SALEAE_COMMAND_IN_ENDPOINT 0x81
#define SALEAE_STREAMING_DATA_IN_ENDPOINT 0x82
#define SALEAE_STREAMING_DATA_OUT_ENDPOINT 0x06

/* Bus 006 Device 006: ID 0925:3881 Lakeview Research */
#define USB_VENDOR_ID 0x0925
#define USB_PRODUCT_ID 0x3881



struct slogic_sample_rate {
	const uint8_t sample_delay;	/* sample rates are translated into sampling delays */
	const char *text;	/* A descriptive text for the sample rate ("24MHz") */
	const unsigned int samples_per_second;
};

enum slogic_recording_state {
	WARMING_UP = 0,
	INITALIZED	= 1,
	RUNNING = 2,
	SPINDOWN = 3,
	COMPLETED_SUCCESSFULLY = 4,
	ABORT = 5,
	DEVICE_GONE = 6,
	TIMEOUT = 7,
	STALL = 9,
	OVERFLOW = 9,
	DONE = 10,
	UNKNOWN = 100
};

/* returns an array of available sample rates. The array is terminates with an entry having a
0 sample_delay , a NULL text and a 0 samples_per_second */
struct slogic_sample_rate *slogic_get_sample_rates();
struct slogic_sample_rate *slogic_parse_sample_rate(const char *str);

#define SALEAE_LOGIC_COMMAND_SET_SAMPLE_DELAY	0x01

//todo, add packing on this struct
typedef struct slogic_command{	
	char		command;
	char		sample_delay;
}slogic_command;


typedef struct logic_transfers{
	struct libusb_transfer		*transfer;
	unsigned long				seq;
	void						*logic_context;
	unsigned long				transfer_id;
	unsigned long				state;
}logic_transfers;


/*
 * Contract between the main program and the utility library
 */
typedef struct slogic_ctx {
	/* pointer to the usb handle */
	libusb_device				*dev;
	libusb_device_handle		*device_handle;
	libusb_context				*usb_context;
	unsigned int				logic_index;
	
	//logic probe managemnt
	char						*fwfile;
	
	//sample data
	size_t						n_samples_requested;	
	size_t						n_samples_fulfilled;
	struct	slogic_sample_rate			*sample_rate;	
	//sampling run management stuff
	unsigned int				n_transfer_buffers;
	unsigned int				transfer_timeout;
	
	size_t						transfer_buffer_size;

	//output stuff
	int						(*data_callback_open)(struct slogic_ctx *handle,char * openstring);
	size_t						(*data_callback_write)(struct slogic_ctx *handle, uint8_t * data, size_t size);
	void						(*data_callback_close)(struct slogic_ctx *handle);	
	void						*data_callback_opts;	
	
	//state machine state
	unsigned int				recording_state;
	unsigned int				transfer_count;
	unsigned int				transfer_counter;
	struct logic_transfers		*transfers;
	z_stream 			strm;
}slogic_ctx;

struct slogic_ctx *slogic_init();
int slogic_open(struct slogic_ctx *handle,int logic_index);
void slogic_close(struct slogic_ctx *handle);
bool slogic_is_firmware_uploaded(struct slogic_ctx *handle);
void slogic_upload_firmware(struct slogic_ctx *handle);
int slogic_readbyte(struct slogic_ctx *handle, unsigned char *out);
typedef bool(*slogic_on_data_callback) (uint8_t * data, size_t size, void *user_data);
void slogic_read_samples_callback(struct libusb_transfer *transfer);
int slogic_execute_recording(struct slogic_ctx *handle);
int ezusb_upload_firmware(struct slogic_ctx *handle, int configuration, const char *filename);

#endif
