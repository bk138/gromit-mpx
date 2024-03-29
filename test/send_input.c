#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <device_id> <offset>\n", argv[0]);
        return 1;
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Unable to open display.\n");
        return 1;
    }

    int device_id = atoi(argv[1]);
    XDevice xdev;
    xdev.device_id = device_id;

    int offset = atoi(argv[2]);

    // Move the pointer to (100+offset, 100)
    int axes[] = {100 + offset, 100};
    XTestFakeDeviceMotionEvent(display, &xdev, 0, 0, axes, 2, 0);

    // Press the left button.
    XTestFakeDeviceButtonEvent(display, &xdev, 1, True, NULL, 0, 0);

    // Move the pointer to (300+offset, 300) in steps of 10
    for(int i = 0; i < 20; ++i) {
        axes[0] += 10;
        axes[1] += 10;
        XTestFakeDeviceMotionEvent(display, &xdev, 0, 0, axes, 2, 0);
        usleep(100*1000);
        XFlush(display);
    }

    // Release the left button.
    XTestFakeDeviceButtonEvent(display, &xdev, 1, False, NULL, 0, 0);

    // Close the display connection.
    XCloseDisplay(display);

    return 0;
}
