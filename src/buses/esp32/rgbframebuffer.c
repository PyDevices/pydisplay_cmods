/*
 * SPDX-FileCopyrightText: 2024 Brad Barnett
 *
 * SPDX-License-Identifier: MIT
 */


#include <stdio.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io.h"
#include "esp_err.h"
#include "esp_log.h"
#include "rom/cache.h"

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/objarray.h"
#include "py/binary.h"


typedef struct _rgbframebuffer_obj_t {
    mp_obj_base_t base;                     // base class
    esp_lcd_panel_handle_t panel_handle;    // panel handle
    uint16_t width;                         // width of the framebuffer
    uint16_t height;                        // height of the framebuffer
    mp_buffer_info_t bufinfo;
} rgbframebuffer_obj_t;

extern const mp_obj_type_t rgbframebuffer_type;

static mp_obj_t rgbframebuffer_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_de, ARG_vsync, ARG_hsync, ARG_dclk, ARG_red, ARG_green, ARG_blue, ARG_frequency, ARG_width, ARG_height, ARG_hsync_pulse_width, ARG_hsync_front_porch, ARG_hsync_back_porch,
    ARG_vsync_pulse_width, ARG_vsync_front_porch, ARG_vsync_back_porch, ARG_hsync_idle_low, ARG_vsync_idle_low,
    ARG_de_idle_high, ARG_pclk_active_high, ARG_pclk_idle_high};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_de, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_vsync, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_hsync, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_dclk, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_red, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_green, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_blue, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_frequency, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_hsync_pulse_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_hsync_front_porch, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_hsync_back_porch, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_vsync_pulse_width, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_vsync_front_porch, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_vsync_back_porch, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_hsync_idle_low, MP_ARG_REQUIRED | MP_ARG_BOOL },
        { MP_QSTR_vsync_idle_low, MP_ARG_REQUIRED | MP_ARG_BOOL },
        { MP_QSTR_de_idle_high, MP_ARG_REQUIRED | MP_ARG_BOOL },
        { MP_QSTR_pclk_active_high, MP_ARG_REQUIRED | MP_ARG_BOOL },
        { MP_QSTR_pclk_idle_high, MP_ARG_REQUIRED | MP_ARG_BOOL },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rgbframebuffer_obj_t *self = m_new_obj(rgbframebuffer_obj_t);
    self->base.type = &rgbframebuffer_type;
    self->width = args[ARG_width].u_int;
    self->height = args[ARG_height].u_int;
    esp_err_t ret;

    mp_printf(&mp_plat_print, "RGB Framebuffer initializing...\n");
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = args[ARG_frequency].u_int,
            .h_res = args[ARG_width].u_int,
            .v_res = args[ARG_height].u_int,
            .hsync_pulse_width = args[ARG_hsync_pulse_width].u_int,
            .hsync_front_porch = args[ARG_hsync_front_porch].u_int,
            .hsync_back_porch = args[ARG_hsync_back_porch].u_int,
            .vsync_pulse_width = args[ARG_vsync_pulse_width].u_int,
            .vsync_front_porch = args[ARG_vsync_front_porch].u_int,
            .vsync_back_porch = args[ARG_vsync_back_porch].u_int,
            .flags = {
                .hsync_idle_low = args[ARG_hsync_idle_low].u_bool,
                .vsync_idle_low = args[ARG_vsync_idle_low].u_bool,
                .de_idle_high = args[ARG_de_idle_high].u_bool,
                .pclk_active_neg = !args[ARG_pclk_active_high].u_bool,
                .pclk_idle_high = args[ARG_pclk_idle_high].u_bool,
            },
        },
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = 0,
        .sram_trans_align = 8,
        .psram_trans_align = 64,
        .hsync_gpio_num = args[ARG_hsync].u_int,
        .vsync_gpio_num = args[ARG_vsync].u_int,
        .de_gpio_num = args[ARG_de].u_int,
        .pclk_gpio_num = args[ARG_dclk].u_int,
        .disp_gpio_num = -1,
        .flags = {
            .disp_active_low = false,
            .refresh_on_demand = false,
            .fb_in_psram = true,
            .double_fb = false,
            .no_fb = false,
            .bb_invalidate_cache = false,
        },
    };

    mp_obj_tuple_t *blue = MP_OBJ_TO_PTR(args[ARG_blue].u_obj);
    mp_obj_tuple_t *green = MP_OBJ_TO_PTR(args[ARG_green].u_obj);
    mp_obj_tuple_t *red = MP_OBJ_TO_PTR(args[ARG_red].u_obj);
    if (blue->len != 5 || green->len != 6 || red->len != 5) {
        mp_raise_ValueError(MP_ERROR_TEXT("Expected blue, green, and red to have lengths 5, 6, and 5 respectively"));
    }

    int idx = 0;
    for (int i = 0; i < blue->len; i++) {
        panel_config.data_gpio_nums[idx++] = mp_obj_get_int(blue->items[i]);
    }

    for (int i = 0; i < green->len; i++) {
        panel_config.data_gpio_nums[idx++] = mp_obj_get_int(green->items[i]);
    }

    for (int i = 0; i < red->len; i++) {
        panel_config.data_gpio_nums[idx++] = mp_obj_get_int(red->items[i]);
    }
    panel_config.data_width = 16;

    ret = esp_lcd_new_rgb_panel(&panel_config, &self->panel_handle);
    if (ret != 0) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Failed to initialize RGB LCD panel"));
    }

    ret = esp_lcd_panel_reset(self->panel_handle);
    if (ret != 0) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Failed to reset RGB LCD panel"));
    }

    ret = esp_lcd_panel_init(self->panel_handle);
    if (ret != 0) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Failed to initialize RGB LCD panel"));
    }

    uint16_t color = 0xffff;
    ret = esp_lcd_panel_draw_bitmap(self->panel_handle, 0, 0, 1, 1, &color);
    if (ret != 0) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Failed to draw bitmap on RGB LCD panel"));
    }

    void *buf;
    ret = esp_lcd_rgb_panel_get_frame_buffer(self->panel_handle, 1, &buf);
    if (ret != 0) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Failed to get framebuffer from RGB LCD panel"));
    }
    self->bufinfo.buf = (uint8_t *)buf;
    self->bufinfo.len = 2 * (panel_config.timings.h_res * panel_config.timings.v_res);
    self->bufinfo.typecode = 'B';

    mp_printf(&mp_plat_print, "RGB Framebuffer initialized\n");

    return MP_OBJ_FROM_PTR(self);
}

static mp_int_t rgbframebuffer_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    *bufinfo = self->bufinfo;
    return 0;
}

static mp_obj_t rgbframebuffer_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_width) {
            dest[0] = MP_OBJ_NEW_SMALL_INT(self->width);
        } else if (attr == MP_QSTR_height) {
            dest[0] = MP_OBJ_NEW_SMALL_INT(self->height);
        } else {
            // Continue lookup in locals_dict.
            dest[1] = MP_OBJ_SENTINEL;
        }
    }
    return mp_const_none;
}

static mp_obj_t rgbframebuffer_refresh(mp_obj_t self_in){
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
#ifdef CACHE_MAP_L1_DCACHE
    Cache_WriteBack_Addr(CACHE_MAP_L1_DCACHE, (uint32_t)(self->bufinfo.buf), self->bufinfo.len);
#else
    Cache_WriteBack_Addr((uint32_t)(self->bufinfo.buf), self->bufinfo.len);
#endif
    // call a done callback if desired
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(rgbframebuffer_refresh_obj, rgbframebuffer_refresh);


static const mp_rom_map_elem_t rgbframebuffer_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&rgbframebuffer_refresh_obj)},
};
static MP_DEFINE_CONST_DICT(rgbframebuffer_locals_dict, rgbframebuffer_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    rgbframebuffer_type,
    MP_QSTR_RGBFrameBuffer,
    MP_TYPE_FLAG_HAS_SPECIAL_ACCESSORS,
    make_new, rgbframebuffer_make_new,
    buffer, rgbframebuffer_get_buffer,
    attr, rgbframebuffer_attr,
    locals_dict, &rgbframebuffer_locals_dict);


static const mp_map_elem_t rgbframebuffer_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_rgbframebuffer)},
    {MP_ROM_QSTR(MP_QSTR_RGBFrameBuffer), (mp_obj_t)&rgbframebuffer_type},
};
static MP_DEFINE_CONST_DICT(mp_module_rgbframebuffer_globals, rgbframebuffer_module_globals_table);

const mp_obj_module_t mp_module_rgbframebuffer = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&mp_module_rgbframebuffer_globals,
};
MP_REGISTER_MODULE(MP_QSTR_rgbframebuffer, mp_module_rgbframebuffer);
