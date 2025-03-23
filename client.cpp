#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <wayland-client.h>
#include <stdio.h>
#include <xkbcommon/xkbcommon.h>
#include <assert.h>

#include "include/xdg-shell-client-protocol.h"
#include "include/types.h"

#include "src/xdg-shell-protocol.c"
#include "src/shm_alloc.cpp"

struct pointer_event 
{
    u32 event_mask;
    wl_fixed_t surface_x;
    wl_fixed_t surface_y;
    u32 button;
    u32 state;
    u32 time;
    u32 serial;
    struct
    {
        b8 valid;
        wl_fixed_t value;
        s32 discrete;
    } axes[2];
    u32 axis_source;
};

u8 *axis_name[2] = 
{
    [WL_POINTER_AXIS_VERTICAL_SCROLL] = (u8*)"vertical",
    [WL_POINTER_AXIS_HORIZONTAL_SCROLL] = (u8*)"horizontal",
};

u8 *axis_source[4] = 
{
    [WL_POINTER_AXIS_SOURCE_WHEEL] = (u8*)"wheel",
    [WL_POINTER_AXIS_SOURCE_FINGER] = (u8*)"finger",
    [WL_POINTER_AXIS_SOURCE_CONTINUOUS] = (u8*)"continuous",
    [WL_POINTER_AXIS_SOURCE_WHEEL_TILT] = (u8*)"wheel tilt",
};

/* Wayland code */
struct client_state {
    /* Globals */
    wl_display *wl_display;
    wl_registry *wl_registry;
    wl_shm *wl_shm;
    wl_compositor *wl_compositor;
    xdg_wm_base *xdg_wm_base;
    wl_seat *wl_seat;
    /* Objects */
    wl_surface *wl_surface;
    xdg_surface *xdg_surface;
    xdg_toplevel *xdg_toplevel;
    wl_keyboard *wl_keyboard;
    wl_pointer *wl_pointer;
    wl_touch *wl_touch;

    f32 offset;
    u32 last_frame;
    pointer_event pointer_event;
    xkb_state *xkb_state;
    xkb_context *xkb_context;
    xkb_keymap *xkb_keymap;
};

enum pointer_event_mask 
{
    POINTER_EVENT_ENTER     = 1 << 0,
    POINTER_EVENT_LEAVE     = 1 << 1,
    POINTER_EVENT_MOTION    = 1 << 2,
    POINTER_EVENT_BUTTON    = 1 << 3,
    POINTER_EVENT_AXIS      = 1 << 4,
    POINTER_EVENT_AXIS_SOURCE   = 1 << 5,
    POINTER_EVENT_AXIS_STOP     = 1 << 6,
    POINTER_EVENT_AXIS_DISCRETE = 1 << 7,
};

internal void
wl_buffer_release(void *data, wl_buffer *wl_buffer)
{
    (void)data;
    wl_buffer_destroy(wl_buffer);
}

global_variable wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

internal wl_buffer *
draw_frame(client_state *state)
{
    const int width = 640, height = 480;
    int stride = width * 4;
    int size = stride * height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        return NULL;
    }

    u32 *data = (u32*)mmap(
        NULL,
        size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (data == MAP_FAILED)
    {
        close(fd);
        return NULL;
    }

    wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
    wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool,
        0,
        width,
        height,
        stride,
        WL_SHM_FORMAT_XRGB8888
    );

    //TODO: why is this destroyed and closed here
    wl_shm_pool_destroy(pool); 
    close(fd);

    /* Draw checkerboxed background */
    int offset = (int)state->offset % 8;
    for (int y = 0; y < height; ++y) 
    {
        for (int x = 0; x < width; ++x) 
        {
            if (((x + offset) + ( y + offset) / 8 * 8) % 16 < 8)
                data[y * width + x] = 0xFF666666;
            else
                data[y * width + x] = 0xFFEEEEEE;
        }
    }

    munmap(data, size);
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    return buffer;
}

internal void
wl_surface_frame_done(void *data, wl_callback *cb, u32);

global_variable wl_callback_listener wl_surface_frame_listener = 
{
    .done = wl_surface_frame_done,
};

internal void
wl_surface_frame_done(void *data, wl_callback *cb, u32 time)
{
    client_state *state = (client_state*)data;
    wl_callback_destroy(cb);

    cb = wl_surface_frame(state->wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, state);

    if (state->last_frame != 0)
    {
        s32 elapsed = time - state->last_frame;
        state->offset += elapsed / 1000.0 * 24;
    }

    wl_buffer *buffer = draw_frame(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_damage(state->wl_surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(state->wl_surface);

    state->last_frame = time;
}

internal void
xdg_surface_configure(void *data, xdg_surface *xdg_surface, u32 serial)
{
    client_state *state = (client_state*)data;
    xdg_surface_ack_configure(xdg_surface, serial);

    wl_buffer *buffer = draw_frame(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_commit(state->wl_surface);
}

global_variable xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

/// KEYBOARD

internal void
wl_keyboard_keymap(void *data, wl_keyboard *wl_keyboard, u32 format, s32 fd, u32 size)
{
    client_state *state = (client_state*)data;
    u8 *map_shm = (u8*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

    xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
        state->xkb_context,
        (const char*)map_shm,
        XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS
    );
    munmap(map_shm, size);
    close(fd);

    xkb_keymap_unref(state->xkb_keymap);
    xkb_state_unref(state->xkb_state);

    xkb_state *xkb_state = xkb_state_new(xkb_keymap);
    state->xkb_keymap = xkb_keymap;
    state->xkb_state = xkb_state;

}

internal void
wl_keyboard_enter(void *data, wl_keyboard *wl_keyboard, u32 serial, wl_surface *surface, wl_array *keys)
{
    client_state *state = (client_state*)data;
    printf("keyboard enter; keys pressed are:\n");

    u32 *key;
    wl_array_for_each(key, keys)
    {
        char buf[128];
        xkb_keysym_t sym = xkb_state_key_get_one_sym( state->xkb_state, *key + 8);
        xkb_keysym_get_name(sym, buf, sizeof(buf));
        printf("sym: %-12s (%d),", buf, sym);
        xkb_state_key_get_utf8(state->xkb_state, *key + 8, buf, sizeof(buf));
        printf("utf8: '%s\n'", buf);
    }
}

internal void
wl_keyboard_leave(void *data, wl_keyboard *wl_keyboard, u32 serial, wl_surface *wl_surface)
{
    printf("keyboard leave\n");
}

internal void
wl_keyboard_modifiers(void *data, wl_keyboard *wl_keyboard, u32 serial, u32 mods_depressed, u32 mods_latched, u32 mods_locked, u32 group)
{
    client_state *state = (client_state*)data;
    xkb_state_update_mask(state->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

internal void
wl_keyboard_repeat_info(void *data, wl_keyboard *wl_keyboard, s32 rate, s32 delay)
{
    // empty
}

internal void
wl_keyboard_key(void *data, wl_keyboard *wl_keyboard, u32 serial, u32 time, u32 key, u32 key_state)
{
    client_state *state = (client_state*)data;

    char buf[128];
    u32 keycode = key + 8;

    xkb_keysym_t sym = xkb_state_key_get_one_sym(state->xkb_state, keycode);
    xkb_keysym_get_name(sym, buf, sizeof(buf));

    u8 *action = key_state == WL_KEYBOARD_KEY_STATE_PRESSED ? (u8*)"press" : (u8*)"release";
    printf("key %s: sym: %-12s (%d)", action, buf, sym);

    xkb_state_key_get_utf8(state->xkb_state, keycode, buf, sizeof(buf));
    printf("utf8: '%s'\n", buf);
}

global_variable wl_keyboard_listener wl_keyboard_listener = 
{
    .keymap = wl_keyboard_keymap,
    .enter  = wl_keyboard_enter,
    .leave  = wl_keyboard_leave,
    .key    = wl_keyboard_key,
    .modifiers = wl_keyboard_modifiers,
    .repeat_info = wl_keyboard_repeat_info,
};

/// POINTER

internal void
wl_pointer_enter(void *data, wl_pointer *wl_pointer, u32 serial,
    wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    client_state *state = (client_state*)data;
    state->pointer_event.event_mask |= POINTER_EVENT_ENTER;
    state->pointer_event.serial = serial;
    state->pointer_event.surface_x = surface_x;
    state->pointer_event.surface_y = surface_y;
}

internal void
wl_pointer_leave(void *data, wl_pointer *wl_pointer, u32 serial, wl_surface *surface)
{
    client_state *state = (client_state*)data;
    state->pointer_event.serial = serial;
    state->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
}

internal void 
wl_pointer_motion(void *data, wl_pointer *wl_pointer, u32 time,
    wl_fixed_t surface_x, wl_fixed_t surface_y)
{
    client_state *state = (client_state*)data;
    state->pointer_event.event_mask |= POINTER_EVENT_MOTION;
    state->pointer_event.time = time;
    state->pointer_event.surface_x = surface_x;
    state->pointer_event.surface_y = surface_y;
}

internal void
wl_pointer_button(void *data, wl_pointer *wl_pointer, u32 serial, u32 time,
    u32 button, u32 button_state)
{
    client_state *state = (client_state*)data;
    state->pointer_event.event_mask |= POINTER_EVENT_BUTTON;
    state->pointer_event.time = time;
    state->pointer_event.serial = serial;
    state->pointer_event.button = button;
    state->pointer_event.state = button_state;
}

internal void
wl_pointer_axis(void *data, wl_pointer *wl_pointer, u32 time, u32 axis, wl_fixed_t value)
{
    client_state *state = (client_state*)data;
    state->pointer_event.event_mask |= POINTER_EVENT_AXIS;
    state->pointer_event.time = time;
    state->pointer_event.axes[axis].valid = true;
    state->pointer_event.axes[axis].value = value;
}

internal void
wl_pointer_axis_source(void *data, wl_pointer *wl_pointer, u32 axis_source)
{
    client_state *state = (client_state*)data;
    state->pointer_event.event_mask |= POINTER_EVENT_AXIS_SOURCE;
    state->pointer_event.axis_source = axis_source;
}

internal void
wl_pointer_axis_stop(void *data, wl_pointer *wl_pointer, u32 time, u32 axis)
{
    client_state *state = (client_state*)data;
    state->pointer_event.event_mask |= POINTER_EVENT_AXIS_STOP;
    state->pointer_event.time = time;
    state->pointer_event.axes[axis].valid = true;
}

internal void
wl_pointer_axis_discrete(void *data, wl_pointer *wl_pointer, u32 axis, s32 discrete)
{
    client_state *state = (client_state*)data;
    state->pointer_event.event_mask |= POINTER_EVENT_AXIS_DISCRETE;
    state->pointer_event.axes[axis].valid = true;
    state->pointer_event.axes[axis].discrete = discrete;
}

internal void
wl_pointer_frame(void *data, wl_pointer *wl_pointer)
{
    client_state *state = (client_state*)data;
    pointer_event *event = &state->pointer_event;
    printf("pointer frame @ %d :", event->time);

    if (event->event_mask & POINTER_EVENT_ENTER)
    {
        printf("entered %f, %f", 
            wl_fixed_to_double(event->surface_x),
            wl_fixed_to_double(event->surface_y)
        );
    }

    if (event->event_mask & POINTER_EVENT_LEAVE)
    {
        printf("leave");
    }

    if (event->event_mask & POINTER_EVENT_MOTION)
    {
        printf("motion %f, %f", 
            wl_fixed_to_double(event->surface_x),
            wl_fixed_to_double(event->surface_y)
        );
    }

    if (event->event_mask & POINTER_EVENT_BUTTON)
    {
        char *state = event->state == WL_POINTER_BUTTON_STATE_RELEASED
             ? (char*)"released" : (char*)"pressed";

        printf("button %d %s", event->button, state);
    }

    u32 axis_events = 
        POINTER_EVENT_AXIS | 
        POINTER_EVENT_AXIS_STOP |
        POINTER_EVENT_AXIS_DISCRETE;

    if (event->event_mask & axis_events)
    {
        for (u8 i = 0; i < 2; ++i)
        {
            if (event->axes[i].valid)
            {
                printf("%s axis ", axis_name[i]);
                if (event->event_mask & POINTER_EVENT_AXIS)
                {
                    printf("value %f ", wl_fixed_to_double(event->axes[i].value));
                }
                if (event->event_mask & POINTER_EVENT_AXIS_DISCRETE)
                {
                    printf("discrete %f ", wl_fixed_to_double(event->axes[i].discrete));
                }
                if (event->event_mask & POINTER_EVENT_AXIS_SOURCE)
                {
                    printf("via %s ", axis_source[event->axis_source]);
                }
                if (event->event_mask & POINTER_EVENT_AXIS_STOP)
                {
                    printf("stopped");
                }
            }
        }
    }
    printf(" END FRAME\n");
    memset(event, 0, sizeof(*event));

}

global_variable wl_pointer_listener wl_pointer_listener = 
{
    .enter = wl_pointer_enter,
    .leave = wl_pointer_leave,
    .motion = wl_pointer_motion,
    .button = wl_pointer_button,
    .axis = wl_pointer_axis,
    .frame = wl_pointer_frame,
    .axis_source = wl_pointer_axis_source,
    .axis_stop = wl_pointer_axis_stop,
    .axis_discrete = wl_pointer_axis_discrete,
};

/// SEAT

internal void
wl_seat_capabilities(void *data, wl_seat *wl_seat, u32 capabilities)
{
    client_state *state = (client_state*)data;

    bool have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;

    if (have_pointer && state->wl_pointer == NULL)
    {
        state->wl_pointer = wl_seat_get_pointer(state->wl_seat); 
        wl_pointer_add_listener( state->wl_pointer, &wl_pointer_listener, state);
    }
    else if (!have_pointer && state->wl_pointer != NULL)
    {
        wl_pointer_release(state->wl_pointer);
        state->wl_pointer = NULL;
    }

    b8 have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

    if (have_keyboard && state->wl_keyboard == NULL)
    {
       state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat); 
       wl_keyboard_add_listener(state->wl_keyboard, &wl_keyboard_listener, data);
    }
    else if (!have_keyboard && state->wl_keyboard != NULL)
    {
        wl_keyboard_release(state->wl_keyboard);
        state->wl_keyboard = NULL;
    }
}

internal void
wl_seat_name(void *data, wl_seat *wl_seat, const char *name)
{
    printf("seat name %s\n", name);
}

global_variable wl_seat_listener wl_seat_listener = 
{
    .capabilities = wl_seat_capabilities,
    .name = wl_seat_name,
};

/// XDG_WM_BASE

internal void
xdg_wm_base_ping(void *data, xdg_wm_base *xdg_wm_base, u32 serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

global_variable xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

/// REGISTRY 

internal void
registry_global(void *data, wl_registry *registry, u32 name, 
    const char *interface, u32 version)
{
    client_state *state = (client_state*)data;

    if (strcmp(interface, wl_shm_interface.name) == 0) 
    {
        state->wl_shm = (wl_shm*)wl_registry_bind(
            registry,
            name,
            &wl_shm_interface,
            1
        );
    } 
    else if (strcmp(interface, wl_compositor_interface.name) == 0) 
    {
        state->wl_compositor = (wl_compositor*)wl_registry_bind( 
            registry,
            name,
            &wl_compositor_interface,
            4
        );
    } 
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) 
    {
        state->xdg_wm_base = (xdg_wm_base*)wl_registry_bind(
            registry,
            name,
            &xdg_wm_base_interface,
            1
        );
        xdg_wm_base_add_listener(
            state->xdg_wm_base, 
            &xdg_wm_base_listener,
            state
        );
    }
    else if (strcmp(interface, wl_seat_interface.name) == 0)
    {
        state->wl_seat = (wl_seat*)wl_registry_bind(
            registry,
            name, 
            &wl_seat_interface,
            7
        );
        wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
    }
}

global_variable wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    //TODO: implement release
    /*.release = */
};

int
main()
{
    client_state state = { };
    state.wl_display = wl_display_connect(NULL);
    state.wl_registry = wl_display_get_registry(state.wl_display);
    state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(
        state.xdg_wm_base,
        state.wl_surface
    );
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_set_title(state.xdg_toplevel, "Example client");
    wl_surface_commit(state.wl_surface);

    wl_callback *cb = wl_surface_frame(state.wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, &state);
    while (wl_display_dispatch(state.wl_display)) 
    {

    }
    return 0;
}
