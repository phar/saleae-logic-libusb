// vim: sw=8:ts=8:noexpandtab
#include "slogic.h"
#include "usbutil.h"
#include "log.h"
#include "main.h"
#include "ezusb.h"
#include <assert.h>
#include <libusb.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* Command line arguments */

int user_forced_shutdown = 0;
char *outputfilename = "saleae_output.bin";


void short_usage(int argc, char **argv,const char *message, ...){
	char p[1024]; //FIXME
	va_list ap;

	printf( "usage: %s -f <output file> -r <sample rate> [-n <number of samples>] [-h ] \n\n", argv[0]);
	va_start(ap, message);
	(void)vsnprintf(p, 1024, message, ap);
	va_end(ap);
	printf( "Error: %s\n", p);
}

void full_usage(int argc, char **argv){
	const struct slogic_sample_rate *sample_iterator = slogic_get_sample_rates();

	printf( "usage: %s -f <output file> -r <sample rate> [-n <number of samples>]\n", argv[0]);
	printf( "\n");
	printf( " -n: Number of samples to record\n");
	printf( "     Defaults to one second of samples for the specified sample rate\n");
	printf( " -f: The output file. Using '-' means that the bytes will be output to stdout.\n");
	printf( " -h: This help message.\n");
	printf( " -r: Select sample rate for the Logic.\n");
	printf( "     Available sample rates:\n");
	while (sample_iterator->text != NULL) {
		printf( "      o %s\n", sample_iterator->text);
		sample_iterator++;
	}
	printf( "\n");
	printf( "Advanced options:\n");
	printf( " -b: Transfer buffer size.\n");
	printf( " -t: Number of transfer buffers.\n");
	printf( " -o: Transfer timeout.\n");
	printf( " -u: libusb debug level: 0 to 3, 3 is most verbose. Defaults to '0'.\n");
	printf( " -d: log level: 0 to 5, 5 is most verbose. Defaults to '1'.\n");
	printf( "\n");
}

/* Returns true if everything was OK */
bool parse_args(int argc, char **argv, struct slogic_ctx *handle){
char c;
int libusb_debug_level = 0;
char *endptr;
	
	optind = 1; //reset incase i need to reparse
	/* TODO: Add a -d flag to turn on internal debugging */
	while ((c = getopt(argc, argv, "n:f:r:hb:t:o:u:d:s:")) != -1) {
		switch (c) {
		case 'n':
			handle->n_samples_requested = strtol(optarg, &endptr, 10);
			if (*endptr != '\0' || handle->n_samples_requested <= 0) {
				short_usage(argc,argv,"Invalid number of samples, must be a positive integer: %s", optarg);
				return false;
			}
			break;

		case 'f':
			handle->fwfile = optarg;
			break;

		case 's':
			outputfilename = optarg;
			break;

		case 'r':
			handle->sample_rate = slogic_parse_sample_rate(optarg);
			if (!handle->sample_rate) {
				short_usage(argc,argv,"Invalid sample rate: %s", optarg);
				return false;
			}
			break;
		case 'h':
			full_usage(argc,argv);
			return false;
		case 'b':
			handle->transfer_buffer_size = strtol(optarg, &endptr, 10);
			if (*endptr != '\0' || handle->transfer_buffer_size <= 0) {
				short_usage(argc,argv,"Invalid transfer buffer size, must be a positive integer: %s", optarg);
				return false;
			}
			break;
		case 't':
			handle->n_transfer_buffers = strtol(optarg, &endptr, 10);
			if (*endptr != '\0' || handle->n_transfer_buffers <= 0) {
				short_usage(argc,argv,"Invalid transfer buffer count, must be a positive integer: %s", optarg);
				return false;
			}
			break;
		case 'o':
			handle->transfer_timeout = strtol(optarg, &endptr, 10);
			if (*endptr != '\0' || handle->transfer_timeout <= 0) {
				short_usage(argc,argv,"Invalid transfer timeout, must be a positive integer: %s", optarg);
				return false;
			}
			break;
		case 'd':
			current_log_level = strtol(optarg, &endptr, 10);
                        if (*endptr != '\0' || current_log_level < 0 || libusb_debug_level > 5) {
                                short_usage(argc,argv,"Invalid log level, must be a positive integer between "
                                            "0 and 5: %s", optarg);
                                return false;
                        }
			break;
		case 'u':
			libusb_debug_level = strtol(optarg, &endptr, 10);
			if (*endptr != '\0' || libusb_debug_level < 0 || libusb_debug_level > 3) {
				short_usage(argc,argv,"Invalid libusb debug level, must be a positive integer between "
					    "0 and 3: %s", optarg);
				return false;
			}
			libusb_set_debug(handle->usb_context, libusb_debug_level);
			break;
		default:
		case '?':
			short_usage(argc,argv,"Unknown argument: %c. Use %s -h for usage.", optopt, argv[0]);
			return false;
		}
	}

	if (!outputfilename) {
		short_usage(argc,argv,"An output file has to be specified.", optarg);
		return false;
	}

	if (!handle->sample_rate) {
		short_usage(argc,argv,"A sample rate has to be specified.", optarg);
		return false;
	}

	if (!handle->n_samples_requested) {
		handle->n_samples_requested = handle->sample_rate->samples_per_second;
	}

	return true;
}




struct callback_test{
	FILE * file;
	char * filename;
	
};

int data_callback_open(struct slogic_ctx *handle,char * openstring){
struct callback_test *foo;
	
	handle->data_callback_opts = calloc(1,sizeof(struct callback_test));
	foo = handle->data_callback_opts;
	foo->filename = openstring;
	if((foo->file = fopen(foo->filename,"wb")) == 0){
		free(foo);
		return 0;
	}
	return 1;
}

int data_callback_write(struct slogic_ctx *handle, uint8_t * data, size_t size){
struct callback_test *foo = handle->data_callback_opts;

	//fixme, errors have to be handled here
	return fwrite(data,1,size,foo->file);	
}

void data_callback_close(struct slogic_ctx *handle){
struct callback_test *foo = handle->data_callback_opts;
	
	fclose(foo->file);
	free(foo);
	handle->data_callback_opts = 0;
	return ;
}

void ctrl_c_handler(int sig){
//FIXME i dont yet have a program based struct which can handle the ctrlc	
}



int main(int argc, char **argv){
struct slogic_ctx *handle = NULL;
	
	do{
		if(handle){
			slogic_close(handle); //we must be retrying for a reason..
		}
		handle = slogic_init();
		
		//defaults 
		handle->fwfile = "saleae-logic.firmware";
		current_log_level = INFO;
		handle->data_callback_open = data_callback_open;
		handle->data_callback_write = data_callback_write;
		handle->data_callback_close= data_callback_close;
		
		if (!handle) {
			log_printf( INFO, "Failed initialise lib slogic\n");
			exit(42);
		}

		if (!parse_args(argc, argv, handle)) {
			exit(EXIT_FAILURE);
		}

		if (slogic_open(handle,handle->logic_index) != 0) {
			log_printf( INFO, "Failed to open the logic analyzer\n");
			exit(EXIT_FAILURE);
		}
		
		if (!slogic_is_firmware_uploaded(handle)) {
			log_printf( INFO, "Uploading the firmware\n");
			ezusb_upload_firmware(handle,1 ,handle->fwfile);
		}else{
			handle->recording_state = INITALIZED;
		}
	}while(handle->recording_state != INITALIZED);
	
	handle->transfer_buffer_size = libusb_get_max_iso_packet_size (handle->dev, SALEAE_STREAMING_DATA_IN_ENDPOINT) * 4;

	signal(SIGINT,&ctrl_c_handler);
	
	log_printf( DEBUG, "Transfer buffers:     %d\n", handle->n_transfer_buffers);
	log_printf( DEBUG, "Transfer buffer size: %zu\n", handle->transfer_buffer_size);
	log_printf( DEBUG, "Transfer timeout:     %u\n", handle->transfer_timeout);
	log_printf( INFO, "Begin Capture\n");

	if(!handle->data_callback_open(handle,outputfilename)){
		perror("in callback open()");
	}

	slogic_set_capture(handle);
	slogic_execute_recording(handle);
	handle->data_callback_close(handle);

	log_printf( INFO, "Capture finished with exit code %d\n",handle->recording_state);
	log_printf( NOTICE , "Total number of samples requested: %i\n", handle->n_samples_requested);
	log_printf( NOTICE, "Total number of samples read: %i\n", handle->n_samples_fulfilled);
	log_printf( NOTICE, "Total number of transfers: %i\n", handle->transfer_counter);

	slogic_close(handle);

	exit(EXIT_SUCCESS);
}
