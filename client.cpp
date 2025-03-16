#include <stdio.h>
#include <wayland-client.h>
#include <string.h>

#include "include/types.h"

#include "src/shm_alloc.cpp"

struct client_state
{
    wl_compositor *compositor;
    wl_shm *shm;
};


static void 
registry_handle_global(void *data, wl_registry *registry, u32 name, const char *interface, u32 version)
{
    (void)version;
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

        const int width = 1920;
        const int height = 1080;
        const int stride = width * 4;
        const int shm_pool_size = height * stride * 2;

        int fd = allocate_shm_file(shm_pool_size);
        u8 *pool_data = (u8*)mmap(
            NULL,
            shm_pool_size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0
        );
        wl_shm_pool *pool = wl_shm_create_pool(state.shm, fd, shm_pool_size);

        int index = 0;
        int offset = height * stride * index;
        wl_buffer *buffer = wl_shm_pool_create_buffer(pool, offset, width, height, stride, WL_SHM_FORMAT_XRGB8888);

        u32 *pixels = (u32*)&pool_data[offset];
        memset(pixels, 0, width * height * 4);

        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, UINT32_MAX, UINT32_MAX);
        wl_surface_commit(surface);

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
