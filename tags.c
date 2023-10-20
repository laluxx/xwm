#include "tags.h"
#include <X11/Xutil.h>

Workspace workspaces[NUM_WORKSPACES];
int current_workspace = 0;

extern Display *display;
extern int screen;
extern int focused_window;
extern int nwindows;
extern Window* windows;
extern void tile();

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

    if ((event->xkey.state & Mod4Mask) && !(event->xkey.state & ShiftMask) && keysym >= XK_1 && keysym <= XK_9) {
        int index = keysym - XK_1;
        switch_to_workspace(index);
    } else if ((event->xkey.state & (Mod4Mask | ShiftMask)) == (Mod4Mask | ShiftMask) && keysym >= XK_1 && keysym <= XK_9) {
        int index = keysym - XK_1;
        if (nwindows > 0 && focused_window >= 0 && focused_window < nwindows) {
            send_window_to_workspace(windows[focused_window], index);
        }
    }
}
