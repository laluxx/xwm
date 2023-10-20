#ifndef TAGS_H
#define TAGS_H

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <stdlib.h>

#define NUM_WORKSPACES 9


typedef struct {
    Window* windows;
    int nwindows;
    int focused_window;
} Workspace;

extern Workspace workspaces[NUM_WORKSPACES];

void initialize_workspaces();
void switch_to_workspace(int index);
void send_window_to_workspace(Window w, int index);
void map_workspace(int index);
void unmap_workspace(int index);
void handle_workspace_keys(XEvent *event);

#endif // TAGS_H
