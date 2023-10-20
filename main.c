#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "tags.h"

#include <signal.h>
#include <sys/wait.h>


#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))


// Configuration
float master_size_factor = 0.6;
int gap = 10;

// Globals
Display *display;
Window root;
int screen;

int focused_window = 0;
Window* windows = NULL;
int nwindows = 0;

void spawn(const char* cmd) {
    if (fork() == 0) {
        system(cmd);
        exit(0);
    }
}

void focus_next() {
    if (nwindows > 0) {
        focused_window = (focused_window + 1) % nwindows;
        XSetInputFocus(display, windows[focused_window], RevertToParent, CurrentTime);
    }
}

void focus_prev() {
    if (nwindows > 0) {
        focused_window = (focused_window - 1 + nwindows) % nwindows;
        XSetInputFocus(display, windows[focused_window], RevertToParent, CurrentTime);
    }
}

void tile() {
    int width = DisplayWidth(display, screen) - 2 * gap;
    int height = DisplayHeight(display, screen) - 2 * gap;

    if (nwindows == 1) {
        XMoveResizeWindow(display, windows[0], gap, gap, width, height);
    } else if (nwindows > 1) {
        int master_width = master_size_factor * width;
        int slave_width = width - master_width - gap;
        int slave_height = (height - (nwindows - 2) * gap) / (nwindows - 1);

        XMoveResizeWindow(display, windows[0], gap, gap, master_width, height);
        for (int i = 1; i < nwindows; ++i) {
            XMoveResizeWindow(display, windows[i], master_width + 2 * gap, gap + (i - 1) * (slave_height + gap), slave_width, slave_height);
        }
    }
}

void remove_window(Window w) {
    int index = -1;
    for (int i = 0; i < nwindows; i++) {
        if (windows[i] == w) {
            index = i;
            break;
        }
    }

    if (index != -1) {
        for (int i = index; i < nwindows - 1; i++) {
            windows[i] = windows[i + 1];
        }
        nwindows--;
        windows = realloc(windows, nwindows * sizeof(Window));

        if (focused_window >= nwindows && nwindows > 0) {
            focused_window = nwindows - 1;
        } else if (nwindows == 0) {
            focused_window = 0;
        }

        tile();
    }
}

void close_focused() {
    if (nwindows > 0) {
        XDestroyWindow(display, windows[focused_window]);
        remove_window(windows[focused_window]);
    }
}

void rotate_stack(int direction) {
    if (nwindows < 2) return; // No need to swap for fewer than 2 windows.

    int swap_index = -1;

    if (direction == 1) {
        // For Super + Shift + j
        if (focused_window == nwindows - 1) {
            swap_index = 0; // Wrap around to the first window.
        } else {
            swap_index = focused_window + 1; // Swap with the next window.
        }
    } else if (direction == -1) {
        // For Super + Shift + k
        if (focused_window == 0) {
            swap_index = nwindows - 1; // Wrap around to the last window.
        } else {
            swap_index = focused_window - 1; // Swap with the previous window.
        }
    }

    if (swap_index != -1) {
        Window temp = windows[focused_window];
        windows[focused_window] = windows[swap_index];
        windows[swap_index] = temp;

        // Move the focus to the swapped position.
        focused_window = swap_index;
    }
}

void handle_event(XEvent *event) {
    if (event->type == KeyPress) {
        KeySym keysym = XkbKeycodeToKeysym(display, event->xkey.keycode, 0, 0);

        // Handle shift key combinations first
        if ((event->xkey.state & Mod4Mask) && (event->xkey.state & ShiftMask) && (keysym == XK_j)) {
            rotate_stack(1);  // Rotating the stack forward for Super + Shift + j
            tile();
        } else if ((event->xkey.state & Mod4Mask) && (event->xkey.state & ShiftMask) && (keysym == XK_k)) {
            rotate_stack(-1);  // Rotating the stack backward for Super + Shift + k
            tile();
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_Return)) {
            spawn("kitty");
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_h)) {
            master_size_factor = max(0.1, master_size_factor - 0.05);
            tile();
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_p)) {
            spawn("dmrun");
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_l)) {
            master_size_factor = min(0.9, master_size_factor + 0.05);
            tile();
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_j)) {
            focus_next();
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_k)) {
            focus_prev();
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_q)) {
            close_focused();
        }
        handle_workspace_keys(event);
    } else if (event->type == MapRequest) {
        /* windows = realloc(windows, (nwindows + 1) * sizeof(Window)); */
        /* windows[nwindows] = event->xmaprequest.window; */
        /* XMapWindow(display, event->xmaprequest.window); */
        /* // Set the newly opened window as the focused one. */
        /* focused_window = nwindows; */
        /* nwindows++; */
        /* tile(); */

        XMapWindow(display, event->xmaprequest.window);
        windows = realloc(windows, (nwindows + 1) * sizeof(Window));
        windows[nwindows] = event->xmaprequest.window;
        focused_window = nwindows;
        nwindows++;
        tile();
    } else if (event->type == DestroyNotify) {
        remove_window(event->xdestroywindow.window);
    }
}

int main(void) {
    initialize_workspaces();
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    screen = DefaultScreen(display);
    root = RootWindow(display, screen);

    XSelectInput(display, root, SubstructureNotifyMask | SubstructureRedirectMask | KeyPressMask);

    // Grabbing keys
    XGrabKey(display, XKeysymToKeycode(display, XK_Return), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_h), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_l), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_j), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_k), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_q), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);

    XGrabKey(display, XKeysymToKeycode(display, XK_j), Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, XKeysymToKeycode(display, XK_k), Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);


    for (int i = XK_1; i <= XK_9; i++) {
        XGrabKey(display, XKeysymToKeycode(display, i), Mod4Mask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(display, XKeysymToKeycode(display, i), Mod4Mask | ShiftMask, root, True, GrabModeAsync, GrabModeAsync);
    }

    XEvent event;
    while (1) {
        XNextEvent(display, &event);
        handle_event(&event);
    }

    XCloseDisplay(display);

    // Clean up windows for each workspace
    for (int i = 0; i < NUM_WORKSPACES; i++) {
        free(workspaces[i].windows);
    }

    // Clean up the global windows list
    free(windows);

    return 0;
}
