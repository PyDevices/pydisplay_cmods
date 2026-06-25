/*
 * SPDX-FileCopyrightText: 2024 Brad Barnett
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/objexcept.h"
#include "shared/runtime/pyexec.h"

#include "common.h"

bool color_trans_done(void *panel_io, void *edata, void *user_ctx) {
    bus_obj_t *self = (bus_obj_t *)user_ctx;
    self->trans_done = true;
    return false;
}

mp_obj_t send(size_t n_args, const mp_obj_t *args) {
    bus_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    int cmd = mp_obj_get_int(args[1]);
    void *buf = NULL;
    int len = 0;
    if (n_args > 2 && args[2] != mp_const_none) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);
        buf = bufinfo.buf;
        len = bufinfo.len;
    }

    int ret = self->tx_param(self->io_handle, cmd, buf, len);
    if (ret != 0) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to send data"));
    }
    return mp_const_none;
}

mp_obj_t send_color(size_t n_args, const mp_obj_t *args) {    
    bus_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    int cmd = mp_obj_get_int(args[1]);
    void *buf = NULL;
    int len = 0;
    if (n_args > 2 && args[2] != mp_const_none) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[2], &bufinfo, MP_BUFFER_READ);
        buf = bufinfo.buf;
        len = bufinfo.len;
    }

    while (self->trans_done == false) {
        mp_handle_pending(true);
    }
    self->trans_done = false;
    int ret = self->tx_color(self->io_handle, cmd, buf, len);
    if (ret != 0) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("Failed to send color data"));
    }
    while (self->trans_done == false) {
        mp_handle_pending(true);
    }

    return mp_const_none;
}
