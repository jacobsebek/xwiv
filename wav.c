/*
Written by Jakub Sebek (github.com/jacobsebek) in September 2021
Public domain

Resources used:

http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
http://soundfile.sapp.org/doc/WaveFormat/
*/
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <sys/soundcard.h>

#define ERRMSG_GENERIC "Failed to parse WAVE file"

extern void error(const char* fmt);
extern void error_exit(int excode, const char* fmt);
extern void errno_exit(const char* msg);

static int _is_little_endian() {
    int n = 1;
    return (*(char*)&n == 1);
}

static void _read_str(int fd, char* buf, int len) {
    int ret;
    if ((ret = read(fd, buf, len)) == -1) 
        errno_exit(ERRMSG_GENERIC);
    if (ret != len) error_exit(EINVAL, ERRMSG_GENERIC);
}

static void _require_str(int fd, const char* str) {
    char buf[8];
    for (int bufsize, len = strlen(str); len > 0; str += bufsize, len -= bufsize) {
        if (( bufsize = read(fd, buf, (len > sizeof(buf) ? sizeof(buf) : len)) ) == -1)
            errno_exit(ERRMSG_GENERIC);
        if (strncmp(buf, str, bufsize) != 0) error_exit(EINVAL, ERRMSG_GENERIC);
    }
}

static uint32_t _read_4(int fd) {
    uint32_t val;
    int ret;
    if ((ret = read(fd, &val, sizeof(val))) == -1 || ret != sizeof(val))
        errno_exit(ERRMSG_GENERIC);
    if (ret != sizeof(val)) error_exit(EINVAL, ERRMSG_GENERIC);

    if (!_is_little_endian())
        val = (val >> 24) & 0x000000FF |
              (val << 24) & 0xFF000000 |
              (val >>  8) & 0x0000FF00 |
              (val <<  8) & 0x00FF0000;

    return val;
}

static uint16_t _read_2(int fd) {
    uint16_t val;
    int ret;
    if ((ret = read(fd, &val, sizeof(val))) == -1 || ret != sizeof(val))
        errno_exit(ERRMSG_GENERIC);
    if (ret != sizeof(val)) error_exit(EINVAL, ERRMSG_GENERIC);

    if (!_is_little_endian())
        val = (val >> 8) & 0x00FF |
              (val << 8) & 0xFF00;

    return val;
}

// Parses the header of a WAVE file. The file descriptor is located
// at the start of the "data" chunk after returning.
void parse_wav(int fd, int* sample_rate, int* num_channels, int* format) {
    // RIFF file tag, must be RIFF
    _require_str(fd, "RIFF");

    //_ File size
    _read_4(fd);

    // File type, must be WAVE 
    _require_str(fd, "WAVE");

    // Format subchunk, must be "fmt "
    _require_str(fd, "fmt ");

    //_ Size of the format chunk
    _read_4(fd);

    // Format, only PCM (1) supported
    uint16_t audio_fmt = _read_2(fd);
    if (audio_fmt != 1)
        error_exit(EINVAL, "Audio encoding not supported");

    // Number of channels
    *num_channels = _read_2(fd); 

    // Sample rate
    *sample_rate = _read_4(fd);

    //_ Byte rate
    _read_4(fd);

    //_ Frame size
    _read_2(fd);

    // Bits per sample (format)
    uint16_t sample_bits = _read_2(fd);
    switch (sample_bits) {
        case 16: *format = AFMT_S16_LE; break;
        default:
            error_exit(EINVAL, "Audio format not supported");
    }

    // Read chunks until data is found
    for (char cidbuf[4]; _read_str(fd, cidbuf, 4), strncmp(cidbuf, "data", 4) != 0;)
        if (lseek(fd, _read_4(fd), SEEK_CUR) == -1) errno_exit(ERRMSG_GENERIC);

    //_ Size of the data
    _read_4(fd);
}

