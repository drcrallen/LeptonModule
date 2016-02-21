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
#include<signal.h>
#include<unistd.h>
#include<mraa.h>
#include<error.h>
#include<arpa/inet.h>

void sig_handler(int signum);
void save_pgm_file();

static sig_atomic_t volatile isrunning = 1;
static uint16_t image[80*60];

static uint16_t const crc_ccit_table[] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

static inline uint16_t crc_ccitt(uint16_t crc, uint8_t const *buffer, size_t len) {
    for(size_t i = 0; i < len; ++i) {
        crc = (crc >> 8) ^ crc_ccit_table[(crc ^ buffer[i]) & 0xFF];
    }
    return crc;
}

int main(int argc, char **argv) {
    signal(SIGINT, &sig_handler);

    // init
    mraa_init();

    mraa_gpio_context cs = mraa_gpio_init(10);
    mraa_gpio_dir(cs, MRAA_GPIO_OUT);

    mraa_spi_context spi = mraa_spi_init(5);    // bus 5 for edison
    mraa_spi_mode(spi, MRAA_SPI_MODE3);
    mraa_spi_frequency(spi, 6250000);
    mraa_spi_lsbmode(spi, 0);
    mraa_spi_bit_per_word(spi, 8);

    uint8_t payload[164];
    uint8_t recvBuff[164];
    uint16_t *recvBuff16 = (uint16_t *)recvBuff;
    const uint16_t nbMask = htons(0x0FFF);

    printf("\nStarting Lepton Test\n");

    //sync
    mraa_gpio_write(cs, 1);     // high to de-select chip
    // should be at least 5 frames at 27Hz for 185185us
    usleep(185190);
    mraa_gpio_write(cs, 0);     // low to select chip

    // loop while discard packets
    for(int good_packet_count = 0; isrunning && good_packet_count < 60; ++good_packet_count) {
        do {
            const mraa_result_t recvResult =  mraa_spi_transfer_buf(spi, payload, recvBuff, 164);
            if(__builtin_expect(recvResult != MRAA_SUCCESS, 0)) {
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
                        errMsg = "Completely unknown error in MRAA";
                }
                error(recvResult, 0, "Error %d in MRAA: %s", (int)recvResult, errMsg);
            }
        } while((recvBuff[0] & 0x0f) == 0x0f && isrunning);
        const uint16_t packetNb = ntohs(recvBuff16[0]) & 0x0FFF;
        // CRC-16-CCITT
        const uint16_t expected_crc16 = ntohs(recvBuff16[1]);
        // Now we reset the data to prepare for crc
        recvBuff16[0] &= nbMask;
        recvBuff16[1] ^= recvBuff16[1];

        const uint16_t actual_crc16 = crc_ccitt(0, recvBuff, 164);

        if(__builtin_expect(expected_crc16 != actual_crc16, 0)) {
            error(1, 0, "CRC missmatch, expected %d found %d", (int)expected_crc16, (int)actual_crc16);
        }

        const uint16_t imageOffset = packetNb * 80;
        for(int i = 0; i < 80; ++i) {
            // Offset is half the byte offset because we use a uint16_t buffer instead of uint8_t
            // In uint8_t bytes this is (i<<1 + 4)
            const int recvOffset = i + 2;
            image[imageOffset + i] = ntohs(recvBuff16[recvOffset]);
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
