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
   Copyright (C) Wallix 2012
   Author(s): Christophe Grosjean

   Class for GCC Client Cluster Data

*/

#if !defined(__CORE_RDP_GCC_CONFERENCE_USER_DATA_CS_CLUSTER_HPP__)
#define __CORE_RDP_GCC_CONFERENCE_USER_DATA_CS_CLUSTER_HPP__


// 2.2.1.3.5 Client Cluster Data (TS_UD_CS_CLUSTER)
// ------------------------------------------------
// The TS_UD_CS_CLUSTER data block is sent by the client to the server either to advertise that it can
// support the Server Redirection PDUs (sections 2.2.13.2 and 2.2.13.3) or to request a connection to
// a given session identifier.

// header (4 bytes): GCC user data block header, as specified in User Data
//                   Header (section 2.2.1.3.1). The User Data Header type
//                   field MUST be set to CS_CLUSTER (0xC004).

// Flags (4 bytes): A 32-bit, unsigned integer. Cluster information flags.

//           Flag                            Meaning
// -------------------------------------------------------------------------
// REDIRECTION_SUPPORTED               The client can receive server session
//       0x00000001                    redirection packets. If this flag is
//                                     set, the
//                                     ServerSessionRedirectionVersionMask
//                                     MUST contain the server session
//                                     redirection version that the client
//                                     supports.
// -------------------------------------------------------------------------
// ServerSessionRedirectionVersionMask The server session redirection
//       0x0000003C                    version that the client supports.
//                                     See the discussion which follows
//                                     this table for more information.
// -------------------------------------------------------------------------
// REDIRECTED_SESSIONID_FIELD_VALID    The RedirectedSessionID field
//       0x00000002                    contains an ID that identifies a
//                                     session on the server to associate
//                                     with the connection.
// -------------------------------------------------------------------------
// REDIRECTED_SMARTCARD                The client logged on with a smart
//       0x00000040                    card.
// -------------------------------------------------------------------------

// The ServerSessionRedirectionVersionMask is a 4-bit enumerated value
// containing the server session redirection version supported by the
// client. The following are possible version values.

//          Value                              Meaning
// -------------------------------------------------------------------------
// REDIRECTION_VERSION3                If REDIRECTION_SUPPORTED is set,
//          0x02                       server session redirection version 3
//                                     is supported by the client.
// -------------------------------------------------------------------------
// REDIRECTION_VERSION4                If REDIRECTION_SUPPORTED is set,
//          0x03                       server session redirection version 4
//                                     is supported by the client.
// -------------------------------------------------------------------------
// REDIRECTION_VERSION5                If REDIRECTION_SUPPORTED is set,
//          0x04                       server session redirection version 5
//                                     is supported by the client.
// -------------------------------------------------------------------------

// Note by CGR: encoding let imagine there should be a V1 an V2 redirection

// The version values cannot be combined; only one value MUST be specified
// if the REDIRECTED_SESSIONID_FIELD_VALID (0x00000002) flag is present in
// the Flags field.

// RedirectedSessionID (4 bytes): A 32-bit unsigned integer. If the
//                                REDIRECTED_SESSIONID_FIELD_VALID flag is
//                                set in the Flags field, then the
//                                RedirectedSessionID field contains a valid
//                                session identifier to which the client
//                                requests to connect.


struct CSClusterGccUserData {
    uint16_t userDataType;
    uint16_t length;

    enum {
        REDIRECTION_SUPPORTED            = 0x00000001,
        REDIRECTED_SESSIONID_FIELD_VALID = 0x00000002,
        REDIRECTED_SMARTCARD             = 0x00000040,
        ServerSessionRedirectionVersionMask = 0x0000003C,
    };
    uint32_t flags;
    uint32_t redirectedSessionID;

    CSClusterGccUserData()
    : userDataType(CS_CLUSTER)
    , length(12)
    , flags(0)
    , redirectedSessionID(0)
    {
    }

    void emit(Stream & stream)
    {
        stream.out_uint16_le(this->userDataType);
        stream.out_uint16_le(this->length);
        stream.out_uint32_le(this->flags);
        stream.out_uint32_le(this->redirectedSessionID);
    }

    void recv(Stream & stream, uint16_t length)
    {
        this->length = length;
        this->flags = stream.in_uint32_le();
        this->redirectedSessionID = stream.in_uint32_le();
    }

    void log(const char * msg)
    {
        // --------------------- Base Fields ---------------------------------------
        LOG(LOG_INFO, "%s GCC User Data CS_CLUSTER (%u bytes)", msg, this->length);
        LOG(LOG_INFO, "cs_cluster::flags [%04x]", this->flags);
        if (this->flags == REDIRECTION_SUPPORTED){
            LOG(LOG_INFO, "cs_cluster::flags::REDIRECTION_SUPPORTED");
            LOG(LOG_INFO, "cs_cluster::flags::redirectionVersion = %u",
                (this->flags & ServerSessionRedirectionVersionMask) >> 2);
        }
        if (this->flags == REDIRECTED_SESSIONID_FIELD_VALID){
            LOG(LOG_INFO, "cs_cluster::flags::REDIRECTED_SESSIONID_FIELD_VALID");
        }
        if (this->flags == REDIRECTED_SMARTCARD){
            LOG(LOG_INFO, "cs_cluster::flags::REDIRECTED_SMARTCARD");
        }
        if (this->length < 12) { return; }
        LOG(LOG_INFO, "cs_cluster::redirectedSessionID = %u", this->redirectedSessionID);
    }
};

#endif
