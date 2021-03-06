/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Product name: redemption, a FLOSS RDP proxy
   Copyright (C) Wallix 2010
   Author(s): Christophe Grosjean, Javier Caverni
   Based on xrdp Copyright (C) Jay Sorg 2004-2010

   xup module main header file

*/

#if !defined(__XUP_HPP__)
#define __XUP_HPP__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <errno.h>

/* include other h files */
#include "stream.hpp"

struct xup_mod : public client_mod {

enum {
    XUPWM_PAINT        = 3,
    XUPWM_KEYDOWN      = 15,
    XUPWM_KEYUP        = 16,
    XUPWM_SYNCHRONIZE  = 17,
    XUPWM_MOUSEMOVE    = 100,
    XUPWM_LBUTTONUP    = 101,
    XUPWM_LBUTTONDOWN  = 102,
    XUPWM_RBUTTONUP    = 103,
    XUPWM_RBUTTONDOWN  = 104,
    XUPWM_BUTTON3UP    = 105,
    XUPWM_BUTTON3DOWN  = 106,
    XUPWM_BUTTON4UP    = 107,
    XUPWM_BUTTON4DOWN  = 108,
    XUPWM_BUTTON5UP    = 109,
    XUPWM_BUTTON5DOWN  = 110,
    XUPWM_BUTTON_OK    = 300,
    XUPWM_SCREENUPDATE = 0x4444,
    XUPWM_CHANNELDATA  = 0x5555,
};

    /* mod data */
    int width;
    int height;
    int bpp;
    Transport *t;
    int rop;
    int fgcolor;
    BGRPalette palette332;

    xup_mod(Transport * t, struct ModContext & context, struct FrontAPI & front, uint16_t front_width, uint16_t front_height)
        : client_mod(front, front_width, front_height)
    {
        this->width = atoi(context.get(STRAUTHID_OPT_WIDTH));
        this->height = atoi(context.get(STRAUTHID_OPT_HEIGHT));
        this->bpp = atoi(context.get(STRAUTHID_OPT_BPP));
        this->rop = 0xCC;
        init_palette332(this->palette332);

        try {
            this->t = t;
            Stream stream(32768);
            uint8_t * hdr = stream.p;
            stream.p += 4;
            stream.out_uint16_le(103);
            stream.out_uint32_le(200);
            /* x and y */
            int xy = 0;
            stream.out_uint32_le(xy);
            /* width and height */
            int cxcy = ((this->width & 0xffff) << 16) | this->height;
            stream.out_uint32_le(cxcy);
            stream.out_uint32_le(0);
            stream.out_uint32_le(0);
            stream.mark_end();
            int len = (int)(stream.end - stream.data);
            stream.p = hdr;
            stream.out_uint32_le(len);
            this->t->send(stream.data, len);

        }
        catch(...){
            delete this->t;
            throw;
        }
    }

    virtual ~xup_mod()
    {
        delete this->t;
    }

    enum {
        XUPWM_INVALIDATE = 200,
    };

    virtual void rdp_input_mouse(int device_flags, int x, int y, Keymap2 * keymap)
    {
        LOG(LOG_INFO, "input mouse");

        if (device_flags & MOUSE_FLAG_MOVE) { /* 0x0800 */
            this->x_input_event(XUPWM_MOUSEMOVE, x, y, 0, 0);
        }
        if (device_flags & MOUSE_FLAG_BUTTON1) { /* 0x1000 */
            this->x_input_event(
                XUPWM_LBUTTONUP + ((device_flags & MOUSE_FLAG_DOWN) >> 15),
                x, y, 0, 0);
        }
        if (device_flags & MOUSE_FLAG_BUTTON2) { /* 0x2000 */
            this->x_input_event(
                XUPWM_RBUTTONUP + ((device_flags & MOUSE_FLAG_DOWN) >> 15),
                x, y, 0, 0);
        }
        if (device_flags & MOUSE_FLAG_BUTTON3) { /* 0x4000 */
            this->x_input_event(
                XUPWM_BUTTON3UP + ((device_flags & MOUSE_FLAG_DOWN) >> 15),
                x, y, 0, 0);
        }
        if (device_flags == MOUSE_FLAG_BUTTON4 || /* 0x0280 */ device_flags == 0x0278) {
            this->x_input_event(XUPWM_BUTTON4DOWN, x, y, 0, 0);
            this->x_input_event(XUPWM_BUTTON4UP, x, y, 0, 0);
        }
        if (device_flags == MOUSE_FLAG_BUTTON5 || /* 0x0380 */ device_flags == 0x0388) {
            this->x_input_event(XUPWM_BUTTON5DOWN, x, y, 0, 0);
            this->x_input_event(XUPWM_BUTTON5UP, x, y, 0, 0);
        }
    }

    virtual void rdp_input_scancode(long param1, long param2, long device_flags, long param4, Keymap2 * keymap){
        LOG(LOG_INFO, "scan code");
        /*
        if (ki != 0) {
            int msg = (device_flags & KBD_FLAG_UP)?XUPWM_KEYUP:XUPWM_KEYDOWN;
            this->x_input_event(msg, ki->chr, ki->sym, param1, device_flags);
        }
        */
    }

    virtual void rdp_input_synchronize(uint32_t time, uint16_t device_flags, int16_t param1, int16_t param2)
    {
        LOG(LOG_INFO, "overloaded by subclasses");
        return;
    }


    virtual void rdp_input_invalidate(const Rect & r)
    {
        LOG(LOG_INFO, "rdp_input_invalidate");
        if (!r.isempty()) {
            this->x_input_event(XUPWM_INVALIDATE,
                ((r.x & 0xffff) << 16) | (r.y & 0xffff),
                ((r.cx & 0xffff) << 16) | (r.cy & 0xffff),
                0, 0);
        }
    }

    void x_input_event(const int msg, const long param1, const long param2, const long param3, const long param4)
    {
        Stream stream(32768);
        stream.out_uint32_le(0); // skip yet unknown len
        stream.out_uint16_le(103);
        stream.out_uint32_le(msg);
        stream.out_uint32_le(param1);
        stream.out_uint32_le(param2);
        stream.out_uint32_le(param3);
        stream.out_uint32_le(param4);
        uint32_t len = stream.get_offset(0);
        stream.set_out_uint32_le(len, 0);
        this->t->send(stream.data, len);
    }

    virtual BackEvent_t draw_event(void)
    {
        BackEvent_t rv = BACK_EVENT_NONE;

        try{
            Stream stream(32768);
            this->t->recv((char**)&stream.end, 8);
            int type = stream.in_uint16_le();
            int num_orders = stream.in_uint16_le();
            int len = stream.in_uint32_le();
            if (type == 1) {
                stream.init(len);
                this->t->recv((char**)&stream.end, len);

                for (int index = 0; index < num_orders; index++) {
                    type = stream.in_uint16_le();
                    switch (type) {
                    case 1:
                        this->front.begin_update();
                        break;
                    case 2:
                        this->front.end_update();
                        break;
                    case 3:
                    {
                        const Rect r(
                            stream.in_sint16_le(),
                            stream.in_sint16_le(),
                            stream.in_uint16_le(),
                            stream.in_uint16_le());
                         this->front.draw(RDPPatBlt(r, this->rop, BLACK, WHITE,
                            RDPBrush(r.x, r.y, 3, 0xaa, (const uint8_t *)"\xaa\x55\xaa\x55\xaa\x55\xaa\x55")
                            ), r);
                    }
                    break;
                    case 4:
                    {
                        const Rect r(
                            stream.in_sint16_le(),
                            stream.in_sint16_le(),
                            stream.in_uint16_le(),
                            stream.in_uint16_le());
                        const int srcx = stream.in_sint16_le();
                        const int srcy = stream.in_sint16_le();
                        const RDPScrBlt scrblt(r, 0xCC, srcx, srcy);
                        this->front.draw(scrblt, r);
                    }
                    break;
                    case 5:
                    {
                        const Rect r(
                            stream.in_sint16_le(),
                            stream.in_sint16_le(),
                            stream.in_uint16_le(),
                            stream.in_uint16_le());
                        int len_bmpdata = stream.in_uint32_le();
                        const uint8_t * bmpdata = stream.in_uint8p(len_bmpdata);
                        int width = stream.in_uint16_le();
                        int height = stream.in_uint16_le();
                        int srcx = stream.in_sint16_le();
                        int srcy = stream.in_sint16_le();
                        Bitmap bmp(bpp, &this->palette332, width, height, bmpdata, sizeof(bmpdata));
                        this->front.draw(RDPMemBlt(0, r, 0xCC, srcx, srcy, 0), r, bmp);
                    }
                    break;
                    case 10: /* server_set_clip */
                    {
                        const Rect r(
                            stream.in_sint16_le(),
                            stream.in_sint16_le(),
                            stream.in_uint16_le(),
                            stream.in_uint16_le());
                          TODO(" see clip management")
//                        this->server_set_clip(r);
                    }
                    break;
                    case 11: /* server_reset_clip */
                          TODO(" see clip management")
//                        this->server_reset_clip();
                    break;
                    case 12: /* server_set_fgcolor */
                    {
                        this->fgcolor = stream.in_uint32_le();
                    }
                    break;
                    case 14:
                        this->rop = stream.in_uint16_le();
                    break;
                    case 17:
                    {
                        int style = stream.in_uint16_le();
                        int width = stream.in_uint16_le();
                        this->server_set_pen(style, width);
                    }
                    break;
                    case 18:
                    {
                        int x1 = stream.in_sint16_le();
                        int y1 = stream.in_sint16_le();
                        int x2 = stream.in_sint16_le();
                        int y2 = stream.in_sint16_le();
                        const RDPLineTo lineto(1, x1, y1, x2, y2, WHITE,
                                               this->rop,
                                               RDPPen(this->pen.style, this->pen.width, this->fgcolor));
                        this->front.draw(lineto, Rect(0,0,1,1));
                    }
                    break;
                    case 19:
                    {
                        int x = stream.in_sint16_le();
                        int y = stream.in_sint16_le();
                        TODO(" copy seems useless here")
                        uint8_t cur_data[32 * (32 * 3)];
                        uint8_t cur_mask[32 * (32 / 8)];
                        memcpy(cur_data, stream.in_uint8p(32 * (32 * 3)), 32 * (32 * 3));
                        memcpy(cur_mask, stream.in_uint8p(32 * (32 / 8)), 32 * (32 / 8));
                        this->front.server_set_pointer(x, y, cur_data, cur_mask);
                    }
                    break;
                    default:
                        throw 1;
                    }
                    if (rv != BACK_EVENT_NONE) {
                        break;
                    }
                }
            }
        }
        catch(...){
            rv = BACK_EVENT_1;
        }
        return rv;
    }

};

#endif
