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
   Author(s): Christophe Grosjean

   Unit test to RDP Orders coder/decoder
   Using lib boost functions for testing
*/


#define BOOST_AUTO_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE TestOrderDestBlt
#include <boost/test/auto_unit_test.hpp>
#include <algorithm>

#include "RDP/orders/RDPOrdersCommon.hpp"
#include "RDP/orders/RDPOrdersPrimaryHeader.hpp"
#include "RDP/orders/RDPOrdersPrimaryDestBlt.hpp"

#include "./test_orders.hpp"

BOOST_AUTO_TEST_CASE(TestDestBlt)
{
    using namespace RDP;

    {
        Stream stream(1000);

        // DESTBLT = 0, hence we won't have order change
        RDPOrderCommon state_common(0, Rect(311, 0, 800, 600));
        RDPDestBlt state_destblt(Rect(), 0);

        RDPOrderCommon newcommon(DESTBLT, Rect(311, 0, 800, 600));
        RDPDestBlt(Rect(300, 400, 50, 60), 0xFF).emit(stream, newcommon, state_common, state_destblt);

        uint8_t datas[11] = {
            STANDARD | BOUNDS | LASTBOUNDS,
            0x1F,        // x, y, cx, cy, rop changed
            0x2C, 0x01,  // x = 300
            0x90, 0x01,  // y = 400
            0x32, 0x00,  // cx = 50
            0x3c, 0x00,  // cy = 60
            0xFF,        // rop
        };
        check_datas(stream.p-stream.data, stream.data, 11, datas, "DestBlt 1");

        stream.end = stream.p; stream.p = stream.data;

        RDPOrderCommon common_cmd = state_common;
        uint8_t control = stream.in_uint8();
        BOOST_CHECK_EQUAL(true, !!(control & STANDARD));
        RDPPrimaryOrderHeader header = common_cmd.receive(stream, control);

        BOOST_CHECK_EQUAL((uint8_t)DESTBLT, common_cmd.order);

        RDPDestBlt cmd(Rect(), 0);

        cmd.receive(stream, header);

        check<RDPDestBlt>(common_cmd, cmd,
            RDPOrderCommon(DESTBLT, Rect(311, 0, 800, 600)),
            RDPDestBlt(Rect(300, 400, 50, 60), 0xFF),
            "DestBlt 1");
    }

}
