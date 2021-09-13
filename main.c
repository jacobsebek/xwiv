/*
Written by Jakub Sebek (github.com/jacobsebek) in September 2021
Public domain

Resources used:

http://manuals.opensound.com/developer/
https://tronche.com/gui/x/xlib/

http://manuals.opensound.com/developer/singen.c.html
https://github.com/gavv/snippets/blob/master/xlib/xlib_hello.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <limits.h>
#include <sys/soundcard.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

// OSSv4 feature, not supported by the anicent linux header.
#ifdef SNDCTL_DSP_SETPLAYVOL
    #define VOLUME_SUPPORTED
#endif

extern void parse_wav(int fd, int* sample_rate, int* num_channels, int* format);

int chunk_size;
int input_fd, playback_fd, sample_rate, num_channels, format, sample_size, volume;
int width, height;
Display* display;
Window window;

void (*visualise)(void* samples);
extern void visualise_wave(void*);
extern void visualise_bars(void*);
extern void visualise_bars2(void*);
extern void visualise_fourier(void*);

void error(const char* fmt) {
    fputs(fmt, stderr);
    fputc('\n', stderr);
}

void error_exit(int excode, const char* fmt) {
    error(fmt);
    exit(excode);
}

void errno_exit(const char* msg) {
    perror(msg);
    exit(errno);
}

static void _parse_cmdline(int argc, char *argv[]) {

    input_fd = sample_rate = num_channels = format = sample_size -1;
    volume = 100;
    width = 320; height = 220;
    visualise = NULL;

    for (int o; o = getopt(argc, argv, "r:f:c:m:v:"), o != -1;)
        switch (o) {
        case 'r': // Sample rate
            sample_rate = strtol(optarg, NULL, 0);
            if (errno == EINVAL || errno == ERANGE) errno_exit("Invalid sample rate");
        break;
        case 'c': // Number of channels
            num_channels = strtol(optarg, NULL, 0);
            if (errno == EINVAL || errno == ERANGE) errno_exit("Invalid channel count");
        break;
        case 'f': // Format
            if (!strcmp(optarg, "s16le"))
                format = AFMT_S16_LE;
            else
                error_exit(EINVAL, "Invalid audio format");
        break;
        case 'v': // Volume
        #ifdef VOLUME_SUPPORTED
            volume = strtol(optarg, NULL, 0);
            if (errno == EINVAL || errno == ERANGE) errno_exit("Invalid volume");
            if (volume > 100 || volume < 0) error_exit(EINVAL, "Volume outside of range");
        #else
            error_exit(EINVAL, "Volume setting not supported");
        #endif
        break;
        case 'm': // Visualisation method
            if (!strcmp(optarg, "wave"))
                visualise = visualise_wave;
            else if (!strcmp(optarg, "bars"))
                visualise = visualise_bars;
            else if (!strcmp(optarg, "bars2"))
                visualise = visualise_bars2;
            else
                error_exit(EINVAL, "Invalid visualisation method");
        break;
        }

    if (optind >= argc) {
        error_exit(EINVAL, "No input specified");
    } else if (!strcmp(argv[optind], "-")) {
        input_fd = STDIN_FILENO;
    } else {
        input_fd = open(argv[optind], O_RDONLY);
        if (input_fd == -1) errno_exit("Failed to open input file");
    }

    if (sample_rate != -1 || num_channels != -1 || format != -1) {
        if (sample_rate == -1) error("Sample rate not specified");
        if (num_channels == -1) error("Channel count not specified");
        if (format == -1) error("Audio format not specified");

        if (sample_rate == -1 || num_channels == -1 || format == -1)
            exit(EINVAL);
    } else
        parse_wav(input_fd, &sample_rate, &num_channels, &format);

    if (visualise == NULL)
        error_exit(EINVAL, "Visualisation method not specified");

    switch (format) {
    case AFMT_S16_LE:
        sample_size = sizeof(short);
    break;
    }

    chunk_size = sample_rate / 20;
}

static void _oss_setopt(int fd, unsigned long request, int val, const char* errmsg) {
    int res = val;
    if (ioctl(fd, request, &res) == -1) errno_exit(errmsg);
    if (res != val) error_exit(EINVAL, errmsg);
}

static void _oss_init() {
    playback_fd = open("/dev/dsp", O_WRONLY);
    if (playback_fd == -1) errno_exit("Failed to open default OSS audio device");

    _oss_setopt(playback_fd, SNDCTL_DSP_SETFMT, format, "Failed to set audio device format");
    _oss_setopt(playback_fd, SNDCTL_DSP_CHANNELS, num_channels, "Failed to set audio device channel count");
    _oss_setopt(playback_fd, SNDCTL_DSP_SPEED, sample_rate, "Failed to set audio device sample rate");

    #ifdef VOLUME_SUPPORTED
        int volume_channels = 0;
        for (char* b = (char*)&volume_channels; b-(char*)&volume_channels < sizeof(int); *b++ |= (char)volume);
        _oss_setopt(playback_fd, SNDCTL_DSP_SETPLAYVOL, volume_channels, "Failed to set volume");
    #endif
}

static void _xlib_init() {
    display = XOpenDisplay(NULL);
    if (!display) error_exit(-1, "Failed to connect to the X server");

    unsigned long black = BlackPixel(display, DefaultScreen(display)),
                  white = WhitePixel(display, DefaultScreen(display));
    window = XCreateSimpleWindow(
        display, DefaultRootWindow(display), 
        0, 0, width, height, 0, 
        black, black
    );
    XSetForeground(display, XDefaultGC(display, 0), white);
    XStoreName(display, window, "XWiv");
    XMapWindow(display, window);
}

static ssize_t _read_full(int fd, void* buf, size_t nbytes) {
    ssize_t total = 0;
    for (ssize_t ret; total < nbytes; total += ret) {
        ret = read(fd, buf+total, nbytes-total);
        if (ret == -1) return ret;
        if (ret == 0) break;
    }
    return total;
}

static void _loop() {
    XWindowAttributes winattr;
    XGetWindowAttributes(display, window, &winattr);

    char samples[chunk_size * (sample_size * num_channels)];
    for (int ssize = 0; 
        ssize = _read_full(input_fd, samples, sizeof(samples)),
        ssize == sizeof(samples);) {

        if (ssize == -1)
            errno_exit("Failed to read audio sample");

        if (write(playback_fd, samples, ssize) < ssize)
            errno_exit("Failed to write audio sample");

        XClearWindow(display, window);
        visualise(samples);
        XFlush(display);
    }

}

int main(int argc, char *argv[]) {

    _parse_cmdline(argc, argv);
    _oss_init();
    _xlib_init();
    _loop();

    close(input_fd);
    close(playback_fd);
    XCloseDisplay(display);
    exit(0);
}
