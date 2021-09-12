/*
Written by Jakub Sebek (github.com/jacobsebek) in September 2021
Public domain

Resources used:
*/

#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <sys/soundcard.h>
#include <X11/Xlib.h>

extern const int chunk_size, num_channels, format;
extern const int width, height;
extern Display* display;
extern Window window;
int rect_width = 8;

static float _get_norm_sample(void* samples, int index, int channel) {
    switch (format) {
    case AFMT_S16_LE:
        return ((signed short*)samples)[index * num_channels + channel] / (float)SHRT_MAX;
    break;
    }

    return 0.0f;
}

static void init_rect(XRectangle* rect, int index, float normval, int middle) {
    rect->width = rect_width - 1;
    rect->x = index * rect_width; 
    
    if (middle) {
        int bheight = (int)(height * normval/2);
        if (bheight > 0) rect->y = height/2 - bheight;
        else             rect->y = height/2;

        rect->height = abs(bheight);
    } else {
        int bheight = (int)(height * fabsf(normval));
        rect->y = height - bheight;
        rect->height = bheight;
    }
}

void visualise_bars2(void* samples) {
    for (int i = 0; i < width/rect_width; i++) {
        XRectangle rect;
        init_rect(&rect, i, _get_norm_sample(samples, (int)((float)(i*rect_width)/width * chunk_size), 0), 1);
        XFillRectangles(display, window, XDefaultGC(display, 0), &rect, 1);
    }
}

void visualise_bars(void* samples) {
    for (int i = 0; i < width/rect_width; i++) {
        XRectangle rect;
        init_rect(&rect, i, _get_norm_sample(samples, (int)((float)(i*rect_width)/width * chunk_size), 0), 0);
        XFillRectangles(display, window, XDefaultGC(display, 0), &rect, 1);
    }
}

void visualise_wave(void* samples) {
    int lx = 0, ly = height/2;
    for (int i = 0; i < chunk_size; i++) {
        int x = (int)(((float)width/chunk_size) * i * num_channels);
        int y = height/2 - (int)(height * _get_norm_sample(samples, i, 0)/2);
        XDrawLine(display, window, XDefaultGC(display, 0), lx, ly, x, y);
        lx = x; ly = y;
    }
}
