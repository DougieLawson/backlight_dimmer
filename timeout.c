/* timeout.c - a little program to blank the RPi touchscreen and unblank it
   on touch.  Original by https://github.com/timothyhollabaugh

   Improvements listed in Readme.

   Improved and Forked by Nathan Ramanathan 02-08-2020+
   First Written by Dougie Lawson 01-22-2019


   (C) Copyright 2019, Dougie Lawson, all right reserved.
 */
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#define fade_amount 1
#define TOUCH_SCREEN_INPUT_DEVICE 6 //found by typing `xinput --list` in terminal
#define SLEEP_TIMEOUT_DURATION 1000 //in ms

static FILE* brightfd;
static uint16_t actual_brightness;
static uint16_t max_brightness;
static uint16_t current_brightness;

static void sig_handler(int _);
static void set_screen_brightness(uint32_t brightness);
static uint32_t fast_atoi( const char * str );
static uint32_t get_idle_time();
static void enable_touch_screen(bool enable);

long int readint(char * filenm) {
		FILE * filefd;
		filefd = fopen(filenm, "r");
		if (filefd == NULL) {
				int err = errno;
				printf("Error opening %s file: %d", filenm, err);
				exit(1);
		}

		char number[10];
		char * end;
		fscanf(filefd, "%s", (char *) &number);
		printf("File: %s ,The number is: %s\n", filenm, number);

		fclose(filefd);
		return strtol(number, &end, 10);
}
///////////////////////////
void sleep_ms(int milliseconds);
#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif
// void sleep_ms(int milliseconds);
void sleep_ms(int milliseconds) // cross-platform sleep function
{
	#ifdef WIN32
		Sleep(milliseconds);
	#elif _POSIX_C_SOURCE >= 199309L
		struct timespec ts;
		ts.tv_sec = milliseconds / 1000;
		ts.tv_nsec = (milliseconds % 1000) * 1000000;
		nanosleep(&ts, NULL);
	#else
		usleep(milliseconds * 1000);
	#endif
}
////////////////////////////

static void sig_handler(int _)
{
		(void)_;
		set_screen_brightness(max_brightness);
		exit(0);
}

static void set_screen_brightness(uint32_t brightness){
		fprintf(brightfd, "%d\n", brightness);
		fflush(brightfd);
		fseek(brightfd, 0, SEEK_SET);
}

static uint32_t fast_atoi( const char * str )
{
		uint32_t val = 0;
		while( *str ) {
				val = val*10 + (*str++ - '0');
		}
		return val;
}

static uint32_t get_idle_time() {
		uint32_t idle_time;
		FILE *fp;
		char path[1035];

		/* Open the command for reading. */
		fp = popen("sudo -u pi env DISPLAY=:0 xprintidle", "r");
		if (fp == NULL) {
				printf("Failed to run command\n" );
				exit(1);
		}

		/* Read the output a line at a time - output it. */
		while (fgets(path, sizeof(path), fp) != NULL) {
				idle_time = fast_atoi(path);
				if((int)idle_time<0) idle_time=0;
		}
		/* close */
		pclose(fp);
		return idle_time;
}

static void enable_touch_screen(bool enable) {
		FILE *fp;
		char final_command[70];
		char set_command[] = "%s %s %d";
		char disable_enable[10];
		strcpy(disable_enable, (enable) ? "--enable" : "--disable");
		char command[] = "sudo -u pi env DISPLAY=:0 xinput";
		sprintf(final_command, set_command, command, disable_enable, TOUCH_SCREEN_INPUT_DEVICE);
		printf("%s\n", final_command);
		/* Open the command for reading. */
		fp = popen((char *) final_command, "r");
		if (fp == NULL) {
				printf("Failed to run command\n" );
				exit(1);
		}
		/* close */
		pclose(fp);
}

int main(int argc, char * argv[]) {
		// printf("Hello World\n");
		signal(SIGINT, sig_handler);
		if (argc < 3) {
				printf("Usage: timeout <timeout_sec>\n");
				printf("    Use lsinput to see input devices.\n");
				printf("    Device to use is shown as /dev/input/<device>\n");
				exit(1);
		}
		uint16_t tlen;
		uint16_t timeout;
		tlen = strlen(argv[1]);
		for (uint16_t i = 0; i < tlen; i++)
				if (!isdigit(argv[1][i])) {
						printf("Entered timeout value is not a number\n");
						exit(1);
				}
		timeout = atoi(argv[1]);
		uint16_t num_dev = argc - 2;
		uint16_t eventfd[num_dev];
		char device[num_dev][32];
		for (uint16_t i = 0; i < num_dev; i++) {
				device[i][0] = '\0';
				strcat(device[i], "/dev/input/");
				strcat(device[i], argv[i + 2]);

				int event_dev = open(device[i], O_RDONLY | O_NONBLOCK);
				if(event_dev == -1) {
						int err = errno;
						printf("Error opening %s: %d\n", device[i], err);
						exit(1);
				}
				eventfd[i] = event_dev;
		}
		printf("Using input device%s: ", (num_dev > 1) ? "s" : "");
		for (uint16_t i = 0; i < num_dev; i++) {
				printf("%s ", device[i]);
		}
		printf("\n");

		printf("Starting...\n");
		struct input_event event[64];
		uint16_t event_size;
		uint16_t size = sizeof(struct input_event);

		// char actual[53] = "/sys/class/backlight/rpi_backlight/actual_brightness";
		char max[50] = "/sys/class/backlight/rpi_backlight/max_brightness";
		char bright[46] = "/sys/class/backlight/rpi_backlight/brightness";

		brightfd = fopen(bright, "w");
		if (brightfd == NULL) {
				int err = errno;
				printf("Error opening %s file: %d", bright, err);
				exit(1);
		}

		max_brightness = readint(max);
		current_brightness = max_brightness;
		actual_brightness = max_brightness; //readint(actual);

		set_screen_brightness(max_brightness);

		printf("actual_brightness %d, max_brightness %d\n", actual_brightness, max_brightness);

		bool fade_direction = true;
		while (1) {
				for (uint16_t i = 0; i < num_dev; i++) {
						event_size = read(eventfd[i], event, size*64);
						if(event_size != -1) {
								printf("%s Value: %d, Code: %x\n", device[i], event[0].value, event[0].code);
								for (uint16_t i = 0; i <= max_brightness; i++) {
										current_brightness += fade_amount;
										if (current_brightness > max_brightness) current_brightness = max_brightness;
										//printf("Brightness now %d\n", current_brightness);
										set_screen_brightness(current_brightness);
										sleep_ms(1);
								}
								fade_direction = false;
						}
				}
				if (fade_direction && get_idle_time() < timeout*1E4) {
						for (uint16_t i = 0; i <= max_brightness; i++) {
								current_brightness += fade_amount;
								if (current_brightness > max_brightness) current_brightness = max_brightness;
								//printf("Brightness now %d\n", current_brightness);
								set_screen_brightness(current_brightness);
								sleep_ms(1);
						}
						fade_direction = false;
				} else if (!fade_direction && get_idle_time() >= timeout*1E4) {
						if (current_brightness > 0) {
								for (uint16_t i = max_brightness; i > 0; i--) {
										current_brightness -= fade_amount;
										if (current_brightness < 0) current_brightness = 0;
										//printf("Brightness now %d\n", current_brightness);
										set_screen_brightness(current_brightness);
										sleep_ms(2);
								}
								enable_touch_screen(false);
						}
						fade_direction = true;
				}
				//printf("Idle Time: %d\n", get_idle_time());
				sleep_ms(SLEEP_TIMEOUT_DURATION);
		}
}
