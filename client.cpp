#include <stdio.h>
#include <wayland-client.h>

int 
main (int argc, char *argv[])
{
    int return_code = 0;
    struct wl_display *display = NULL;
    char *display_name = NULL;

    if (argc >= 2)
    {
        display_name = argv[1];
    }

    if (display_name != NULL)
    {
        display = wl_display_connect(display_name);
    }

    if (display)
    {
        fprintf(stderr, "Connection established!\n");
        wl_display_disconnect(display);
    }
    else
    {
        fprintf(stderr, "Failed to connect to Wayland display.\n");
        return_code = 1;
    }

    return return_code;
}
