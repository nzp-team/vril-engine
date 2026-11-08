/*
 * Copyright (C) 2026 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "../../nzportable_def.h"
#include "xbox_local.h"
#include <windows.h>
#include <usbh_lib.h>
#include <xid_driver.h>
#include <stddef.h>

typedef struct {
    xid_dev_t *    device;
    xid_gamepad_in report;
    qboolean       reading;
} xbox_pad_t;

static xbox_pad_t pads[CONFIG_XID_MAX_DEV];
static xbox_pad_t * active_pad;
static xid_gamepad_in pad;
static unsigned short old_ui_dpad;
static unsigned short rumble_low, rumble_high, sent_low, sent_high;
static double rumble_stop_time;
static qboolean rumble_pending, usb_initialized;
extern cvar_t in_rumble;

static const struct {
    int key, pressure;
} KeyTable[] = {
    { K_BOTTOMFACE, offsetof(xid_gamepad_in, a)     },
    { K_RIGHTFACE,  offsetof(xid_gamepad_in, b)     },
    { K_LEFTFACE,   offsetof(xid_gamepad_in, x)     },
    { K_TOPFACE,    offsetof(xid_gamepad_in, y)     },
    { K_LTRIGGER,   offsetof(xid_gamepad_in, white) },
    { K_RTRIGGER,   offsetof(xid_gamepad_in, black) },
    { K_ZLTRIGGER,  offsetof(xid_gamepad_in, l)     },
    { K_ZRTRIGGER,  offsetof(xid_gamepad_in, r)     }
};

qboolean IN_PlatformHasMouse(void){ return false; }

qboolean IN_PlatformHasGamepad(void){ return true; }

void IN_SetMouseToRelative(bool relative){ (void) relative; }

void IN_PlatformMouseMove(usercmd_t * cmd){ (void) cmd; }

void IN_PlatformMove(usercmd_t * cmd){ (void) cmd; }

void IN_PlatformClearPendingInput(void){ old_ui_dpad = 0; }

static void
IN_XboxRead(UTR_T * transfer)
{
    xid_dev_t * device = transfer->context;
    xbox_pad_t * slot  = device ? device->user_data : NULL;

    if (!slot)
        return;

    if (transfer->status < 0) {
        slot->reading = false;
        memset(&slot->report, 0, sizeof(slot->report));
        return;
    }
    if (transfer->xfer_len >= sizeof(slot->report))
        memcpy(&slot->report, transfer->buff, sizeof(slot->report));
    transfer->xfer_len        = 0;
    transfer->bIsTransferDone = 0;
    if (usbh_int_xfer(transfer) != USBH_OK)
        slot->reading = false;
}

static void
IN_XboxConnected(xid_dev_t * device, int status)
{
    int i;

    (void) status;

    if (device->xid_desc.bType != XID_TYPE_GAMECONTROLLER)
        return;

    for (i = 0; i < CONFIG_XID_MAX_DEV; ++i) {
        if (pads[i].device)
            continue;
        pads[i].device    = device;
        device->user_data = &pads[i];
        return;
    }
}

static void
IN_XboxDisconnected(xid_dev_t * device, int status)
{
    xbox_pad_t * slot = device->user_data;

    (void) status;

    if (!slot)
        return;

    if (active_pad == slot) {
        active_pad       = NULL;
        rumble_low       = rumble_high = sent_low = sent_high = 0;
        rumble_stop_time = 0;
        rumble_pending   = false;
    }
    memset(slot, 0, sizeof(*slot));
    device->user_data = NULL;
}

void
IN_PlatformInit(void)
{
    extern cvar_t cl_controllerglyphs;

    Cvar_Set("cl_controllerglyphs", "xbox");
    cl_controllerglyphs.flags |= CVAR_ROM;
    usbh_core_init();
    usbh_xid_init();
    usbh_install_xid_conn_callback(IN_XboxConnected, IN_XboxDisconnected);
    usb_initialized = true;
}

void
IN_PlatformShutdown(void)
{
    int i;

    if (!usb_initialized)
        return;

    for (i = 0; i < CONFIG_XID_MAX_DEV; ++i)
        if (pads[i].device)
            usbh_xid_rumble(pads[i].device, 0, 0);
    usbh_install_xid_conn_callback(NULL, NULL);
    usbh_core_deinit();
    memset(pads, 0, sizeof(pads));
    memset(&pad, 0, sizeof(pad));
    active_pad      = NULL;
    usb_initialized = false;
}

void
IN_XboxPoll(void)
{
    static const struct { unsigned short mask;
                          int            key;
    } buttons[] = {
        { 0x10, K_START  }, { 0x20, K_SELECT },
        { 0x40, K_LTHUMB }, { 0x80, K_RTHUMB }
    };
    static const struct { unsigned short mask;
                          int            game_key, ui_key;
    } dpad[] = {
        { 1, K_UPARROW,   K_DPAD_UP   }, { 2, K_DOWNARROW,  K_DPAD_DOWN  },
        { 4, K_LEFTARROW, K_DPAD_LEFT }, { 8, K_RIGHTARROW, K_DPAD_RIGHT }
    };
    unsigned short game_dpad, ui_dpad;
    KIRQL irql;
    size_t i;

    if (!usb_initialized)
        return;

    usbh_pooling_hubs();
    for (i = 0; i < CONFIG_XID_MAX_DEV; ++i) {
        xbox_pad_t * slot = &pads[i];
        if (!slot->device)
            continue;
        if (!active_pad)
            active_pad = slot;
        if (!slot->reading) {
            // Completion can interrupt the main thread as soon as the read is queued.
            slot->reading = true;
            if (usbh_xid_read(slot->device, 0, IN_XboxRead) != USBH_OK)
                slot->reading = false;
        }
    }
    irql = KeRaiseIrqlToDpcLevel();
    if (active_pad)
        pad = active_pad->report;
    else
        memset(&pad, 0, sizeof(pad));
    KfLowerIrql(irql);

    for (i = 0; i < sizeof(KeyTable) / sizeof(KeyTable[0]); ++i) {
        int pressure = ((const byte *) &pad)[KeyTable[i].pressure];
        // Face buttons need a lighter activation point than the analog triggers.
        Key_PressureEvent(KeyTable[i].key, pressure > (i < 6 ? 20 : 32), pressure);
    }
    for (i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
        Key_PressureEvent(buttons[i].key, (pad.dButtons & buttons[i].mask) != 0, -1);
    game_dpad = key_dest == key_game ? pad.dButtons : 0;
    ui_dpad   = key_dest == key_game ? 0 : pad.dButtons;
    for (i = 0; i < sizeof(dpad) / sizeof(dpad[0]); ++i) {
        Key_PressureEvent(dpad[i].game_key, (game_dpad & dpad[i].mask) != 0, -1);
        if ((ui_dpad ^ old_ui_dpad) & dpad[i].mask)
            Key_Event(dpad[i].ui_key, (ui_dpad & dpad[i].mask) != 0);
    }
    old_ui_dpad = ui_dpad;
} /* IN_XboxPoll */

void
IN_PlatformCommands(void)
{
    if (!active_pad)
        return;

    if (!in_rumble.value || (rumble_stop_time && Sys_FloatTime() >= rumble_stop_time)) {
        rumble_low       = rumble_high = 0;
        rumble_stop_time = 0;
        rumble_pending   = sent_low || sent_high;
    }
    if (rumble_pending && usbh_xid_rumble(active_pad->device, rumble_low, rumble_high) == USBH_OK) {
        sent_low       = rumble_low;
        sent_high      = rumble_high;
        rumble_pending = false;
    }
}

void
IN_PlatformRumble(unsigned short low_frequency, unsigned short high_frequency, unsigned int duration)
{
    // Legacy gameplay effects use 1000-2000, below the Duke motors' useful range.
    rumble_low       = min(65535u, (unsigned) low_frequency * 16u);
    rumble_high      = min(65535u, (unsigned) high_frequency * 16u);
    rumble_stop_time = duration ? Sys_FloatTime() + duration / 1000.0 : 0;
    rumble_pending   = true;
    IN_PlatformCommands();
}

static float
IN_XboxAxis(short value)
{
    return value < 0 ? value / 32768.0f : value / 32767.0f;
}

void
IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t * value)
{
    if (stick == IN_STICK_LEFT) {
        value->x = IN_XboxAxis(pad.leftStickX);
        value->y = IN_XboxAxis(pad.leftStickY);
    } else {
        value->x = IN_XboxAxis(pad.rightStickX);
        value->y = IN_XboxAxis(pad.rightStickY);
    }
}
