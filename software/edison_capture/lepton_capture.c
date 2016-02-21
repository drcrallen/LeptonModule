/*
Copyright (c) 2014, C. Gyger, S. Raible
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include<stdio.h>
#include<endian.h>
#include<signal.h>
#include<unistd.h>
#include<mraa.h>
#include<error.h>
#include<sys/time.h>

void sig_handler(int signum);
void save_pgm_file();

static sig_atomic_t volatile isrunning = 1;
static unsigned int image[80*60];

main(int argc, char **argv)
{
	signal(SIGINT, &sig_handler);
	
	// init
	mraa_init();
	
	mraa_gpio_context cs = mraa_gpio_init(10);	
	mraa_gpio_dir(cs, MRAA_GPIO_OUT);

	mraa_spi_context spi = mraa_spi_init(5);	// bus 5 for edison
	mraa_spi_mode(spi, MRAA_SPI_MODE3);	
	mraa_spi_frequency(spi, 6250000);
	mraa_spi_lsbmode(spi, 0);
	mraa_spi_bit_per_word(spi, 8);
		
	uint8_t payload[164];
	uint8_t recvBuff[164];
	uint16_t recvBuff16 = (uint16_t *)recvBuff;
	
	printf("\nStarting Lepton Test\n");
	
	//sync	
	mraa_gpio_write(cs, 1);	// high to de-select chip
	// should be at least 5 frames at 27Hz for 185185us
	usleep(185190);
	mraa_gpio_write(cs, 0);	// low to select chip
	
	// loop while discard packets
	for(int good_packet_count = 0; isrunning && good_packet_count < 60; ++good_packet_count) {
		do {
			const mraa_result_t recvResult =  mraa_spi_transfer_buf(spi, payload, recvBuff, 164);
			if(unlikely(recvResult != MRAA_SUCCESS)) {
				char *errMsg = NULL;
				switch(recvResult) {
					case MRAA_ERROR_FEATURE_NOT_IMPLEMENTED:
						errMsg = "MRAA_ERROR_FEATURE_NOT_IMPLEMENTED";
					case MRAA_ERROR_FEATURE_NOT_SUPPORTED:
						errMsg = "MRAA_ERROR_FEATURE_NOT_SUPPORTED";
					case MRAA_ERROR_INVALID_VERBOSITY_LEVEL:
						errMsg = "MRAA_ERROR_INVALID_VERBOSITY_LEVEL";
					case MRAA_ERROR_INVALID_PARAMETER:
						errMsg = "MRAA_ERROR_INVALID_PARAMETER";
					case MRAA_ERROR_INVALID_HANDLE:
						errMsg = "MRAA_ERROR_INVALID_HANDLE";
					case MRAA_ERROR_NO_RESOURCES:
						errMsg = "MRAA_ERROR_NO_RESOURCES";
					case MRAA_ERROR_INVALID_RESOURCE:
						errMsg = "MRAA_ERROR_INVALID_RESOURCE";
					case MRAA_ERROR_INVALID_QUEUE_TYPE:
						errMsg = "MRAA_ERROR_INVALID_QUEUE_TYPE";
					case MRAA_ERROR_NO_DATA_AVAILABLE:
						errMsg = "MRAA_ERROR_NO_DATA_AVAILABLE";
					case MRAA_ERROR_INVALID_PLATFORM:
						errMsg = "MRAA_ERROR_INVALID_PLATFORM";
					case MRAA_ERROR_PLATFORM_NOT_INITIALISED:
						errMsg = "MRAA_ERROR_PLATFORM_NOT_INITIALISED";
					case MRAA_ERROR_PLATFORM_ALREADY_INITIALISED:
						errMsg = "MRAA_ERROR_PLATFORM_ALREADY_INITIALISED";
					case MRAA_ERROR_UNSPECIFIED:
						errMsg = "MRAA_ERROR_UNSPECIFIED";
					default:
						errMsg = "Completely unknown error in MRAA"
				}
				error(recvResult, 0, "Error %d in MRAA: %s", (int)recvResult, errMsg);
			}
		} while(unlikely((recvBuff[0] & 0x0f) == 0x0f && isrunning));
		const uint16_t packetNb = be16toh(recvBuff16[0]& 0x0FFF);
		// CRC-16-CCITT
		const uint16_t crc16 = be16toh(recvBuff16[1]);
		const uint16_t imageOffset = packetNb * 80;
		for(int i = 0; i < 80; ++i) {
			// Offset is half the byte offset because we use a uint16_t buffer instead of uint8_t
			// In uint8_t bytes this is (i<<1 + 4)
			const recvOffset = i + 2;
			image[imageOffset + i] = be16toh(recvBuff16[recvOffset]);
		}
	}
	
	// de-init	
	fprintf(stdout, "\nEnding, set CS = 0\n");
	mraa_gpio_write(cs, 0);	
	mraa_gpio_close(cs);
	
	printf("\nFrame received, storing PGM file\n");
	save_pgm_file();
	
	fprintf(stdout, "\nDone\n");
	
	return MRAA_SUCCESS;	
}

void save_pgm_file()
{
	int i;
	int j;
	unsigned int maxval = 0;
	unsigned int minval = 100000;
	FILE *f = fopen("image.pgm", "w");
	if (f == NULL)
	{
		printf("Error opening file!\n");
		exit(1);
	}
	printf("\nCalculating min/max values for proper scaling...\n");
	for(i=0;i<60;i++)
	{
		for(j=0;j<80;j++)
		{
			if (image[i * 80 + j] > maxval) {
				maxval = image[i * 80 + j];
			}
			if (image[i * 80 + j] < minval) {
				minval = image[i * 80 + j];
			}
		}
	}
	printf("maxval = %u\n",maxval);
	printf("minval = %u\n",minval);
	fprintf(f,"P2\n80 60\n%u\n",maxval-minval);
	for(i=0;i<60;i++)
	{
		for(j=0;j<80;j++)
		{
			fprintf(f,"%d ", image[i * 80 + j] - minval);
		}
		fprintf(f,"\n");
	}
	fprintf(f,"\n\n");
	fclose(f);
}

void sig_handler(int signum)
{
	if(signum == SIGINT)
		isrunning = 0;
}
