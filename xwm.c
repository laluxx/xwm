#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/Xutil.h>
#include "config.h"


#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))


// Globals
Display *display;
Window root;
int screen;

int focused_window = 0;
Window* windows = NULL;
int nwindows = 0;




// Layouts
void tile() {
    int width = DisplayWidth(display, screen) - 2 * gap;
    int height = DisplayHeight(display, screen) - 2 * gap;

    if (nwindows == 1) {
        XMoveResizeWindow(display, windows[0], gap, gap, width, height);
    } else if (nwindows > 1) {
        int master_width = mfact * width;
        int slave_width = width - master_width - gap;
        int slave_height = (height - (nwindows - 2) * gap) / (nwindows - 1);

        XMoveResizeWindow(display, windows[0], gap, gap, master_width, height);
        for (int i = 1; i < nwindows; ++i) {
            XMoveResizeWindow(display, windows[i], master_width + 2 * gap, gap + (i - 1) * (slave_height + gap), slave_width, slave_height);
        }
    }
}







// WORKSPACES
typedef struct {
    Window* windows;
    int nwindows;
    int focused_window;
} Workspace;

Workspace workspaces[NUM_WORKSPACES];

Bool window_exists(Display *disp, Window w) {
    XWindowAttributes wa;
    return (XGetWindowAttributes(disp, w, &wa) != 0);
}

void initialize_workspaces() {
    for (int i = 0; i < NUM_WORKSPACES; i++) {
        workspaces[i].windows = NULL;
        workspaces[i].nwindows = 0;
        workspaces[i].focused_window = 0;
    }
}

void map_workspace(int index) {
    Workspace* ws = &workspaces[index];
    for (int i = 0; i < ws->nwindows; i++) {
        if (window_exists(display, ws->windows[i])) {
            XMapWindow(display, ws->windows[i]);
        }
    }
    tile();
}

void unmap_workspace(int index) {
    Workspace* ws = &workspaces[index];
    for (int i = 0; i < ws->nwindows; i++) {
        if (window_exists(display, ws->windows[i])) {
            XUnmapWindow(display, ws->windows[i]);
        }
    }
}

void switch_to_workspace(int index) {
    if (index < 0 || index >= NUM_WORKSPACES) return;

    // 1. Store the current windows in the old workspace
    workspaces[current_workspace].windows = windows;
    workspaces[current_workspace].nwindows = nwindows;
    workspaces[current_workspace].focused_window = focused_window;

    unmap_workspace(current_workspace);
    current_workspace = index;

    windows = workspaces[current_workspace].windows;
    nwindows = workspaces[current_workspace].nwindows;
    focused_window = (nwindows > 0) ? workspaces[current_workspace].focused_window : 0;

    map_workspace(current_workspace);
}

void send_window_to_workspace(Window w, int index) {
    // Validation: check the workspace index and window existence.
    if (index < 0 || index >= NUM_WORKSPACES || !window_exists(display, w))
        return;

    // Hide the window.
    XUnmapWindow(display, w);

    // Add the window to the target workspace.
    Workspace* target_ws = &workspaces[index];
    Window* tmp = realloc(target_ws->windows, (target_ws->nwindows + 1) * sizeof(Window));
    if (!tmp) {
        // Allocation error.
        return;
    }
    target_ws->windows = tmp;
    target_ws->windows[target_ws->nwindows++] = w;

    // Remove the window from the current workspace.
    int window_found = -1;
    for (int i = 0; i < nwindows; i++) {
        if (windows[i] == w) {
            window_found = i;
            break;
        }
    }

    // If the window is found in the current workspace, remove it.
    if (window_found != -1) {
        // Shift the remaining windows to the left to fill the gap.
        for (int j = window_found; j < nwindows - 1; j++) {
            windows[j] = windows[j + 1];
        }
        nwindows--;

        // Reallocate the windows array to its new size.
        Window* new_windows = realloc(windows, nwindows * sizeof(Window));
        if (!new_windows) {
            // Allocation error.
            return;
        }
        windows = new_windows;

        // Update the focused window index.
        if (focused_window == window_found && nwindows > 0) {
            focused_window = 0;
        } else if (focused_window > window_found) {
            focused_window--;
        }
    }

    // Re-tile the current workspace windows.
    tile();
}

void handle_workspace_keys(XEvent *event) {
    KeySym keysym = XkbKeycodeToKeysym(display, event->xkey.keycode, 0, 0);

    if (keysym < XK_1 || keysym > XK_9) {
        return; // Not a workspace key, so exit early.
    }

    int index = keysym - XK_1;

    // If the key combination is for the current workspace, do nothing and return.
    if (index == current_workspace) {
        return;
    }

    if ((event->xkey.state & Mod4Mask) && !(event->xkey.state & ShiftMask)) {
        switch_to_workspace(index);
    } else if ((event->xkey.state & (Mod4Mask | ShiftMask)) == (Mod4Mask | ShiftMask)) {
        if (nwindows > 0 && focused_window >= 0 && focused_window < nwindows) {
            send_window_to_workspace(windows[focused_window], index);
        }
    }
}

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
            spawn(terminalcmd);
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_p)) {
            spawn(dmenucmd);
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_h)) {
            mfact = max(0.1, mfact - 0.05);
            tile();
        } else if ((event->xkey.state & Mod4Mask) && (keysym == XK_l)) {
            mfact = min(0.9, mfact + 0.05);
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
