// v90s — Powkiddy V90S (Allwinner A133P, Batocera-splice stock OS)
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#include <dirent.h>
#include <stdint.h>

static int wake_fd = -1;

#define V90S_INPUT_COUNT 8
#define EV_KEY 0x01
#define EV_ABS 0x03

// The built-in "adc_gamepad" (rocknix-singleadc-joypad driver, event4)
// reports standard BTN_* codes over evdev with the dpad on hat0
// (ABS_HAT0X/Y) — same shape as the h700 port this file is derived from.
// Raw evdev is used for the built-in pad; SDL joysticks are for external
// USB pads only.
#define RAW_HATY 17
#define RAW_HATX 16

struct input_event {
	struct timeval time;
	uint16_t type;
	uint16_t code;
	int32_t value;
};

static int input_fds[V90S_INPUT_COUNT];
static uint32_t last_input_scan = 0;

static void close_evdev_input(int i) {
	if (i < 0 || i >= V90S_INPUT_COUNT || input_fds[i] < 0)
		return;
	close(input_fds[i]);
	input_fds[i] = -1;
}

static void open_evdev_input(int i) {
	char path[64];
	snprintf(path, sizeof(path), "/dev/input/event%i", i);

	input_fds[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (input_fds[i] < 0)
		return;

	char name_path[128];
	char name[256] = {0};
	snprintf(name_path, sizeof(name_path), "/sys/class/input/event%i/device/name", i);
	getFile(name_path, name, sizeof(name));
	if (name[0])
		LOG_info("Opening input event%i: %s\n", i, name);
	else
		LOG_info("Opening input event%i\n", i);
}

static void scan_evdev_inputs(void) {
	uint32_t now = SDL_GetTicks();
	if (last_input_scan && now - last_input_scan < 2000)
		return;
	last_input_scan = now;

	for (int i = 0; i < V90S_INPUT_COUNT; i++) {
		char path[64];
		snprintf(path, sizeof(path), "/dev/input/event%i", i);
		int connected = exists(path);
		if (input_fds[i] < 0 && connected)
			open_evdev_input(i);
		else if (input_fds[i] >= 0 && !connected)
			close_evdev_input(i);
	}
}

static void apply_button_state(int btn, int id, int pressed, uint32_t tick) {
	if (btn == BTN_NONE || id < 0)
		return;

	if (!pressed) {
		if (pad.is_pressed & btn) {
			pad.is_pressed &= ~btn;
			pad.just_repeated &= ~btn;
			pad.just_released |= btn;
		}
	}
	else if ((pad.is_pressed & btn) == BTN_NONE) {
		pad.just_pressed |= btn;
		pad.just_repeated |= btn;
		pad.is_pressed |= btn;
		pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
	}
}

static int button_from_code(int code, int *id) {
	     if (code == CODE_UP)       { *id = BTN_ID_DPAD_UP;    return BTN_DPAD_UP; }
	else if (code == CODE_DOWN)     { *id = BTN_ID_DPAD_DOWN;  return BTN_DPAD_DOWN; }
	else if (code == CODE_LEFT)     { *id = BTN_ID_DPAD_LEFT;  return BTN_DPAD_LEFT; }
	else if (code == CODE_RIGHT)    { *id = BTN_ID_DPAD_RIGHT; return BTN_DPAD_RIGHT; }
	else if (code == CODE_A)        { *id = BTN_ID_A;          return BTN_A; }
	else if (code == CODE_B)        { *id = BTN_ID_B;          return BTN_B; }
	else if (code == CODE_X)        { *id = BTN_ID_X;          return BTN_X; }
	else if (code == CODE_Y)        { *id = BTN_ID_Y;          return BTN_Y; }
	else if (code == CODE_START)    { *id = BTN_ID_START;      return BTN_START; }
	else if (code == CODE_SELECT)   { *id = BTN_ID_SELECT;     return BTN_SELECT; }
	else if (code == CODE_MENU)     { *id = BTN_ID_MENU;       return BTN_MENU; }
	else if (code == CODE_L1)       { *id = BTN_ID_L1;         return BTN_L1; }
	else if (code == CODE_L2)       { *id = BTN_ID_L2;         return BTN_L2; }
	else if (code == CODE_R1)       { *id = BTN_ID_R1;         return BTN_R1; }
	else if (code == CODE_R2)       { *id = BTN_ID_R2;         return BTN_R2; }
	else if (code == CODE_PLUS)     { *id = BTN_ID_PLUS;       return BTN_PLUS; }
	else if (code == CODE_MINUS)    { *id = BTN_ID_MINUS;      return BTN_MINUS; }
	else if (code == CODE_POWER)    { *id = BTN_ID_POWER;      return BTN_POWER; }
	return BTN_NONE;
}

static int button_from_joy(int joy, int *id) {
	     if (joy == JOY_UP)       { *id = BTN_ID_DPAD_UP;    return BTN_DPAD_UP; }
	else if (joy == JOY_DOWN)     { *id = BTN_ID_DPAD_DOWN;  return BTN_DPAD_DOWN; }
	else if (joy == JOY_LEFT)     { *id = BTN_ID_DPAD_LEFT;  return BTN_DPAD_LEFT; }
	else if (joy == JOY_RIGHT)    { *id = BTN_ID_DPAD_RIGHT; return BTN_DPAD_RIGHT; }
	else if (joy == JOY_A)        { *id = BTN_ID_A;          return BTN_A; }
	else if (joy == JOY_B)        { *id = BTN_ID_B;          return BTN_B; }
	else if (joy == JOY_X)        { *id = BTN_ID_X;          return BTN_X; }
	else if (joy == JOY_Y)        { *id = BTN_ID_Y;          return BTN_Y; }
	else if (joy == JOY_START)    { *id = BTN_ID_START;      return BTN_START; }
	else if (joy == JOY_SELECT)   { *id = BTN_ID_SELECT;     return BTN_SELECT; }
	else if (joy == JOY_MENU)     { *id = BTN_ID_MENU;       return BTN_MENU; }
	else if (joy == JOY_MENU_ALT) { *id = BTN_ID_MENU;       return BTN_MENU; }
	else if (joy == JOY_MENU_ALT2){ *id = BTN_ID_MENU;       return BTN_MENU; }
	else if (joy == JOY_L1)       { *id = BTN_ID_L1;         return BTN_L1; }
	else if (joy == JOY_L2)       { *id = BTN_ID_L2;         return BTN_L2; }
	else if (joy == JOY_R1)       { *id = BTN_ID_R1;         return BTN_R1; }
	else if (joy == JOY_R2)       { *id = BTN_ID_R2;         return BTN_R2; }
	else if (joy == JOY_PLUS)     { *id = BTN_ID_PLUS;       return BTN_PLUS; }
	else if (joy == JOY_MINUS)    { *id = BTN_ID_MINUS;      return BTN_MINUS; }
	else if (joy == JOY_POWER)    { *id = BTN_ID_POWER;      return BTN_POWER; }
	return BTN_NONE;
}

static void apply_hat_axis(int neg_id, int pos_id, int value, uint32_t tick) {
	apply_button_state(1 << neg_id, neg_id, value < 0, tick);
	apply_button_state(1 << pos_id, pos_id, value > 0, tick);
}

void PLAT_initPlatform(void) {
	// single device, no HDMI, no rotation — nothing to detect
}

static SDL_Joystick **joysticks = NULL;
static int num_joysticks = 0;

// The built-in pad is handled via raw evdev (above); opening it as an SDL
// joystick too would feed poll_sdl_input() duplicate events interpreted with
// the external-pad JOY_* layout. SDL joysticks are for external pads only.
static int is_builtin_pad(const char *name) {
	return name && strcmp(name, "adc_gamepad") == 0;
}

void PLAT_initInput(void) {
	for (int i = 0; i < V90S_INPUT_COUNT; i++)
		input_fds[i] = -1;
	last_input_scan = 0;

	if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
		LOG_error("Failed initializing joysticks: %s\n", SDL_GetError());
	SDL_JoystickEventState(SDL_ENABLE);
	int total = SDL_NumJoysticks();
    if (total > 0) {
        joysticks = (SDL_Joystick **)malloc(sizeof(SDL_Joystick *) * total);
        for (int i = 0; i < total; i++) {
			const char *name = SDL_JoystickNameForIndex(i);
			if (is_builtin_pad(name)) {
				LOG_info("Skipping built-in joystick %d: %s (handled via evdev)\n", i, name);
				continue;
			}
			SDL_Joystick *joy = SDL_JoystickOpen(i);
			if (!joy) {
				LOG_error("Failed to open joystick %d: %s\n", i, SDL_GetError());
				continue;
			}
			joysticks[num_joysticks++] = joy;
			LOG_info("Opening joystick %d: %s\n", i, SDL_JoystickName(joy));
        }
    }
	scan_evdev_inputs();
}

void PLAT_quitInput(void) {
	for (int i = 0; i < V90S_INPUT_COUNT; i++)
		close_evdev_input(i);

	if (joysticks) {
        for (int i = 0; i < num_joysticks; i++) {
            if (SDL_JoystickGetAttached(joysticks[i])) {
				LOG_info("Closing joystick %d: %s\n", i, SDL_JoystickName(joysticks[i]));
				SDL_JoystickClose(joysticks[i]);
			}
        }
        free(joysticks);
        joysticks = NULL;
        num_joysticks = 0;
	}
	if (wake_fd >= 0) {
		close(wake_fd);
		wake_fd = -1;
	}
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

static void poll_sdl_input(uint32_t tick) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		int btn = BTN_NONE;
		int pressed = 0;
		int id = -1;

		if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP) {
			pressed = event.type == SDL_JOYBUTTONDOWN;
			btn = button_from_joy(event.jbutton.button, &id);
		}
		else if (event.type == SDL_JOYHATMOTION) {
			int hat = event.jhat.value;
			apply_button_state(BTN_DPAD_UP, BTN_ID_DPAD_UP, hat & SDL_HAT_UP, tick);
			apply_button_state(BTN_DPAD_DOWN, BTN_ID_DPAD_DOWN, hat & SDL_HAT_DOWN, tick);
			apply_button_state(BTN_DPAD_LEFT, BTN_ID_DPAD_LEFT, hat & SDL_HAT_LEFT, tick);
			apply_button_state(BTN_DPAD_RIGHT, BTN_ID_DPAD_RIGHT, hat & SDL_HAT_RIGHT, tick);
			continue;
		}
		else if (event.type == SDL_JOYAXISMOTION) {
			// external pads only; the built-in pad has no analog sticks
			int axis = event.jaxis.axis;
			int val = event.jaxis.value;

			if (axis == 0) {
				pad.laxis.x = val;
				PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, val, tick + PAD_REPEAT_DELAY);
				continue;
			}
			else if (axis == 1) {
				pad.laxis.y = val;
				PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, val, tick + PAD_REPEAT_DELAY);
				continue;
			}
			else if (axis == 2) {
				pad.raxis.x = val;
				continue;
			}
			else if (axis == 3) {
				pad.raxis.y = val;
				continue;
			}
		}
		else if (event.type == SDL_QUIT) {
			PWR_powerOff(0);
			continue;
		}
		else if (event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED) {
			PAD_update(&event);
			continue;
		}

		apply_button_state(btn, id, pressed, tick);
	}
}

static void poll_evdev_input(uint32_t tick) {
	struct input_event event;

	for (int i = 0; i < V90S_INPUT_COUNT; i++) {
		int input = input_fds[i];
		if (input < 0)
			continue;

		errno = 0;
		while (read(input, &event, sizeof(event)) == sizeof(event)) {
			if (event.type != EV_KEY && event.type != EV_ABS)
				continue;

			int btn = BTN_NONE;
			int pressed = 0;
			int id = -1;
			int code = event.code;
			int value = event.value;

			if (event.type == EV_KEY) {
				if (value > 1)
					continue;
				pressed = value;
				btn = button_from_code(code, &id);
			}
			else if (event.type == EV_ABS) {
				if (code == RAW_HATY) {
					apply_hat_axis(BTN_ID_DPAD_UP, BTN_ID_DPAD_DOWN, value, tick);
					continue;
				}
				else if (code == RAW_HATX) {
					apply_hat_axis(BTN_ID_DPAD_LEFT, BTN_ID_DPAD_RIGHT, value, tick);
					continue;
				}
			}

			apply_button_state(btn, id, pressed, tick);
		}

		if (errno && errno != EAGAIN && errno != EWOULDBLOCK)
			close_evdev_input(i);
	}
}

// wake_fd is a second, independent open of event0 used by PLAT_shouldWake.
// It must be kept drained while awake, otherwise the power-key release that
// triggers the next sleep is still buffered when PWR_waitForWake starts
// polling and the device wakes back up instantly.
static void drain_wake_fd(void) {
	if (wake_fd < 0)
		wake_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (wake_fd < 0) return;

	struct input_event event;
	errno = 0;
	while (read(wake_fd, &event, sizeof(event)) == sizeof(event));
	if (errno && errno != EAGAIN && errno != EWOULDBLOCK) {
		close(wake_fd);
		wake_fd = -1;
	}
}

void PLAT_pollInput(void) {
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i = 0; i < BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick >= pad.repeat_at[i])) {
			pad.just_repeated |= btn;
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}

	scan_evdev_inputs();
	poll_sdl_input(tick);
	poll_evdev_input(tick);

	drain_wake_fd();
}

void PLAT_updateInput(const SDL_Event *event) {
	switch (event->type) {
    case SDL_JOYDEVICEADDED: {
        int device_index = event->jdevice.which;
        const char *name = SDL_JoystickNameForIndex(device_index);
        if (is_builtin_pad(name)) {
            LOG_info("Skipping built-in joystick %d: %s (handled via evdev)\n", device_index, name);
            break;
        }
        SDL_Joystick *new_joy = SDL_JoystickOpen(device_index);
        if (new_joy) {
            joysticks = realloc(joysticks, sizeof(SDL_Joystick *) * (num_joysticks + 1));
            joysticks[num_joysticks++] = new_joy;
            LOG_info("Joystick added at index %d: %s\n", device_index, SDL_JoystickName(new_joy));
        } else {
            LOG_error("Failed to open added joystick at index %d: %s\n", device_index, SDL_GetError());
        }
        break;
    }

    case SDL_JOYDEVICEREMOVED: {
        SDL_JoystickID removed_id = event->jdevice.which;
        for (int i = 0; i < num_joysticks; ++i) {
            if (SDL_JoystickInstanceID(joysticks[i]) == removed_id) {
                LOG_info("Joystick removed: %s\n", SDL_JoystickName(joysticks[i]));
                SDL_JoystickClose(joysticks[i]);

                // Shift down the remaining entries
                for (int j = i; j < num_joysticks - 1; ++j)
                    joysticks[j] = joysticks[j + 1];
                num_joysticks--;

                if (num_joysticks == 0) {
                    free(joysticks);
                    joysticks = NULL;
                } else {
                    joysticks = realloc(joysticks, sizeof(SDL_Joystick *) * num_joysticks);
                }
                break;
            }
        }
        break;
    }

    default:
        break;
    }
}

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	PLAT_getBatteryStatusFine(is_charging, charge);

	// worry less about battery and more about the game you're playing
	     if (*charge>80) *charge = 100;
	else if (*charge>60) *charge =  80;
	else if (*charge>40) *charge =  60;
	else if (*charge>20) *charge =  40;
	else if (*charge>10) *charge =  20;
	else           		 *charge =  10;
}

void PLAT_getCPUTemp() {
	perf.cpu_temp = getInt("/sys/devices/virtual/thermal/thermal_zone0/temp")/1000;
}

void PLAT_getCPUSpeed()
{
	perf.cpu_speed = getInt("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq")/1000;
}

void PLAT_getGPUTemp() {
	perf.gpu_temp = getInt("/sys/devices/virtual/thermal/thermal_zone1/temp")/1000;
}

void PLAT_getGPUSpeed() {
	// TODO(recon): PowerVR GE8300 devfreq path on the A133 BSP kernel
	int speed = getInt("/sys/kernel/debug/clk/gpu/clk_rate");
	if (speed <= 0)
		speed = getInt("/sys/kernel/debug/clk/pll_gpu/clk_rate");
	perf.gpu_speed = speed > 0 ? speed / 1000000 : 660; // MHz
}

void PLAT_getNetworkStatus(int* is_online)
{
	// no wifi hardware
	if(is_online)
		*is_online = 0;
}

// axp2202 PMIC confirmed by recon (same as the h700 devices)
void PLAT_getBatteryStatusFine(int *is_charging, int *charge)
{
	if(is_charging) {
		int charger_present = getInt("/sys/class/power_supply/axp2202-usb/online");
		*is_charging = (charger_present == 1);
	}
	if(charge) {
		*charge = getInt("/sys/class/power_supply/axp2202-battery/capacity");
	}
}

// sunxi disp2: fbdev blank powers the LCD (and its backlight) down through
// the panel driver, and DISP_LCD_SET_BRIGHTNESS alone only programs the PWM
// level — it does not re-enable a disabled backlight. Without the explicit
// enable ioctl the panel keeps rendering into darkness after wake.
#define DISP_LCD_BACKLIGHT_ENABLE  0x104
#define DISP_LCD_BACKLIGHT_DISABLE 0x105
static void disp_setBacklightPower(int enable) {
	int fd = open("/dev/disp", O_RDWR);
	if (fd >= 0) {
		unsigned long param[4] = {0, 0, 0, 0};
		ioctl(fd, enable ? DISP_LCD_BACKLIGHT_ENABLE : DISP_LCD_BACKLIGHT_DISABLE, &param);
		close(fd);
	}
	// belt and suspenders: the standard backlight class power knob
	putInt("/sys/class/backlight/sunxi_backlight/bl_power", enable ? 0 : 4);
}

void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt("/sys/class/graphics/fb0/blank", 0);
		disp_setBacklightPower(1);
		SetBrightness(GetBrightness());
	}
	else {
		SetRawBrightness(0);
		putInt("/sys/class/graphics/fb0/blank", 1);
	}
}

void PLAT_powerOff(int reboot) {
	system("rm -f /tmp/nextui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	system("cat /dev/zero > /dev/fb0 2>/dev/null");
	if(reboot > 0)
		touch("/tmp/reboot");
	else
		touch("/tmp/poweroff");
	sync();
	exit(0);
}

// Deep sleep (`echo mem`) looked broken on first hardware test (resume with
// dead backlight + dead input), but that run was confounded: stock's
// battery-saver daemon was still alive and re-suspending the device on input
// inactivity right after our wake. With the stock idle daemons neutered in
// postshare.sh, deep sleep is back on trial. If the corpse state returns
// with the daemons confirmed dead, flip this to 0 (soft sleep + the shared
// PWR code's 2-minute poweroff escalation, covered by quicksave/auto-resume).
int PLAT_supportsDeepSleep(void) { return 1; }

// The V90S clamshell has no lid-close sensor (no SW_LID device, no PMIC
// hallkey) — sleep/wake is power-button only.
void PLAT_initLid(void) {
	lid.has_lid = 0;
}

int PLAT_lidChanged(int* state) {
	return 0;
}

int PLAT_shouldWake(void) {
	if (wake_fd < 0)
		wake_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (wake_fd < 0) return 0;

	struct input_event event;
	int should_wake = 0;
	errno = 0;
	while (read(wake_fd, &event, sizeof(event)) == sizeof(event)) {
		if (event.type == EV_KEY && event.code == CODE_POWER && event.value == 0) {
			LOG_debug("PLAT_shouldWake: power key release, waking\n");
			should_wake = 1;
		}
	}
	if (errno && errno != EAGAIN && errno != EWOULDBLOCK) {
		close(wake_fd);
		wake_fd = -1;
	}
	return should_wake;
}

///////////////////////////////

double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9; // Convert to seconds
}
double get_process_cpu_time_sec() {
	// this gives cpu time in nanoseconds needed to accurately calculate cpu usage in very short time frames.
	// unfortunately about 20ms between meassures seems the lowest i can go to get accurate results
	// maybe in the future i will find and even more granual way to get cpu time, but might just be a limit of C or Linux alltogether
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9; // Convert to seconds
}

static pthread_mutex_t currentcpuinfo;
// a roling average for the display values of about 2 frames, otherwise they are unreadable jumping too fast up and down and stuff to read
#define ROLLING_WINDOW 120

void *PLAT_cpu_monitor(void *arg) {
    if (!Perf_tryBeginCPUMonitor()) return NULL;

    double prev_real_time = get_time_sec();
    double prev_cpu_time = get_process_cpu_time_sec();

    double cpu_usage_history[ROLLING_WINDOW] = {0};
    int history_index = 0;
    int history_count = 0;

    while (Perf_isCPUMonitorEnabled()) {
        double curr_real_time = get_time_sec();
        double curr_cpu_time = get_process_cpu_time_sec();

        double elapsed_real_time = curr_real_time - prev_real_time;
        double elapsed_cpu_time = curr_cpu_time - prev_cpu_time;

        if (elapsed_real_time > 0) {
            double cpu_usage = (elapsed_cpu_time / elapsed_real_time) * 100.0;

            pthread_mutex_lock(&currentcpuinfo);

            cpu_usage_history[history_index] = cpu_usage;
            history_index = (history_index + 1) % ROLLING_WINDOW;
            if (history_count < ROLLING_WINDOW) history_count++;

            double sum_cpu_usage = 0;
            for (int i = 0; i < history_count; i++) sum_cpu_usage += cpu_usage_history[i];
            perf.cpu_usage = sum_cpu_usage / history_count;

            pthread_mutex_unlock(&currentcpuinfo);
        }

        prev_real_time = curr_real_time;
        prev_cpu_time = curr_cpu_time;
        usleep(100000);
    }

    Perf_endCPUMonitor();
    return NULL;
}

void PLAT_setCPUSpeed(int speed) {
	const char* mode;
	switch (speed) {
		case CPU_SPEED_AUTO: mode = "auto"; break;
		case CPU_SPEED_PERFORMANCE: mode = "performance"; break;
		case CPU_SPEED_POWERSAVE: mode = "powersave"; break;
		default: return;
	}

	const char* system_path = getenv("SYSTEM_PATH");
	if (!system_path) {
		LOG_info("WARNING: SYSTEM_PATH not set, cannot run governor script\n");
		return;
	}
	char cmd[512];
	int n = snprintf(cmd, sizeof(cmd), "sh \"%s/bin/governor.sh\" \"%s\"", system_path, mode);
	if (n < 0 || n >= (int)sizeof(cmd)) {
		LOG_info("WARNING: SYSTEM_PATH too long for governor script path\n");
		return;
	}
	int ret = system(cmd);
	if (ret != 0) LOG_info("WARNING: governor script exited with status %d for mode '%s'\n", ret, mode);
}

void PLAT_setRumble(int strength) {
	// no rumble motor
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

void PLAT_overrideMute(int mute) {
	// TODO(recon): confirm control name; 'HpSpeaker Switch' per the Knulli
	// powkiddy-v90s asound.state
	system(mute ? "amixer -q sset 'HpSpeaker Switch' off" : "amixer -q sset 'HpSpeaker Switch' on");
}

char* PLAT_getModel(void) {
	return "Powkiddy V90S";
}

void PLAT_getOsVersionInfo(char* output_str, size_t max_len)
{
	char os_release[512] = {0};
	char kernel[128] = {0};
	FILE *fp = popen(". /etc/os-release 2>/dev/null; printf '%s' \"$PRETTY_NAME\"", "r");
	if (fp) {
		fgets(os_release, sizeof(os_release), fp);
		pclose(fp);
	}
	getFile("/proc/sys/kernel/osrelease", kernel, sizeof(kernel));
	trimTrailingNewlines(os_release);
	trimTrailingNewlines(kernel);
	snprintf(output_str, max_len, "%s%s%s", os_release[0] ? os_release : "V90S stock OS", kernel[0] ? " / Linux " : "", kernel);
}

bool PLAT_btIsConnected(void)
{
	return false;
}

ConnectionStrength PLAT_connectionStrength(void) {
	return SIGNAL_STRENGTH_OFF;
}

void PLAT_initDefaultLeds() {
}
void PLAT_initLeds(LightSettings *lights)
{
}

void PLAT_setLedInbrightness(LightSettings *led)
{
}
void PLAT_setLedBrightness(LightSettings *led)
{
}
void PLAT_setLedEffect(LightSettings *led)
{
}
void PLAT_setLedEffectCycles(LightSettings *led)
{
}
void PLAT_setLedEffectSpeed(LightSettings *led)
{
}
void PLAT_setLedColor(LightSettings *led)
{
}

//////////////////////////////////////////////

bool PLAT_canTurbo(void) { return false; }

int PLAT_toggleTurbo(int btn_id)
{
	return 0;
}

void PLAT_clearTurbo() {
}

//////////////////////////////////////////////

int PLAT_setDateTime(int y, int m, int d, int h, int i, int s) {
	char cmd[512];
	int n = snprintf(cmd, sizeof(cmd), "date -s '%d-%d-%d %d:%d:%d'; hwclock -u -w", y,m,d,h,i,s);
	if (n < 0 || n >= (int)sizeof(cmd)) return -1;
	system(cmd);
	return 0; // why does this return an int?
}

#define MAX_LINE_LENGTH 200
#define ZONE_PATH "/usr/share/zoneinfo"
#define ZONE_TAB_PATH ZONE_PATH "/zone.tab"

static char cached_timezones[MAX_TIMEZONES][MAX_TZ_LENGTH];
static int cached_tz_count = -1;

int compare_timezones(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

void PLAT_initTimezones() {
    if (cached_tz_count != -1) { // Already initialized
        return;
    }

    FILE *file = fopen(ZONE_TAB_PATH, "r");
    if (!file) {
        LOG_info("Error opening file %s\n", ZONE_TAB_PATH);
        return;
    }

    char line[MAX_LINE_LENGTH];
    cached_tz_count = 0;

    while (fgets(line, sizeof(line), file)) {
        // Skip comment lines
        if (line[0] == '#' || strlen(line) < 3) {
            continue;
        }

        char *token = strtok(line, "\t"); // Skip country code
        if (!token) continue;

        token = strtok(NULL, "\t"); // Skip latitude/longitude
        if (!token) continue;

        token = strtok(NULL, "\t\n"); // Extract timezone
        if (!token) continue;

        // Check for duplicates before adding
        int duplicate = 0;
        for (int i = 0; i < cached_tz_count; i++) {
            if (strcmp(cached_timezones[i], token) == 0) {
                duplicate = 1;
                break;
            }
        }

        if (!duplicate && cached_tz_count < MAX_TIMEZONES) {
            strncpy(cached_timezones[cached_tz_count], token, MAX_TZ_LENGTH - 1);
            cached_timezones[cached_tz_count][MAX_TZ_LENGTH - 1] = '\0'; // Ensure null-termination
            cached_tz_count++;
        }
    }

    fclose(file);

    // Sort the list alphabetically
    qsort(cached_timezones, cached_tz_count, MAX_TZ_LENGTH, compare_timezones);
}

void PLAT_getTimezones(char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH], int *tz_count) {
    if (cached_tz_count == -1) {
        LOG_warn("Error: Timezones not initialized. Call PLAT_initTimezones first.\n");
        *tz_count = 0;
        return;
    }

    memcpy(timezones, cached_timezones, sizeof(cached_timezones));
    *tz_count = cached_tz_count;
}

// Batocera has no timedatectl; the timezone is a batocera.conf setting
// applied by making /etc/localtime point into /usr/share/zoneinfo.
char *PLAT_getCurrentTimezone() {
	char *output = (char *)malloc(256);
	if (!output) {
		return NULL;
	}
	FILE *fp = popen("readlink /etc/localtime 2>/dev/null | sed 's|.*/zoneinfo/||'", "r");
	if (!fp) {
		free(output);
		return NULL;
	}
	if (!fgets(output, 256, fp))
		output[0] = '\0';
	pclose(fp);
	trimTrailingNewlines(output);

	return output;
}

void PLAT_setCurrentTimezone(const char* tz) {
	if (cached_tz_count == -1) {
		LOG_warn("Error: Timezones not initialized. Call PLAT_initTimezones first.\n");
        return;
    }

	char cmd[512];
	snprintf(cmd, sizeof(cmd), "ln -sf '" ZONE_PATH "/%s' /etc/localtime", tz);
	system(cmd);
}

bool PLAT_getNetworkTimeSync(void) {
	// no network hardware, no NTP
	return false;
}

void PLAT_setNetworkTimeSync(bool on) {
}

/////////////////////////

// We use the generic video implementation here
#include "generic_video.c"
