#include <stdio.h>
#include <wayland-client.h>
#include <string.h>

#include "include/types.h"

struct client_state
{
    wl_compositor *compositor;
    wl_shm *shm;
};

static void 
registry_handle_global(void *data, wl_registry *registry, u32 name, const char *interface, u32 version)
{
    client_state *state = (client_state*)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0)
    {
        state->compositor = (wl_compositor*)wl_registry_bind(registry, name, &wl_compositor_interface, 4);    
    }
    else if(strcmp(interface, wl_shm_interface.name) == 0)
    {
        state->shm = (wl_shm*)wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
    /*
    fprintf(stdout, "interface: \%s\(v: %d, name: %d)\n", 
        interface,
        version,
        name
    );
    */
}

static void
registry_handle_global_remove(void *data, wl_registry *registry, u32 name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static wl_registry_listener registry_listener =
{
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

int 
main (int argc, char *argv[])
{
    int return_code = 0;
    struct wl_display *display = NULL;
    struct wl_registry *registry = NULL;
    char *display_name = NULL;

    if (argc >= 2)
    {
        display_name = argv[1];
    }

    if (display_name != NULL)
    {
        display = wl_display_connect(display_name);
    }
    else
    {
        display = wl_display_connect(NULL);
    }

    if (display)
    {
        registry = wl_display_get_registry(display);
    }

    if(registry)
    {
        fprintf(stderr, "Connection established!\n");
        client_state state = {};
        wl_registry_add_listener(registry, &registry_listener, &state);
        wl_display_roundtrip(display);
        wl_display_disconnect(display);
    }
    else
    {
        fprintf(stderr, "Failed to connect to Wayland display.\n");
        return_code = 1;
    }

    return return_code;
}
