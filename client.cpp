#include <stdio.h>
#include <wayland-client.h>

int 
main ()
{
    int return_code = 0;
    struct wl_display *display = wl_display_connect(NULL);

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
