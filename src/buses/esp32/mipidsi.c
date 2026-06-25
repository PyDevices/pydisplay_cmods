/*
 * SPDX-FileCopyrightText: 2026 Brad Barnett
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "rom/cache.h"

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/objarray.h"
#include "py/binary.h"

#include "esp_lcd_mipi_dsi.h"

typedef struct _mipidsi_bus_obj_t {
    mp_obj_base_t base;
    uint32_t frequency;
    uint8_t num_lanes;
    esp_lcd_dsi_bus_handle_t bus_handle;
} mipidsi_bus_obj_t;

typedef struct _mipidsi_display_obj_t {
    mp_obj_base_t base;
    mipidsi_bus_obj_t *bus;
    esp_lcd_panel_handle_t panel_handle;
    uint16_t width;
    uint16_t height;
    uint8_t color_depth;
    uint8_t virtual_channel;
    mp_buffer_info_t bufinfo;
} mipidsi_display_obj_t;

extern const mp_obj_type_t mipidsi_bus_type;
extern const mp_obj_type_t mipidsi_display_type;

// ========== Bus class ==========

static mp_obj_t mipidsi_bus_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_frequency, ARG_num_lanes };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_frequency, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 500000000} },
        { MP_QSTR_num_lanes, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 2} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    uint32_t freq = args[ARG_frequency].u_int;
    uint8_t num_lanes = args[ARG_num_lanes].u_int;

    // Validate num_lanes (1-4 supported by MIPI DSI)
    if (num_lanes < 1 || num_lanes > 4) {
        mp_raise_ValueError(MP_ERROR_TEXT("num_lanes must be between 1 and 4"));
    }

    mipidsi_bus_obj_t *self = m_new_obj(mipidsi_bus_obj_t);
    self->base.type = &mipidsi_bus_type;
    self->frequency = freq;
    self->num_lanes = num_lanes;
    self->bus_handle = NULL;

    mp_printf(&mp_plat_print, "MIPI DSI Bus initializing (frequency=%u Hz, lanes=%d)...\n", freq, num_lanes);

    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = num_lanes,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = freq / 1000000,
    };

    esp_err_t ret = esp_lcd_new_dsi_bus(&bus_config, &self->bus_handle);
    if (ret != ESP_OK) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to create MIPI DSI bus"));
    }

    mp_printf(&mp_plat_print, "MIPI DSI Bus initialized\n");
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mipidsi_bus_deinit(mp_obj_t self_in) {
    mipidsi_bus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->bus_handle != NULL) {
        self->bus_handle = NULL;
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mipidsi_bus_deinit_obj, mipidsi_bus_deinit);

static const mp_rom_map_elem_t mipidsi_bus_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&mipidsi_bus_deinit_obj)},
};
static MP_DEFINE_CONST_DICT(mipidsi_bus_locals_dict, mipidsi_bus_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mipidsi_bus_type,
    MP_QSTR_Bus,
    MP_TYPE_FLAG_NONE,
    make_new, mipidsi_bus_make_new,
    locals_dict, &mipidsi_bus_locals_dict);

// ========== Display class ==========

static mp_obj_t mipidsi_display_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_bus,
        ARG_init_sequence,
        ARG_width,
        ARG_height,
        ARG_hsync_pulse_width,
        ARG_hsync_back_porch,
        ARG_hsync_front_porch,
        ARG_vsync_pulse_width,
        ARG_vsync_back_porch,
        ARG_vsync_front_porch,
        ARG_pixel_clock_frequency,
        ARG_virtual_channel,
        ARG_rotation,
        ARG_color_depth,
        ARG_backlight_pin,
        ARG_brightness,
        ARG_native_frames_per_second,
        ARG_backlight_on_high,
    };

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_bus, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_init_sequence, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_width, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_height, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_hsync_pulse_width, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_hsync_back_porch, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_hsync_front_porch, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_vsync_pulse_width, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_vsync_back_porch, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_vsync_front_porch, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_pixel_clock_frequency, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_virtual_channel, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_rotation, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_color_depth, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 16} },
        { MP_QSTR_backlight_pin, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_brightness, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_native_frames_per_second, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 60} },
        { MP_QSTR_backlight_on_high, MP_ARG_KW_ONLY | MP_ARG_BOOL, {.u_bool = true} },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // Validate and extract bus object
    if (!mp_obj_is_type(args[ARG_bus].u_obj, &mipidsi_bus_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("bus must be a mipidsi.Bus object"));
    }
    mipidsi_bus_obj_t *bus = MP_OBJ_TO_PTR(args[ARG_bus].u_obj);

    // Extract init_sequence
    mp_buffer_info_t init_seq_bufinfo;
    mp_get_buffer_raise(args[ARG_init_sequence].u_obj, &init_seq_bufinfo, MP_BUFFER_READ);

    uint16_t width = args[ARG_width].u_int;
    uint16_t height = args[ARG_height].u_int;
    uint32_t pixel_clock_freq = args[ARG_pixel_clock_frequency].u_int;
    uint8_t virtual_channel = args[ARG_virtual_channel].u_int;
    uint8_t color_depth = args[ARG_color_depth].u_int;

    // Validate color_depth (16 or 24)
    if (color_depth != 16 && color_depth != 24) {
        mp_raise_ValueError(MP_ERROR_TEXT("color_depth must be 16 or 24"));
    }

    // Validate dimensions
    if (width == 0 || height == 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("width and height must be non-zero"));
    }

    mipidsi_display_obj_t *self = m_new_obj(mipidsi_display_obj_t);
    self->base.type = &mipidsi_display_type;
    self->bus = bus;
    self->width = width;
    self->height = height;
    self->color_depth = color_depth;
    self->virtual_channel = virtual_channel;
    self->panel_handle = NULL;
    self->bufinfo.buf = NULL;
    self->bufinfo.len = 0;
    self->bufinfo.typecode = 'B';

    mp_printf(&mp_plat_print, "MIPI DSI Display initializing (%dx%d, %d-bit color)...\n", width, height, color_depth);

    if (pixel_clock_freq == 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("pixel_clock_frequency must be non-zero"));
    }

    esp_lcd_dpi_panel_config_t panel_config = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = pixel_clock_freq / 1000000,
        .virtual_channel = virtual_channel,
        .pixel_format = (color_depth == 24) ? LCD_COLOR_PIXEL_FORMAT_RGB888 : LCD_COLOR_PIXEL_FORMAT_RGB565,
        .in_color_format = (color_depth == 24) ? LCD_COLOR_FMT_RGB888 : LCD_COLOR_FMT_RGB565,
        .out_color_format = (color_depth == 24) ? LCD_COLOR_FMT_RGB888 : LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = width,
            .v_size = height,
            .hsync_back_porch = args[ARG_hsync_back_porch].u_int,
            .hsync_pulse_width = args[ARG_hsync_pulse_width].u_int,
            .hsync_front_porch = args[ARG_hsync_front_porch].u_int,
            .vsync_back_porch = args[ARG_vsync_back_porch].u_int,
            .vsync_pulse_width = args[ARG_vsync_pulse_width].u_int,
            .vsync_front_porch = args[ARG_vsync_front_porch].u_int,
        },
        .flags = {
            .use_dma2d = 0,
            .disable_lp = 0,
        },
    };

    esp_err_t ret = esp_lcd_new_panel_dpi(bus->bus_handle, &panel_config, &self->panel_handle);
    if (ret != ESP_OK) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to create MIPI DSI DPI panel"));
    }

    ret = esp_lcd_panel_reset(self->panel_handle);
    if (ret != ESP_OK) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to reset MIPI DSI panel"));
    }

    ret = esp_lcd_panel_init(self->panel_handle);
    if (ret != ESP_OK) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to initialize MIPI DSI panel"));
    }

    // Note: init_sequence is accepted for API compatibility but not parsed by this implementation.
    (void)init_seq_bufinfo;

    void *fb = NULL;
    ret = esp_lcd_dpi_panel_get_frame_buffer(self->panel_handle, 1, &fb);
    if (ret != ESP_OK || fb == NULL) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to get DPI framebuffer"));
    }

    self->bufinfo.buf = fb;
    self->bufinfo.len = (color_depth / 8) * width * height;
    self->bufinfo.typecode = 'B';

    mp_printf(&mp_plat_print, "MIPI DSI Display initialized\n");
    return MP_OBJ_FROM_PTR(self);
}

static mp_int_t mipidsi_display_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    *bufinfo = self->bufinfo;
    return 0;
}

static mp_obj_t mipidsi_display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_width) {
            dest[0] = MP_OBJ_NEW_SMALL_INT(self->width);
        } else if (attr == MP_QSTR_height) {
            dest[0] = MP_OBJ_NEW_SMALL_INT(self->height);
        } else if (attr == MP_QSTR_color_depth) {
            dest[0] = MP_OBJ_NEW_SMALL_INT(self->color_depth);
        } else {
            // Continue lookup in locals_dict.
            dest[1] = MP_OBJ_SENTINEL;
        }
    }
    return mp_const_none;
}

static mp_obj_t mipidsi_display_deinit(mp_obj_t self_in) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->panel_handle != NULL) {
        esp_lcd_panel_del(self->panel_handle);
        self->panel_handle = NULL;
    }
    self->bufinfo.buf = NULL;
    self->bufinfo.len = 0;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mipidsi_display_deinit_obj, mipidsi_display_deinit);

static mp_obj_t mipidsi_display_refresh(mp_obj_t self_in) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (self->panel_handle != NULL) {
        // Flush framebuffer to display
        esp_lcd_panel_draw_bitmap(self->panel_handle, 0, 0, self->width, self->height, (const void *)self->bufinfo.buf);
    }

    // Cache writeback for performance
#ifdef CACHE_MAP_L1_DCACHE
    Cache_WriteBack_Addr(CACHE_MAP_L1_DCACHE, (uint32_t)(self->bufinfo.buf), self->bufinfo.len);
#else
    Cache_WriteBack_Addr((uint32_t)(self->bufinfo.buf), self->bufinfo.len);
#endif

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mipidsi_display_refresh_obj, mipidsi_display_refresh);

static const mp_rom_map_elem_t mipidsi_display_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&mipidsi_display_deinit_obj)},
    {MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&mipidsi_display_refresh_obj)},
};
static MP_DEFINE_CONST_DICT(mipidsi_display_locals_dict, mipidsi_display_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mipidsi_display_type,
    MP_QSTR_Display,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS,
    make_new, mipidsi_display_make_new,
    buffer, mipidsi_display_get_buffer,
    attr, mipidsi_display_attr,
    locals_dict, &mipidsi_display_locals_dict);

// ========== Module definition ==========

static const mp_map_elem_t mipidsi_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_mipidsi)},
    {MP_ROM_QSTR(MP_QSTR_Bus), (mp_obj_t)&mipidsi_bus_type},
    {MP_ROM_QSTR(MP_QSTR_Display), (mp_obj_t)&mipidsi_display_type},
};
static MP_DEFINE_CONST_DICT(mp_module_mipidsi_globals, mipidsi_module_globals_table);

const mp_obj_module_t mp_module_mipidsi = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&mp_module_mipidsi_globals,
};
MP_REGISTER_MODULE(MP_QSTR_mipidsi, mp_module_mipidsi);
