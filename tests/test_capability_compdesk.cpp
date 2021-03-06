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

   Unit test to RDP CompDesk object
   Using lib boost functions for testing
*/

#define BOOST_AUTO_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE TestCapabilityCompDesk
#include <boost/test/auto_unit_test.hpp>

#define LOGPRINT
#include "log.hpp"
#include "RDP/capabilities.hpp"


BOOST_AUTO_TEST_CASE(TestCapabilityCompDeskEmit)
{
    CompDeskCaps compdesk_caps;
    compdesk_caps.CompDeskSupportLevel = COMPDESK_SUPPORTED;

    BOOST_CHECK_EQUAL(compdesk_caps.capabilityType, (uint16_t)CAPSETTYPE_TYPE_COMPDESK);
    BOOST_CHECK_EQUAL(compdesk_caps.len, (uint16_t)RDP_CAPLEN_COMPDESK);
    BOOST_CHECK_EQUAL(compdesk_caps.CompDeskSupportLevel, (uint16_t)COMPDESK_SUPPORTED);

    Stream stream(1024);
    compdesk_caps.emit(stream);
    stream.end = stream.p;
    stream.p = stream.data;

    CompDeskCaps compdesk_caps2;

    BOOST_CHECK_EQUAL(compdesk_caps2.capabilityType, (uint16_t)CAPSETTYPE_TYPE_COMPDESK);
    BOOST_CHECK_EQUAL(compdesk_caps2.len, (uint16_t)RDP_CAPLEN_COMPDESK);

    BOOST_CHECK_EQUAL((uint16_t)CAPSETTYPE_TYPE_COMPDESK, stream.in_uint16_le());
    BOOST_CHECK_EQUAL((uint16_t)RDP_CAPLEN_COMPDESK, stream.in_uint16_le());
    compdesk_caps2.recv(stream, RDP_CAPLEN_COMPDESK);

    BOOST_CHECK_EQUAL(compdesk_caps2.CompDeskSupportLevel, (uint16_t)COMPDESK_SUPPORTED);
}
