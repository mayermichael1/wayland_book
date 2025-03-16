#include <stdio.h>
#include <wayland-server.h>

int 
main()
{
    int return_code = 0;
    struct wl_display *display = wl_display_create();

    if(display)
    {
        const char *socket = wl_display_add_socket_auto(display);
        if(socket)
        {
            fprintf(stderr, "Running Wayland Display on %s\n", socket);
            wl_display_run(display);
            wl_display_destroy(display);
        }
        else
        {
            fprintf(stderr, "Unable to add socket to Wayland display.\n");
            return_code = 1;
        }
    }
    else
    {
        fprintf(stderr, "Unable to create display.\n");
        return_code = 1;
    }

    return return_code;
}
