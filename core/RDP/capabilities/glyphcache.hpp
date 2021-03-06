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
   Copyright (C) Wallix 2011
   Author(s): Christophe Grosjean

   RDP Capabilities :

*/

#if !defined(__RDP_CAPABILITIES_GLYPHCACHE_HPP__)
#define __RDP_CAPABILITIES_GLYPHCACHE_HPP__


// 2.2.7.1.8 Glyph Cache Capability Set (TS_GLYPHCACHE_CAPABILITYSET)
// ==================================================================

// The TS_GLYPHCACHE_CAPABILITYSET structure advertises the glyph support level
// and associated cache sizes. This capability is only sent from client to
// server.

// capabilitySetType (2 bytes): A 16-bit, unsigned integer. The type of the
// capability set. This field MUST be set to CAPSTYPE_GLYPHCACHE (16).

// lengthCapability (2 bytes): A 16-bit, unsigned integer. The length in bytes
//    of the capability data, including the size of the capabilitySetType and
//    lengthCapability fields.

// GlyphCache (40 bytes): An array of 10 TS_CACHE_DEFINITION structures. An
//    ordered specification of the layout of each of the glyph caches with IDs 0
//    through to 9 ([MS-RDPEGDI] section 3.1.1.1.2).

// FragCache (4 bytes): Fragment cache data. The maximum number of entries
//    allowed in the cache is 256, and the largest allowed maximum size of an
//    element is 256 bytes.

// GlyphSupportLevel (2 bytes): A 16-bit, unsigned integer. The level of glyph
//    support.
//    +-------------------------------+-------------------------------------------+
//    | 0x0000 GLYPH_SUPPORT_NONE     | The client does not support glyph caching.|
//    |                               | All text output will be sent to the client|
//    |                               | as expensive Bitmap Updates (see sections |
//    |                               | 2.2.9.1.1.3.1.2 and 2.2.9.1.2.1.2).       |
//    +-------------------------------+-------------------------------------------+
//    | 0x0001 GLYPH_SUPPORT_PARTIAL  | Indicates support for Revision 1 Cache    |
//    |                               | Glyph Secondary Drawing Orders (see       |
//    |                               | [MS-RDPEGDI] section 2.2.2.2.1.2.5).      |
//    +-------------------------------+-------------------------------------------+
//    | 0x0002 GLYPH_SUPPORT_FULL     | Indicates support for Revision 1 Cache    |
//    |                               | Glyph Secondary Drawing Orders (see       |
//    |                               | [MS-RDPEGDI] section 2.2.2.2.1.2.5).      |
//    +-------------------------------+-------------------------------------------+
//    | 0x0003 GLYPH_SUPPORT_ENCODE   | Indicates support for Revision 2 Cache    |
//    |                               | Glyph Secondary Drawing Orders (see       |
//    |                               | [MS-RDPEGDI] section 2.2.2.2.1.2.6).      |
//    +-------------------------------+-------------------------------------------+

//    If the GlyphSupportLevel is greater than GLYPH_SUPPORT_NONE (0), the client
//    MUST support the GlyphIndex Primary Drawing Order (see [MS-RDPEGDI] section
//    2.2.2.2.1.1.2.13) or the FastIndex Primary Drawing Order (see [MS-RDPEGDI]
//    section 2.2.2.2.1.1.2.14). If the FastIndex Primary Drawing Order is not
//    supported, then support for the GlyphIndex Primary Drawing Order is assumed
//    by the server (order support is specified in the Order Capability Set, as
//    described in section 2.2.7.1.3).

// pad2octets (2 bytes): A 16-bit, unsigned integer. Padding. Values in this
//    field MUST be ignored.

enum {
       GLYPH_SUPPORT_NONE
     , GLYPH_SUPPORT_PARTIAL
     , GLYPH_SUPPORT_FULL
     , GLYPH_SUPPORT_ENCODE
};

struct GlyphSupportCaps : public Capability {
    uint8_t glyphCache[40];
    uint32_t fragCache;
    uint16_t glyphSupportLevel;
    uint16_t pad2octets;

    GlyphSupportCaps()
    : Capability(CAPSTYPE_GLYPHCACHE, RDP_CAPLEN_GLYPHCACHE)
    , fragCache(0x01000100) // max number of entries in the cache = 256
                            // largest allowed maximum size of an element in (bytes) = 256
    , glyphSupportLevel(GLYPH_SUPPORT_NONE) // By default, no support
    , pad2octets(0x0000)
    {
        const uint8_t init_glyphCache[] = {
                                            0xFE, 0x00, 0x04, 0x00, 0xFE, 0x00, 0x04, 0x00,
                                            0xFE, 0x00, 0x08, 0x00, 0xFE, 0x00, 0x08, 0x00,
                                            0xFE, 0x00, 0x10, 0x00, 0xFE, 0x00, 0x20, 0x00,
                                            0xFE, 0x00, 0x40, 0x00, 0xFE, 0x00, 0x80, 0x00,
                                            0xFE, 0x00, 0x00, 0x01, 0x40, 0x00, 0x00, 0x08
                                          };

        for(size_t i = 0; i < sizeof(init_glyphCache); i++) {
            this->glyphCache[i] = init_glyphCache[i];
        }
    }

    void emit(Stream & stream){
        stream.out_uint16_le(this->capabilityType);
        stream.out_uint16_le(this->len);
        stream.out_copy_bytes(this->glyphCache, 40);
        stream.out_uint32_le(this->fragCache);
        stream.out_uint16_le(this->glyphSupportLevel);
        stream.out_uint16_le(this->pad2octets);
    }

    void recv(Stream & stream, uint16_t len){
        this->len = len;
        stream.in_copy_bytes(this->glyphCache, 40);
        this->fragCache = stream.in_uint32_le();
        this->glyphSupportLevel = stream.in_uint16_le();
        this->pad2octets = stream.in_uint16_le();
    }

    void log(const char * msg){
        LOG(LOG_INFO, "%s GlyphCache caps (%u bytes)", msg, this->len);
        LOG(LOG_INFO, "GlyphCache caps::glyphCache %u", this->glyphCache);
        LOG(LOG_INFO, "GlyphCache caps::fragCache %u", this->fragCache);
        LOG(LOG_INFO, "GlyphCache caps::glyphSupportLevel %u", this->glyphSupportLevel);
        LOG(LOG_INFO, "GlyphCache caps::pad2octets %u", this->pad2octets);
    }
};

#endif
