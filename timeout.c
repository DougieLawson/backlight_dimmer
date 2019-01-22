/* timeout.c - a little program to blank the RPi touchscreen and unblank it
   on touch.  Original by https://github.com/timothyhollabaugh

   Loosely based on the original. The main difference is that the 
   brightness is progressively reduced to zero.

   On a touch event we reset the brightness to the original value (read when
   the program started.

   Unless you change the permissions in udev this program needs to run as
   root or setuid as root.

   2019-01-22 Dougie Lawson
   (C) Copyright 2019, Dougie Lawson, all right reserved.
*/

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>

long int readint(char* filenm) {
	FILE* filefd;
	filefd = fopen(filenm, "r");
        if(filefd == NULL){
                int err = errno;
                printf("Error opening %s file: %d", filenm, err);
                exit(1);
        }

	char number[10];
	char* end;
	fscanf(filefd, "%s", &number);
	printf("File: %s ,The number is: %s\n",filenm, number);

	fclose(filefd);
	return strtol(number,&end,10);
}

int main(int argc, char* argv[]){
        if (argc < 3) {
                printf("Usage: timeout <timeout_sec> <device> [<device>...]\n");
                printf("    Use lsinput to see input devices.\n");
                printf("    Device to use is shown as /dev/input/<device>\n");
                exit(1);
        }
        int i;
        int tlen;
        int timeout;
        tlen = strlen(argv[1]);
        for (i=0;i<tlen; i++)
                if (!isdigit(argv[1][i])) {
                        printf ("Entered timeout value is not a number\n");
                        exit(1);
                }
        timeout = atoi(argv[1]);

        int num_dev = argc - 2;
        int eventfd[num_dev];
        char device[num_dev][32];
        for (i = 0; i < num_dev; i++) {
                device[i][0] = '\0';
                strcat(device[i], "/dev/input/");
                strcat(device[i], argv[i + 2]);

                int event_dev = open(device[i], O_RDONLY | O_NONBLOCK);
                if(event_dev == -1){
                        int err = errno;
                        printf("Error opening %s: %d\n", device[i], err);
                        exit(1);
                }
                eventfd[i] = event_dev;
        }
        printf("Using input device%s: ", (num_dev > 1) ? "s" : "");
        for (i = 0; i < num_dev; i++) {
                printf("%s ", device[i]);
        }
        printf("\n");

        printf("Starting...\n");
        struct input_event event[64];
	FILE* brightfd;
        int event_size;
        int light_size;
        int size = sizeof(struct input_event);

	long int actual_brightness;
	long int max_brightness;
	long int current_brightness;

        /* new sleep code to bring CPU usage down from 100% on a core */
        struct timespec sleepTime;
        sleepTime.tv_sec = 0;
        sleepTime.tv_nsec = 100000000L;  /* 1 seconds - larger values may reduce load even more */

	char actual[53] = "/sys/class/backlight/rpi_backlight/actual_brightness";
	char max[50] = "/sys/class/backlight/rpi_backlight/max_brightness";
  	char bright[46] = "/sys/class/backlight/rpi_backlight/brightness";

	brightfd = fopen(bright, "w");
        if(brightfd == NULL){
                int err = errno;
                printf("Error opening %s file: %d", bright, err);
                exit(1);
        }

        actual_brightness = readint(actual);
        max_brightness = readint(max);
	current_brightness = actual_brightness;

	printf("actual_brightness %d, max_brightness %d\n", actual_brightness, max_brightness);

        time_t now = time(NULL);
        time_t touch = now;

        while(1) {
                now = time(NULL);
                
                for (i = 0; i < num_dev; i++) {               
                        event_size = read(eventfd[i], event, size*64);
                        if(event_size != -1) {
                                printf("%s Value: %d, Code: %x\n", device[i], event[0].value, event[0].code);
                                touch = now;
				current_brightness = actual_brightness;
				printf("Brightness now %d\n", current_brightness);
				fprintf(brightfd, "%d\n", current_brightness);
				fflush(brightfd);
				fseek(brightfd, 0, SEEK_SET);

                        }
                }

                if(difftime(now, touch) > timeout) {
			if (current_brightness > 0) {
				current_brightness -= 15;
				if (current_brightness < 0) current_brightness = 0;
				printf("Brightness now %d\n", current_brightness);
				fprintf(brightfd, "%d\n", current_brightness);
				fflush(brightfd);
				fseek(brightfd, 0, SEEK_SET);
			}
                }

                nanosleep(&sleepTime, NULL);
        }
}
