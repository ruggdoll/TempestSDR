/*
#-------------------------------------------------------------------------------
# Copyright (c) 2014 Martin Marinov.
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the GNU Public License v3.0
# which accompanies this distribution, and is available at
# http://www.gnu.org/licenses/gpl.html
# 
# Contributors:
#     Martin Marinov - initial API and implementation
#     Added HackRF plugin
#-------------------------------------------------------------------------------
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "TSDRPlugin.h"
#include "TSDRCodes.h"

#include <hackrf.h>

#if defined(_WIN32) || defined(__CYGWIN__)
	#include <windows.h>
	static void thread_sleep(uint32_t milliseconds) { Sleep(milliseconds); }
#else
	#include <unistd.h>
	static void thread_sleep(uint32_t milliseconds) { usleep(1000 * milliseconds); }
#endif

#define DEFAULT_SAMPLE_RATE (20000000)
#define DEFAULT_FREQ (200000000)
#define DEFAULT_GAIN (0.5f)

static volatile int working = 0;

static volatile uint32_t desired_samplerate = DEFAULT_SAMPLE_RATE;
static volatile uint32_t desired_freq = DEFAULT_FREQ;
static volatile float desired_gain = DEFAULT_GAIN;
static volatile uint32_t desired_bandwidth = 0;
static volatile int desired_amp = 0;

static uint32_t applied_samplerate = 0;
static uint32_t applied_freq = 0;
static float applied_gain = -1.0f;
static int applied_amp = -1;

static hackrf_device *device = NULL;
static int hackrf_inited = 0;

static float *float_buffer = NULL;
static size_t float_buffer_size = 0;

static tsdrplugin_readasync_function user_callback = NULL;
static void *user_ctx = NULL;

int errormsg_code;
char * errormsg;
int errormsg_size = 0;
#define RETURN_EXCEPTION(message, status) {announceexception(message, status); return status;}
#define RETURN_OK() {errormsg_code = TSDR_OK; return TSDR_OK;}

static inline void announceexception(const char * message, int status) {
	errormsg_code = status;
	if (status == TSDR_OK) return;

	const int length = strlen(message);
	if (errormsg_size == 0) {
		errormsg = (char *) malloc(length+1);
		if (errormsg == NULL) return;
		errormsg_size = length;
	} else if (length > errormsg_size) {
		char *tmp = (char *) realloc((void*) errormsg, length+1);
		if (tmp == NULL) return;
		errormsg = tmp;
		errormsg_size = length;
	}
	memcpy(errormsg, message, length + 1);
}

static float clampf(float val, float minval, float maxval) {
	if (val < minval) return minval;
	if (val > maxval) return maxval;
	return val;
}

static int gain_to_lna(float gain) {
	int step = (int) (gain * 5.0f + 0.5f);
	if (step < 0) step = 0;
	if (step > 5) step = 5;
	return step * 8;
}

static int gain_to_vga(float gain) {
	int step = (int) (gain * 31.0f + 0.5f);
	if (step < 0) step = 0;
	if (step > 31) step = 31;
	return step * 2;
}

static void parse_params(const char * params) {
	if (params == NULL || params[0] == '\0') return;

	char * copy = strdup(params);
	char * token = strtok(copy, " ");
	while (token != NULL) {
		char * eq = strchr(token, '=');
		if (eq != NULL) {
			*eq = '\0';
			const char * key = token;
			const char * value = eq + 1;

			if (strcmp(key, "samplerate") == 0 || strcmp(key, "rate") == 0 || strcmp(key, "sr") == 0) {
				const unsigned long rate = strtoul(value, NULL, 10);
				if (rate > 0) desired_samplerate = (uint32_t) rate;
			} else if (strcmp(key, "freq") == 0 || strcmp(key, "frequency") == 0) {
				const unsigned long freq = strtoul(value, NULL, 10);
				if (freq > 0) desired_freq = (uint32_t) freq;
			} else if (strcmp(key, "gain") == 0) {
				const float gain = (float) atof(value);
				desired_gain = clampf(gain, 0.0f, 1.0f);
			} else if (strcmp(key, "bandwidth") == 0 || strcmp(key, "bw") == 0) {
				const unsigned long bw = strtoul(value, NULL, 10);
				if (bw > 0) desired_bandwidth = (uint32_t) bw;
			} else if (strcmp(key, "amp") == 0 || strcmp(key, "amp_enable") == 0) {
				desired_amp = (int) strtol(value, NULL, 10) ? 1 : 0;
			}
		}
		token = strtok(NULL, " ");
	}
	free(copy);
}

static void cleanup_device(void) {
	if (device != NULL) {
		hackrf_stop_rx(device);
		hackrf_close(device);
		device = NULL;
	}
}

static int ensure_float_buffer(const size_t items) {
	if (items == 0) return 0;
	if (float_buffer_size < items) {
		float * newbuf = (float *) realloc(float_buffer, sizeof(float) * items);
		if (newbuf == NULL) return -1;
		float_buffer = newbuf;
		float_buffer_size = items;
	}
	return 0;
}

static int apply_samplerate(void) {
	const uint32_t rate = desired_samplerate ? desired_samplerate : DEFAULT_SAMPLE_RATE;
	if (rate == applied_samplerate) return 0;
	if (hackrf_set_sample_rate(device, rate) != HACKRF_SUCCESS) return -1;

	const uint32_t bw = desired_bandwidth ? desired_bandwidth : rate;
	hackrf_set_baseband_filter_bandwidth(device, bw);

	applied_samplerate = rate;
	return 0;
}

static void apply_freq(void) {
	const uint32_t freq = desired_freq;
	if (freq == applied_freq) return;
	if (hackrf_set_freq(device, freq) == HACKRF_SUCCESS)
		applied_freq = freq;
}

static void apply_gain(void) {
	const float gain = desired_gain;
	if (gain == applied_gain) return;
	hackrf_set_lna_gain(device, gain_to_lna(gain));
	hackrf_set_vga_gain(device, gain_to_vga(gain));
	applied_gain = gain;
}

static void apply_amp(void) {
	const int amp = desired_amp ? 1 : 0;
	if (amp == applied_amp) return;
	if (hackrf_set_amp_enable(device, (uint8_t) amp) == HACKRF_SUCCESS)
		applied_amp = amp;
}

static int hackrf_rx_callback(hackrf_transfer *transfer) {
	if (!working) return -1;
	if (transfer == NULL || transfer->buffer == NULL || transfer->valid_length <= 0) return 0;

	const size_t items = (size_t) transfer->valid_length;
	if (ensure_float_buffer(items) != 0) return -1;

	size_t i;
	for (i = 0; i < items; i++)
		float_buffer[i] = ((int8_t) transfer->buffer[i]) / 128.0f;

	if (user_callback != NULL)
		user_callback(float_buffer, items, user_ctx, 0);

	return 0;
}

char TSDRPLUGIN_API __stdcall * tsdrplugin_getlasterrortext(void) {
	if (errormsg_code == TSDR_OK)
		return NULL;
	else
		return errormsg;
}

void TSDRPLUGIN_API __stdcall tsdrplugin_getName(char * name) {
	strncpy(name, "TSDR HackRF Plugin", 199);
	name[199] = '\0';
}

uint32_t TSDRPLUGIN_API __stdcall tsdrplugin_setsamplerate(uint32_t rate) {
	if (rate > 0) desired_samplerate = rate;
	if (device != NULL && !working) apply_samplerate();
	return desired_samplerate;
}

uint32_t TSDRPLUGIN_API __stdcall tsdrplugin_getsamplerate() {
	return desired_samplerate ? desired_samplerate : DEFAULT_SAMPLE_RATE;
}

int TSDRPLUGIN_API __stdcall tsdrplugin_setbasefreq(uint32_t freq) {
	desired_freq = freq;
	if (device != NULL) apply_freq();
	RETURN_OK();
}

int TSDRPLUGIN_API __stdcall tsdrplugin_stop(void) {
	working = 0;
	if (device != NULL) hackrf_stop_rx(device);
	RETURN_OK();
}

int TSDRPLUGIN_API __stdcall tsdrplugin_setgain(float gain) {
	desired_gain = clampf(gain, 0.0f, 1.0f);
	if (device != NULL) apply_gain();
	RETURN_OK();
}

int TSDRPLUGIN_API __stdcall tsdrplugin_init(const char * params) {
	parse_params(params);

	if (!hackrf_inited) {
		if (hackrf_init() != HACKRF_SUCCESS)
			RETURN_EXCEPTION("Cannot initialize HackRF library.", TSDR_ERR_PLUGIN);
		hackrf_inited = 1;
	}

	RETURN_OK();
}

int TSDRPLUGIN_API __stdcall tsdrplugin_readasync(tsdrplugin_readasync_function cb, void *ctx) {
	working = 1;
	user_callback = cb;
	user_ctx = ctx;

	if (hackrf_open(&device) != HACKRF_SUCCESS) {
		working = 0;
		RETURN_EXCEPTION("Cannot open HackRF device.", TSDR_CANNOT_OPEN_DEVICE);
	}

	if (apply_samplerate() != 0) {
		working = 0;
		cleanup_device();
		RETURN_EXCEPTION("Invalid HackRF sample rate.", TSDR_SAMPLE_RATE_WRONG);
	}

	apply_freq();
	apply_gain();
	apply_amp();

	if (hackrf_start_rx(device, hackrf_rx_callback, NULL) != HACKRF_SUCCESS) {
		working = 0;
		cleanup_device();
		RETURN_EXCEPTION("Cannot start HackRF RX stream.", TSDR_ERR_PLUGIN);
	}

	while (working) {
		apply_freq();
		apply_gain();
		apply_amp();
		thread_sleep(50);
	}

	cleanup_device();

	RETURN_OK();
}

void TSDRPLUGIN_API __stdcall tsdrplugin_cleanup(void) {
	working = 0;
	cleanup_device();
	if (float_buffer != NULL) {
		free(float_buffer);
		float_buffer = NULL;
		float_buffer_size = 0;
	}
	if (hackrf_inited) {
		hackrf_exit();
		hackrf_inited = 0;
	}
}
