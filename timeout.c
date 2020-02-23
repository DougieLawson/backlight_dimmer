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
#include <pcre.h> 

#define fade_amount 1
#define SLEEP_TIMEOUT_DURATION 1000 //in ms
#define XPRINT_IDLE_TIMEOUT 3000 //in ms

static uint16_t actual_brightness;
static uint16_t max_brightness;
static uint16_t current_brightness;
static uint16_t prev_brightness = 0;
static long long int current_time;
static long long int relative_idle_time;
static long long int last_known_idle_time;

static const char actual_file[53] = "/sys/class/backlight/rpi_backlight/actual_brightness";
static const char max_file[50] = "/sys/class/backlight/rpi_backlight/max_brightness";
static const char bright_file[46] = "/sys/class/backlight/rpi_backlight/brightness";

static void sig_handler(int _);
static void set_screen_brightness(FILE* filefd, uint32_t brightness);
static void restart_xprintidle();
static void disable_xenergystar();
static uint32_t fast_atoi( const char * str );
static uint32_t get_idle_time();
static uint32_t get_touch_screen_id();
static void enable_touch_screen(bool enable);
static void increase_brightness(bool increase);

long long int readint(const char * filenm) {
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
		increase_brightness(true);
		exit(0);
}

static void set_screen_brightness(FILE* filefd, uint32_t brightness){
		fprintf(filefd, "%d\n", brightness);
		fflush(filefd);
		fseek(filefd, 0, SEEK_SET);
}

static uint32_t fast_atoi( const char * str )
{
		uint32_t val = 0;
		while( *str ) {
				val = val*10 + (*str++ - '0');
		}
		return val;
}

static void restart_xprintidle() {
		FILE *fp;

		/* Open the command for reading. */
		fp = popen("sudo systemctl restart display-manager", "r");
		if (fp == NULL) {
				printf("Failed to run command\n" );
				exit(1);
		}

		/* close */
		pclose(fp);
}

static void disable_xenergystar(){
		FILE *fp;

		/* Open the command for reading. */
		fp = popen("sudo -u pi env DISPLAY=:0 xset s off -dpms", "r");
		if (fp == NULL) {
				printf("Failed to run command\n" );
				exit(1);
		}

		/* close */
		pclose(fp);
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

static uint32_t get_touch_screen_id(){
		int id;

		pcre *reCompiled;
		pcre_extra *pcreExtra;
		int pcreExecRet;
		int subStrVec[30];
		const char *pcreErrorStr;
		int pcreErrorOffset;
		char aStrRegex[40] = "FT5406 memory based driver\\s*id=(\\d+)";
		const char *psubStrMatchStr;

		reCompiled = pcre_compile(aStrRegex, 0, &pcreErrorStr, &pcreErrorOffset, NULL);
		if(reCompiled == NULL) {
				printf("ERROR: Could not compile '%s': %s\n", aStrRegex, pcreErrorStr);
				exit(1);
		}
		pcreExtra = pcre_study(reCompiled, 0, &pcreErrorStr);

		if(pcreErrorStr != NULL) {
				printf("ERROR: Could not study '%s': %s\n", aStrRegex, pcreErrorStr);
				exit(1);
		}

		FILE *fp;
		char path[1035];

		fp = popen("sudo -u pi env DISPLAY=:0 xinput --list", "r");
		if (fp == NULL) {
				printf("Failed to run command\n" );
				exit(1);
		}

		while (fgets(path, sizeof(path), fp) != NULL) {
				pcreExecRet = pcre_exec(reCompiled,
				                        pcreExtra,
				                        path,
				                        strlen(path),                                 // length of string
				                        0,                                          // Start looking at this point
				                        0,                                          // OPTIONS
				                        subStrVec,
				                        30);                                        // Length of subStrVec
				if(pcreExecRet >= 0) {
						// At this point, rc contains the number of substring matches found...
						if(pcreExecRet == 0) {
								// Set rc to the max number of substring matches possible.
								pcreExecRet = 30 / 3;
						}                                                         /* end if */

						pcre_get_substring(path, subStrVec, pcreExecRet, 1, &(psubStrMatchStr));
						// printf("ID: %s\n", psubStrMatchStr);
						id = fast_atoi(psubStrMatchStr);
						// Free up the substring
						pcre_free_substring(psubStrMatchStr);
				}
		}
		if(pcreExtra != NULL) {
		#ifdef PCRE_CONFIG_JIT
				pcre_free_study(pcreExtra);
		#else
				pcre_free(pcreExtra);
		#endif
		}
		fclose(fp);
		return id;
}

static void enable_touch_screen(bool enable) {
		FILE *fp;
		char final_command[70];
		char set_command[] = "%s %s %d";
		char disable_enable[10];
		strcpy(disable_enable, (enable) ? "--enable" : "--disable");
		char command[] = "sudo -u pi env DISPLAY=:0 xinput";
		sprintf(final_command, set_command, command, disable_enable, get_touch_screen_id());
		// printf("%s\n", final_command);
		/* Open the command for reading. */
		fp = popen((char *) final_command, "r");
		if (fp == NULL) {
				printf("Failed to run command\n" );
				exit(1);
		}
		/* close */
		pclose(fp);
}

static void increase_brightness(bool increase){
		FILE* brightfd;
		brightfd = fopen(bright_file, "w");
		if (brightfd == NULL) {
				int err = errno;
				printf("Error opening %s file: %d", bright_file, err);
				return;
		}
		prev_brightness = current_brightness;
		//printf("Setting Brightness...\n");
		if(increase) {
				for (uint16_t i = 0; i <= max_brightness-fade_amount; i++) {
						current_brightness = (current_brightness+fade_amount>=max_brightness)?max_brightness:current_brightness+fade_amount;
						if (prev_brightness!=current_brightness){ 
							//printf("Brightness: %d\n", current_brightness);
							set_screen_brightness(brightfd, current_brightness);
							prev_brightness = current_brightness;
						}
						sleep_ms(1);
				}
		}
		else{
				for (uint16_t i = max_brightness; i > 0 + fade_amount; i--) {
						current_brightness = (current_brightness-fade_amount<=0)?0:current_brightness-fade_amount;
						if (prev_brightness!=current_brightness){ 
							//printf("Brightness: %d\n", current_brightness);	
							set_screen_brightness(brightfd, current_brightness);
							prev_brightness = current_brightness;
						}
						sleep_ms(3);
				}
		}
		//printf("Done setting Brightness!\n");
		pclose(brightfd);
}

int main(int argc, char * argv[]) {
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
		for (uint16_t i = 0; i < tlen; i++){
				if (!isdigit(argv[1][i])) {
						printf("Entered timeout value is not a number\n");
						exit(1);
				}
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

		
		max_brightness = readint(max_file);
		current_brightness = max_brightness;
		actual_brightness = readint(actual_file);

		increase_brightness(true);
		enable_touch_screen(true);
		restart_xprintidle();
		sleep_ms(XPRINT_IDLE_TIMEOUT);
		disable_xenergystar();
					
		printf("Touchscreen's ID: %d, actual_brightness %d, max_brightness %d\n", get_touch_screen_id(), actual_brightness, max_brightness);

		bool fade_direction = false;
		bool touch_screen_triggered = true;
		bool user_moved = false;
		time_t last_touch_time = time(NULL);
		relative_idle_time = get_idle_time();

		while (1) {
				if(fade_direction && !touch_screen_triggered) {
						for (uint16_t i = 0; i < num_dev; i++) {
								event_size = read(eventfd[i], event, size*64);
								if(event_size != -1 && event_size != 65535 && event[i].time.tv_sec != current_time) {
										printf("Touch Detected!\n");
										printf("Setting Brightness, Time: %ld\n", event[i].time.tv_sec);
										fade_direction = false;
										touch_screen_triggered = true;
										last_touch_time = time(NULL);
										current_time = event[i].time.tv_sec;
										increase_brightness(true);
										enable_touch_screen(true);
								}
						}
				}
				else{
						for (uint16_t i = 0; i < num_dev; i++) {
								event_size = read(eventfd[i], event, size*64);
								if(event_size != -1 && event_size != 65535 && event[i].time.tv_sec != current_time) {
										printf("Time: %lld\n", event[i].time.tv_sec);
										current_time = event[i].time.tv_sec;
								}
						}
				}
				if(touch_screen_triggered && (time(NULL)-last_touch_time >= timeout)) {
						printf("Touch screen check ended\n");
						touch_screen_triggered = false;
				}
				if (!user_moved && fade_direction && ((get_idle_time()-relative_idle_time) < timeout*1E4)) {
						printf("Mouse moved\n");
						relative_idle_time = get_idle_time();
						fade_direction = false;
						user_moved = true;
						increase_brightness(true);
						enable_touch_screen(true);
				}
				if ((!touch_screen_triggered||user_moved) && !fade_direction && ((get_idle_time()-relative_idle_time) >= timeout*1E4)) {
						relative_idle_time = get_idle_time()-timeout*1E4;
						printf("Screen dim: %lld\n", (get_idle_time()-relative_idle_time));
						fade_direction = true;
						user_moved = false;
						if (current_brightness > 0) {
								increase_brightness(false);
								enable_touch_screen(false);
						}
				}
				printf("Idle Time: %lld\n", (get_idle_time()-relative_idle_time));
				sleep_ms(SLEEP_TIMEOUT_DURATION);
		}
}
