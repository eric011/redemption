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
   Author(s): Christophe Grosjean, Javier Caverni
   Based on xrdp Copyright (C) Jay Sorg 2004-2010

   common sec layer at core module

*/

#if !defined(__SEC_HPP__)
#define __SEC_HPP__

#include "RDP/x224.hpp"
#include "RDP/rdp.hpp"
#include "client_info.hpp"
#include "rsa_keys.hpp"
#include "constants.hpp"

#include <assert.h>
#include <stdint.h>

#include <iostream>

#warning ssl calls introduce some dependency on ssl system library, injecting it in the sec object would be better.
#include "ssl_calls.hpp"


inline static void sec_make_40bit(uint8_t* key)
{
    key[0] = 0xd1;
    key[1] = 0x26;
    key[2] = 0x9e;
}

// Output a uint32 into a buffer (little-endian)
inline static void buf_out_uint32(uint8_t* buffer, int value)
{
  buffer[0] = value & 0xff;
  buffer[1] = (value >> 8) & 0xff;
  buffer[2] = (value >> 16) & 0xff;
  buffer[3] = (value >> 24) & 0xff;
}


#warning method used by licence, common with basic crypto support code should be made common. pad are also common to several functions.
/* Generate a MAC hash (5.2.3.1), using a combination of SHA1 and MD5 */
inline static void sec_sign(uint8_t* signature, int siglen, uint8_t* session_key, int keylen,
             uint8_t* data, int datalen)
{
    static uint8_t pad_54[40] = { 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
                                 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
                                 54, 54, 54, 54, 54, 54, 54, 54
                               };
    static uint8_t pad_92[48] = { 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
                             92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
                             92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92
                           };

    uint8_t shasig[20];
    uint8_t md5sig[16];
    uint8_t lenhdr[4];
    SSL_SHA1 sha1;
    SSL_MD5 md5;

    buf_out_uint32(lenhdr, datalen);

    ssllib ssl;

    ssl.sha1_init(&sha1);
    ssl.sha1_update(&sha1, session_key, keylen);
    ssl.sha1_update(&sha1, pad_54, 40);
    ssl.sha1_update(&sha1, lenhdr, 4);
    ssl.sha1_update(&sha1, data, datalen);
    ssl.sha1_final(&sha1, shasig);

    ssl.md5_init(&md5);
    ssl.md5_update(&md5, session_key, keylen);
    ssl.md5_update(&md5, pad_92, 48);
    ssl.md5_update(&md5, shasig, 20);
    ssl.md5_final(&md5, md5sig);

    memcpy(signature, md5sig, siglen);
}



/* used in sec */
struct mcs_channel_item {
    char name[16];
    int flags;
    int chanid;
    mcs_channel_item(){
        this->name[0] = 0;
        this->flags = 0;
        this->chanid = 0;
    }
};




struct CryptContext
{
    int use_count;
    uint8_t sign_key[16]; // should I call it session_key ?
    uint8_t key[16];
    uint8_t update_key[16];
    int rc4_key_len;
    SSL_RC4 rc4_info;
    CryptContext() : use_count(0)
    {
        memset(this->sign_key, 0, 16);
    }

    /* Encrypt data using RC4 */
    void encrypt(uint8_t* data, int length)
    {
        ssllib ssl;

        if (this->use_count == 4096){
            this->update();
            if (this->rc4_key_len == 8) {
                sec_make_40bit(this->key);
            }
            ssl.rc4_set_key(this->rc4_info, this->key, this->rc4_key_len);
            this->use_count = 0;
        }
        ssl.rc4_crypt(this->rc4_info, data, data, length);
        this->use_count++;
    }

    /* Decrypt data using RC4 */
    void decrypt(uint8_t* data, int len)
    {
        ssllib ssl;

        if (this->use_count == 4096) {
            this->update();
            if (this->rc4_key_len == 8) {
                sec_make_40bit(this->key);
            }
            ssl.rc4_set_key(this->rc4_info, this->key, this->rc4_key_len);
            this->use_count = 0;
        }
        ssl.rc4_crypt(this->rc4_info, data, data, len);
        this->use_count++;
    }

    /* Generate a MAC hash (5.2.3.1), using a combination of SHA1 and MD5 */
    void sign(uint8_t* signature, int siglen, uint8_t* data, int datalen)
    {
        static uint8_t pad_54[40] = { 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
                                     54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
                                     54, 54, 54, 54, 54, 54, 54, 54
                                   };
        static uint8_t pad_92[48] = { 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
                                 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
                                 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92
                               };

        uint8_t shasig[20];
        uint8_t md5sig[16];
        uint8_t lenhdr[4];
        SSL_SHA1 sha1;
        SSL_MD5 md5;

        buf_out_uint32(lenhdr, datalen);

        ssllib ssl;

        ssl.sha1_init(&sha1);
        ssl.sha1_update(&sha1, this->sign_key, this->rc4_key_len);
        ssl.sha1_update(&sha1, pad_54, 40);
        ssl.sha1_update(&sha1, lenhdr, 4);
        ssl.sha1_update(&sha1, data, datalen);
        ssl.sha1_final(&sha1, shasig);

        ssl.md5_init(&md5);
        ssl.md5_update(&md5, this->sign_key, this->rc4_key_len);
        ssl.md5_update(&md5, pad_92, 48);
        ssl.md5_update(&md5, shasig, 20);
        ssl.md5_final(&md5, md5sig);

        memcpy(signature, md5sig, siglen);
    }

    void update()
    {
        static uint8_t pad_54[40] = {
            54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
            54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
            54, 54, 54, 54, 54, 54, 54, 54
        };

        static uint8_t pad_92[48] = {
            92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
            92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92,
            92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92, 92
        };

        uint8_t shasig[20];
        SSL_SHA1 sha1;
        SSL_MD5 md5;
        SSL_RC4 update;

        ssllib ssl;

        ssl.sha1_init(&sha1);
        ssl.sha1_update(&sha1, this->update_key, this->rc4_key_len);
        ssl.sha1_update(&sha1, pad_54, 40);
        ssl.sha1_update(&sha1, this->key, this->rc4_key_len);
        ssl.sha1_final(&sha1, shasig);

        ssl.md5_init(&md5);
        ssl.md5_update(&md5, this->update_key, this->rc4_key_len);
        ssl.md5_update(&md5, pad_92, 48);
        ssl.md5_update(&md5, shasig, 20);
        ssl.md5_final(&md5, key);

        ssl.rc4_set_key(update, this->key, this->rc4_key_len);
        ssl.rc4_crypt(update, this->key, this->key, this->rc4_key_len);
    }

};

struct Sec
{

// only in server_sec : need cleanup

    uint8_t server_random[32];
    uint8_t client_random[64];

    uint8_t pub_exp[4];
    uint8_t pub_mod[64];
    uint8_t pub_sig[64];
    uint8_t pri_exp[64];
    Stream data;

// only in rdp_sec : need cleanup
    int server_public_key_len;

// shared
    struct rdp_lic {
        uint8_t licence_key[16];
        uint8_t licence_sign_key[16];
        int licence_issued;

        rdp_lic(void){
            memset(this->licence_key, 0, 16);
            memset(this->licence_sign_key, 0, 16);
            this->licence_issued = 0;
        }
    } lic_layer;

    uint8_t * licence_data;
    size_t licence_size;

    int userid;
    vector<struct mcs_channel_item *> channel_list;

    #warning windows 2008 does not write trailer because of overflow of buffer below, checked actual size: 64 bytes on xp, 256 bytes on windows 2008
    uint8_t client_crypt_random[512];


    CryptContext encrypt, decrypt;


    uint8_t crypt_level;
    int rc4_key_size; /* 1 = 40-bit, 2 = 128-bit */

    Sec(uint8_t crypt_level) :
      licence_data(0),
      licence_size(0),
      crypt_level(crypt_level)
    {
        // from server_sec
        // CGR: see if init has influence for the 3 following fields
        memset(this->server_random, 0, 32);
        memset(this->client_random, 0, 64);
        memset(this->client_crypt_random, 0, 72);

        memset(this->pub_exp, 0, 4);
        memset(this->pub_mod, 0, 64);
        memset(this->pub_sig, 0, 64);
        memset(this->pri_exp, 0, 64);

        // from rdp_sec
        memset(this->client_crypt_random, 0, 512);
        this->server_public_key_len = 0;

        // shared
        memset(this->decrypt.key, 0, 16);
        memset(this->encrypt.key, 0, 16);
        memset(this->decrypt.update_key, 0, 16);
        memset(this->encrypt.update_key, 0, 16);
        switch (crypt_level) {
        case 1:
        case 2:
            this->rc4_key_size = 1; /* 40 bits */
            this->decrypt.rc4_key_len = 8; /* 8 = 40 bit */
            this->encrypt.rc4_key_len = 8; /* 8 = 40 bit */
        break;
        default:
        case 3:
            this->rc4_key_size = 2; /* 128 bits */
            this->decrypt.rc4_key_len = 16; /* 16 = 128 bit */
            this->encrypt.rc4_key_len = 16; /* 16 = 128 bit */
        break;
        }

    }

    ~Sec()
    {
        // clear channel_list
        int count = (int) this->channel_list.size();
        for (int index = 0; index < count; index++) {
            mcs_channel_item* channel_item = this->channel_list[index];
            if (0 != channel_item) {
                delete channel_item;
            }
        }
    }


    // 16-byte transformation used to generate export keys (6.2.2).
    static void sec_hash_16(uint8_t* out, const uint8_t* in, const uint8_t* salt1, const uint8_t* salt2)
    {
        SSL_MD5 md5;

        ssllib ssl;

        ssl.md5_init(&md5);
        ssl.md5_update(&md5, in, 16);
        ssl.md5_update(&md5, salt1, 32);
        ssl.md5_update(&md5, salt2, 32);
        ssl.md5_final(&md5, out);
    }

    // 48-byte transformation used to generate master secret (6.1) and key material (6.2.2).
    // Both SHA1 and MD5 algorithms are used.
    static void sec_hash_48(uint8_t* out, const uint8_t* in, const uint8_t* salt1, const uint8_t* salt2, const uint8_t salt)
    {
        uint8_t shasig[20];
        uint8_t pad[4];
        SSL_SHA1 sha1;
        SSL_MD5 md5;

        ssllib ssl;

        for (int i = 0; i < 3; i++) {
            memset(pad, salt + i, i + 1);

            ssl.sha1_init(&sha1);
            ssl.sha1_update(&sha1, pad, i + 1);
            ssl.sha1_update(&sha1, in, 48);
            ssl.sha1_update(&sha1, salt1, 32);
            ssl.sha1_update(&sha1, salt2, 32);
            ssl.sha1_final(&sha1, shasig);

            ssl.md5_init(&md5);
            ssl.md5_update(&md5, in, 48);
            ssl.md5_update(&md5, shasig, 20);
            ssl.md5_final(&md5, &out[i * 16]);
        }
    }

    /* process the mcs client data we received from the mcs layer */
    void server_sec_process_mcs_data(Stream & stream, ClientInfo * client_info) throw (Error)
    {
        stream.p = stream.data;
        stream.skip_uint8(23);

// 2.2.1.3.1 User Data Header (TS_UD_HEADER)
// =========================================

// type (2 bytes): A 16-bit, unsigned integer. The type of the data
//                 block that this header precedes.

// +-------------------+-------------------------------------------------------+
// | CS_CORE 0xC001 : The data block that follows contains Client Core
//                 Data (section 2.2.1.3.2).
// +-------------------+-------------------------------------------------------+
// | CS_SECURITY 0xC002 : The data block that follows contains Client
//                  Security Data (section 2.2.1.3.3).
// +-------------------+-------------------------------------------------------+
// | CS_NET 0xC003 : The data block that follows contains Client Network
//                 Data (section 2.2.1.3.4).
// +-------------------+-------------------------------------------------------+
// | CS_CLUSTER 0xC004 | The data block that follows contains Client Cluster   |
// |                   | Data (section 2.2.1.3.5).                             |
// +-------------------+-------------------------------------------------------+
// | CS_MONITOR 0xC005 | The data block that follows contains Client
//                 Monitor Data (section 2.2.1.3.6).
// +-------------------+-------------------------------------------------------+
// | SC_CORE 0x0C01 : The data block that follows contains Server Core
//                 Data (section 2.2.1.4.2)
// +-------------------+-------------------------------------------------------+
// | SC_SECURITY 0x0C02 : The data block that follows contains Server
//                 Security Data (section 2.2.1.4.3).
// +-------------------+-------------------------------------------------------+
// | SC_NET 0x0C03 : The data block that follows contains Server Network
//                 Data (section 2.2.1.4.4)
// +-------------------+-------------------------------------------------------+

// length (2 bytes): A 16-bit, unsigned integer. The size in bytes of the data
//   block, including this header.



        while (stream.check_rem(4)) {
            uint8_t * current_header = stream.p;
            uint16_t tag = stream.in_uint16_le();
            uint16_t length = stream.in_uint16_le();
            if (length < 4 || !stream.check_rem(length - 4)) {
                LOG(LOG_ERR,
                    "error reading block tag %d size %d\n",
                    tag, length);
                break;
            }

            switch (tag){
                case CS_CORE:
                    #warning we should check length to call the two variants of core_data (or begin by reading the common part then the extended part)
                    this->server_sec_parse_mcs_data_cs_core(stream, client_info);
                break;
                case CS_SECURITY:
                    this->server_sec_parse_mcs_data_cs_security(stream);
                break;
                case CS_NET:
                    this->server_sec_parse_mcs_data_cs_net(stream, client_info);
                break;
                case CS_CLUSTER:
                    this->server_sec_parse_mcs_data_cs_cluster(stream, client_info);
                break;
                case CS_MONITOR:
                    this->server_sec_parse_mcs_data_cs_monitor(stream);
                break;
                case SC_CORE:
                    this->server_sec_parse_mcs_data_sc_core(stream);
                break;
                case SC_SECURITY:
                    this->server_sec_parse_mcs_data_sc_security(stream);
                break;
                case SC_NET:
                    this->server_sec_parse_mcs_data_sc_net(stream);
                break;
                default:
                    LOG(LOG_INFO, "Unknown data block tag\n");
                break;
            }
            stream.p = current_header + length;
        }
    }

    void unicode_in(Stream & stream, int uni_len, uint8_t* dst, int dst_len) throw (Error)
    {
        int dst_index = 0;
        int src_index = 0;
        while (src_index < uni_len) {
            if (dst_index >= dst_len || src_index > 512) {
                break;
            }
            dst[dst_index] = stream.in_uint8();
            stream.skip_uint8(1);
            dst_index++;
            src_index += 2;
        }
        stream.skip_uint8(2);
    }


    void server_sec_send_lic_initial(Transport * trans) throw (Error)
    {
        /* some compilers need unsigned char to avoid warnings */
        static uint8_t lic1[322] = {
            0x80, 0x00, 0x3e, 0x01, 0x01, 0x02, 0x3e, 0x01,
            0x7b, 0x3c, 0x31, 0xa6, 0xae, 0xe8, 0x74, 0xf6,
            0xb4, 0xa5, 0x03, 0x90, 0xe7, 0xc2, 0xc7, 0x39,
            0xba, 0x53, 0x1c, 0x30, 0x54, 0x6e, 0x90, 0x05,
            0xd0, 0x05, 0xce, 0x44, 0x18, 0x91, 0x83, 0x81,
            0x00, 0x00, 0x04, 0x00, 0x2c, 0x00, 0x00, 0x00,
            0x4d, 0x00, 0x69, 0x00, 0x63, 0x00, 0x72, 0x00,
            0x6f, 0x00, 0x73, 0x00, 0x6f, 0x00, 0x66, 0x00,
            0x74, 0x00, 0x20, 0x00, 0x43, 0x00, 0x6f, 0x00,
            0x72, 0x00, 0x70, 0x00, 0x6f, 0x00, 0x72, 0x00,
            0x61, 0x00, 0x74, 0x00, 0x69, 0x00, 0x6f, 0x00,
            0x6e, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x32, 0x00, 0x33, 0x00, 0x36, 0x00, 0x00, 0x00,
            0x0d, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x03, 0x00, 0xb8, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x06, 0x00, 0x5c, 0x00, 0x52, 0x53, 0x41, 0x31,
            0x48, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
            0x3f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
            0x01, 0xc7, 0xc9, 0xf7, 0x8e, 0x5a, 0x38, 0xe4,
            0x29, 0xc3, 0x00, 0x95, 0x2d, 0xdd, 0x4c, 0x3e,
            0x50, 0x45, 0x0b, 0x0d, 0x9e, 0x2a, 0x5d, 0x18,
            0x63, 0x64, 0xc4, 0x2c, 0xf7, 0x8f, 0x29, 0xd5,
            0x3f, 0xc5, 0x35, 0x22, 0x34, 0xff, 0xad, 0x3a,
            0xe6, 0xe3, 0x95, 0x06, 0xae, 0x55, 0x82, 0xe3,
            0xc8, 0xc7, 0xb4, 0xa8, 0x47, 0xc8, 0x50, 0x71,
            0x74, 0x29, 0x53, 0x89, 0x6d, 0x9c, 0xed, 0x70,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x08, 0x00, 0x48, 0x00, 0xa8, 0xf4, 0x31, 0xb9,
            0xab, 0x4b, 0xe6, 0xb4, 0xf4, 0x39, 0x89, 0xd6,
            0xb1, 0xda, 0xf6, 0x1e, 0xec, 0xb1, 0xf0, 0x54,
            0x3b, 0x5e, 0x3e, 0x6a, 0x71, 0xb4, 0xf7, 0x75,
            0xc8, 0x16, 0x2f, 0x24, 0x00, 0xde, 0xe9, 0x82,
            0x99, 0x5f, 0x33, 0x0b, 0xa9, 0xa6, 0x94, 0xaf,
            0xcb, 0x11, 0xc3, 0xf2, 0xdb, 0x09, 0x42, 0x68,
            0x29, 0x56, 0x58, 0x01, 0x56, 0xdb, 0x59, 0x03,
            0x69, 0xdb, 0x7d, 0x37, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x0e, 0x00, 0x0e, 0x00, 0x6d, 0x69, 0x63, 0x72,
            0x6f, 0x73, 0x6f, 0x66, 0x74, 0x2e, 0x63, 0x6f,
            0x6d, 0x00
       };

        Stream stream(8192);
        X224Out tpdu(X224Packet::DT_TPDU, stream);
        McsOut sdin_out(stream, MCS_SDIN, this->userid, MCS_GLOBAL_CHANNEL);
        stream.out_copy_bytes((char*)lic1, 322);
        sdin_out.end();
        tpdu.end();
        tpdu.send(trans);
    }

    void server_sec_send_lic_response(Transport * trans) throw (Error)
    {
        /* some compilers need unsigned char to avoid warnings */
        static uint8_t lic2[20] = { 0x80, 0x00, 0x10, 0x00, 0xff, 0x02, 0x10, 0x00,
                                 0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
                                 0x28, 0x14, 0x00, 0x00
                               };

        Stream stream(8192);
        X224Out tpdu(X224Packet::DT_TPDU, stream);
        McsOut sdin_out(stream, MCS_SDIN, this->userid, MCS_GLOBAL_CHANNEL);
        stream.out_copy_bytes((char*)lic2, 20);
        sdin_out.end();
        tpdu.end();
        tpdu.send(trans);
    }

    void server_sec_send_media_lic_response(Transport * trans) throw (Error)
    {
        /* mce */
        /* some compilers need unsigned char to avoid warnings */
        static uint8_t lic3[20] = { 0x80, 0x02, 0x10, 0x00, 0xff, 0x03, 0x10, 0x00,
                                 0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
                                 0xf3, 0x99, 0x00, 0x00
                                 };

        Stream stream(8192);
        X224Out tpdu(X224Packet::DT_TPDU, stream);
        McsOut sdin_out(stream, MCS_SDIN, this->userid, MCS_GLOBAL_CHANNEL);
        stream.out_copy_bytes((char*)lic3, 20);
        sdin_out.end();
        tpdu.end();
        tpdu.send(trans);
// ----------------------------

    }

    void server_sec_rsa_op()
    {
        ssl_mod_exp(this->client_random, 64,
                    this->client_crypt_random, 64,
                    this->pub_mod, 64,
                    this->pri_exp, 64);
    }

    void server_sec_process_logon_info(Stream & stream, ClientInfo * client_info) throw (Error)
    {
        // LOG(LOG_DEBUG, "server_sec_process_logon_info\n");
        stream.skip_uint8(4);
        int flags = stream.in_uint32_le();
        /* this is the first test that the decrypt is working */
        if ((flags & RDP_LOGON_NORMAL) != RDP_LOGON_NORMAL) /* 0x33 */
        {                                                   /* must be or error */
            throw Error(ERR_SEC_PROCESS_LOGON_UNKNOWN_FLAGS);
        }
        if (flags & RDP_LOGON_LEAVE_AUDIO) {
            client_info->sound_code = 1;
        }
        if ((flags & RDP_LOGON_AUTO) && (!client_info->is_mce))
            /* todo, for now not allowing autologon and mce both */
        {
            client_info->rdp_autologin = 1;
        }
        if (flags & RDP_COMPRESSION) {
            client_info->rdp_compression = 1;
        }
        unsigned len_domain = stream.in_uint16_le();
        unsigned len_user = stream.in_uint16_le();
        unsigned len_password = stream.in_uint16_le();
        unsigned len_program = stream.in_uint16_le();
        unsigned len_directory = stream.in_uint16_le();
        /* todo, we should error out in any of the above lengths are > 512 */
        /* to avoid buffer overruns */
        unicode_in(stream, len_domain, (uint8_t*)client_info->domain, 255);
        unicode_in(stream, len_user, (uint8_t*)client_info->username, 255);
        // LOG(LOG_DEBUG, "setting username to %s\n", client_info->username);

        if (flags & RDP_LOGON_AUTO) {
            unicode_in(stream, len_password, (uint8_t*)client_info->password, 255);
        } else {
            stream.skip_uint8(len_password + 2);
        }
        unicode_in(stream, len_program, (uint8_t*)client_info->program, 255);
        unicode_in(stream, len_directory, (uint8_t*)client_info->directory, 255);
        if (flags & RDP_LOGON_BLOB) {
            stream.skip_uint8(2);                                    /* unknown */
            unsigned len_ip = stream.in_uint16_le();
            uint8_t tmpdata[256];
            unicode_in(stream, len_ip - 2, tmpdata, 255);
            unsigned len_dll = stream.in_uint16_le();
            unicode_in(stream, len_dll - 2, tmpdata, 255);
            stream.in_uint32_le(); /* len of timetone */
            stream.skip_uint8(62); /* skip */
            stream.skip_uint8(22); /* skip misc. */
            stream.skip_uint8(62); /* skip */
            stream.skip_uint8(26); /* skip stuff */
            client_info->rdp5_performanceflags = stream.in_uint32_le();
        }
    }

    // 2.2.1.3.2 Client Core Data (TS_UD_CS_CORE)
    // -------------------------------------
    // Below relevant quotes from MS-RDPBCGR v20100601 (2.2.1.3.2)

    // header (4 bytes): GCC user data block header, as specified in section
    //                   2.2.1.3.1. The User Data Header type field MUST be
    //                   set to CS_CORE (0xC001).

    // version (4 bytes): A 32-bit, unsigned integer. Client version number
    //                    for the RDP. The major version number is stored in
    //                    the high 2 bytes, while the minor version number
    //                    is stored in the low 2 bytes.
    //
    //         Value Meaning
    //         0x00080001 RDP 4.0 clients
    //         0x00080004 RDP 5.0, 5.1, 5.2, 6.0, 6.1, and 7.0 clients

    // desktopWidth (2 bytes): A 16-bit, unsigned integer. The requested
    //                         desktop width in pixels (up to a maximum
    //                         value of 4096 pixels).

    // desktopHeight (2 bytes): A 16-bit, unsigned integer. The requested
    //                         desktop height in pixels (up to a maximum
    //                         value of 2048 pixels).

    // colorDepth (2 bytes): A 16-bit, unsigned integer. The requested color
    //                       depth. Values in this field MUST be ignored if
    //                       the postBeta2ColorDepth field is present.
    //          Value Meaning
    //          RNS_UD_COLOR_4BPP 0xCA00 4 bits-per-pixel (bpp)
    //          RNS_UD_COLOR_8BPP 0xCA01 8 bpp

    // SASSequence (2 bytes): A 16-bit, unsigned integer. Secure access
    //                        sequence. This field SHOULD be set to
    //                        RNS_UD_SAS_DEL (0xAA03).

    // keyboardLayout (4 bytes): A 32-bit, unsigned integer. Keyboard layout
    //                           (active input locale identifier). For a
    //                           list of possible input locales, see
    //                           [MSDN-MUI].

    // clientBuild (4 bytes): A 32-bit, unsigned integer. The build number
    // of the client.

    // clientName (32 bytes): Name of the client computer. This field
    //                        contains up to 15 Unicode characters plus a
    //                        null terminator.

    // keyboardType (4 bytes): A 32-bit, unsigned integer. The keyboard type.
    //              Value Meaning
    //              0x00000001 IBM PC/XT or compatible (83-key) keyboard
    //              0x00000002 Olivetti "ICO" (102-key) keyboard
    //              0x00000003 IBM PC/AT (84-key) and similar keyboards
    //              0x00000004 IBM enhanced (101-key or 102-key) keyboard
    //              0x00000005 Nokia 1050 and similar keyboards
    //              0x00000006 Nokia 9140 and similar keyboards
    //              0x00000007 Japanese keyboard

    // keyboardSubType (4 bytes): A 32-bit, unsigned integer. The keyboard
    //                        subtype (an original equipment manufacturer-
    //                        -dependent value).

    // keyboardFunctionKey (4 bytes): A 32-bit, unsigned integer. The number
    //                        of function keys on the keyboard.

    // imeFileName (64 bytes): A 64-byte field. The Input Method Editor
    //                        (IME) file name associated with the input
    //                        locale. This field contains up to 31 Unicode
    //                        characters plus a null terminator.

    // --> Note By CGR How do we know that the following fields are
    //     present of Not ? The only rational method I see is to look
    //     at the length field in the preceding User Data Header
    //     120 bytes without optional data
    //     216 bytes with optional data present

    // postBeta2ColorDepth (2 bytes): A 16-bit, unsigned integer. The
    //                        requested color depth. Values in this field
    //                        MUST be ignored if the highColorDepth field
    //                        is present.
    //       Value Meaning
    //       RNS_UD_COLOR_4BPP 0xCA00        : 4 bits-per-pixel (bpp)
    //       RNS_UD_COLOR_8BPP 0xCA01        : 8 bpp
    //       RNS_UD_COLOR_16BPP_555 0xCA02   : 15-bit 555 RGB mask
    //                                         (5 bits for red, 5 bits for
    //                                         green, and 5 bits for blue)
    //       RNS_UD_COLOR_16BPP_565 0xCA03   : 16-bit 565 RGB mask
    //                                         (5 bits for red, 6 bits for
    //                                         green, and 5 bits for blue)
    //       RNS_UD_COLOR_24BPP 0xCA04       : 24-bit RGB mask
    //                                         (8 bits for red, 8 bits for
    //                                         green, and 8 bits for blue)
    // If this field is present, all of the preceding fields MUST also be
    // present. If this field is not present, all of the subsequent fields
    // MUST NOT be present.

    // clientProductId (2 bytes): A 16-bit, unsigned integer. The client
    //                          product ID. This field SHOULD be initialized
    //                          to 1. If this field is present, all of the
    //                          preceding fields MUST also be present. If
    //                          this field is not present, all of the
    //                          subsequent fields MUST NOT be present.

    // serialNumber (4 bytes): A 32-bit, unsigned integer. Serial number.
    //                         This field SHOULD be initialized to 0. If
    //                         this field is present, all of the preceding
    //                         fields MUST also be present. If this field
    //                         is not present, all of the subsequent fields
    //                         MUST NOT be present.

    // highColorDepth (2 bytes): A 16-bit, unsigned integer. The requested
    //                         color depth.
    //          Value Meaning
    // HIGH_COLOR_4BPP  0x0004             : 4 bpp
    // HIGH_COLOR_8BPP  0x0008             : 8 bpp
    // HIGH_COLOR_15BPP 0x000F             : 15-bit 555 RGB mask
    //                                       (5 bits for red, 5 bits for
    //                                       green, and 5 bits for blue)
    // HIGH_COLOR_16BPP 0x0010             : 16-bit 565 RGB mask
    //                                       (5 bits for red, 6 bits for
    //                                       green, and 5 bits for blue)
    // HIGH_COLOR_24BPP 0x0018             : 24-bit RGB mask
    //                                       (8 bits for red, 8 bits for
    //                                       green, and 8 bits for blue)
    //
    // If this field is present, all of the preceding fields MUST also be
    // present. If this field is not present, all of the subsequent fields
    // MUST NOT be present.

    // supportedColorDepths (2 bytes): A 16-bit, unsigned integer. Specifies
    //                                 the high color depths that the client
    //                                 is capable of supporting.
    //
    //         Flag Meaning
    //   RNS_UD_24BPP_SUPPORT 0x0001       : 24-bit RGB mask
    //                                       (8 bits for red, 8 bits for
    //                                       green, and 8 bits for blue)
    //   RNS_UD_16BPP_SUPPORT 0x0002       : 16-bit 565 RGB mask
    //                                       (5 bits for red, 6 bits for
    //                                       green, and 5 bits for blue)
    //   RNS_UD_15BPP_SUPPORT 0x0004       : 15-bit 555 RGB mask
    //                                       (5 bits for red, 5 bits for
    //                                       green, and 5 bits for blue)
    //   RNS_UD_32BPP_SUPPORT 0x0008       : 32-bit RGB mask
    //                                       (8 bits for the alpha channel,
    //                                       8 bits for red, 8 bits for
    //                                       green, and 8 bits for blue)
    // If this field is present, all of the preceding fields MUST also be
    // present. If this field is not present, all of the subsequent fields
    // MUST NOT be present.

    // earlyCapabilityFlags (2 bytes)      : A 16-bit, unsigned integer. It
    //                                       specifies capabilities early in
    //                                       the connection sequence.
    //        Flag                        Meaning
    //  RNS_UD_CS_SUPPORT_ERRINFO_PDU Indicates that the client supports
    //    0x0001                        the Set Error Info PDU
    //                                 (section 2.2.5.1).
    //
    //  RNS_UD_CS_WANT_32BPP_SESSION Indicates that the client is requesting
    //    0x0002                     a session color depth of 32 bpp. This
    //                               flag is necessary because the
    //                               highColorDepth field does not support a
    //                               value of 32. If this flag is set, the
    //                               highColorDepth field SHOULD be set to
    //                               24 to provide an acceptable fallback
    //                               for the scenario where the server does
    //                               not support 32 bpp color.
    //
    //  RNS_UD_CS_SUPPORT_STATUSINFO_PDU  Indicates that the client supports
    //    0x0004                          the Server Status Info PDU
    //                                    (section 2.2.5.2).
    //
    //  RNS_UD_CS_STRONG_ASYMMETRIC_KEYS  Indicates that the client supports
    //    0x0008                          asymmetric keys larger than
    //                                    512 bits for use with the Server
    //                                    Certificate (section 2.2.1.4.3.1)
    //                                    sent in the Server Security Data
    //                                    block (section 2.2.1.4.3).
    //
    //  RNS_UD_CS_VALID_CONNECTION_TYPE Indicates that the connectionType
    //     0x0020                       field contains valid data.
    //
    //  RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU Indicates that the client
    //     0x0040                            supports the Monitor Layout PDU
    //                                       (section 2.2.12.1).
    //
    // If this field is present, all of the preceding fields MUST also be
    // present. If this field is not present, all of the subsequent fields
    // MUST NOT be present.

    // clientDigProductId (64 bytes): Contains a value that uniquely
    //                                identifies the client. If this field
    //                                is present, all of the preceding
    //                                fields MUST also be present. If this
    //                                field is not present, all of the
    //                                subsequent fields MUST NOT be present.

    // connectionType (1 byte): An 8-bit unsigned integer. Hints at the type
    //                      of network connection being used by the client.
    //                      This field only contains valid data if the
    //                      RNS_UD_CS_VALID_CONNECTION_TYPE (0x0020) flag
    //                      is present in the earlyCapabilityFlags field.
    //
    //    Value                          Meaning
    //  CONNECTION_TYPE_MODEM 0x01 : Modem (56 Kbps)
    //  CONNECTION_TYPE_BROADBAND_LOW 0x02 : Low-speed broadband
    //                                 (256 Kbps - 2 Mbps)
    //  CONNECTION_TYPE_SATELLITE 0x03 : Satellite
    //                                 (2 Mbps - 16 Mbps with high latency)
    //  CONNECTION_TYPE_BROADBAND_HIGH 0x04 : High-speed broadband
    //                                 (2 Mbps - 10 Mbps)
    //  CONNECTION_TYPE_WAN 0x05 : WAN (10 Mbps or higher with high latency)
    //  CONNECTION_TYPE_LAN 0x06 : LAN (10 Mbps or higher)

    // If this field is present, all of the preceding fields MUST also be
    // present. If this field is not present, all of the subsequent fields
    // MUST NOT be present.

    // pad1octet (1 byte): An 8-bit, unsigned integer. Padding to align the
    //   serverSelectedProtocol field on the correct byte boundary. If this
    //   field is present, all of the preceding fields MUST also be present.
    //   If this field is not present, all of the subsequent fields MUST NOT
    //   be present.

    // serverSelectedProtocol (4 bytes): A 32-bit, unsigned integer that
    //   contains the value returned by the server in the selectedProtocol
    //   field of the RDP Negotiation Response (section 2.2.1.2.1). In the
    //   event that an RDP Negotiation Response was not received from the
    //   server, this field MUST be initialized to PROTOCOL_RDP (0). This
    //   field MUST be present if an RDP Negotiation Request (section
    //   2.2.1.1.1) was sent to the server. If this field is present,
    //   then all of the preceding fields MUST also be present.

    #warning use official field names from MS-RDPBCGR
    void server_sec_parse_mcs_data_cs_core(Stream & stream, ClientInfo * client_info)
    {
        LOG(LOG_INFO, "PARSE CS_CORE\n");
        uint16_t rdp_version = stream.in_uint16_le();
        LOG(LOG_INFO, "core_data: rdp_version (1=RDP1, 4=RDP5) %u\n", rdp_version);
        uint16_t dummy1 = stream.in_uint16_le();
        LOG(LOG_INFO, "core_data: ?? = %u\n", dummy1);
        client_info->width = stream.in_uint16_le();
        LOG(LOG_INFO, "core_data: width = %u\n", client_info->width);
        client_info->height = stream.in_uint16_le();
        LOG(LOG_INFO, "core_data: height = %u\n", client_info->height);
        uint16_t bpp_code = stream.in_uint16_le();
        LOG(LOG_INFO, "core_data: bpp_code = %x\n", bpp_code);
        uint16_t dummy2 = stream.in_uint16_le();
        LOG(LOG_INFO, "core_data: ?? = %x\n", dummy2);
        /* get keylayout */
        client_info->keylayout = stream.in_uint32_le();
        LOG(LOG_INFO, "core_data: layout = %x\n", client_info->keylayout);
        /* get build : windows build */
        client_info->build = stream.in_uint32_le();
        LOG(LOG_INFO, "core_data: build = %x\n", client_info->build);

        /* get hostname (it is UTF16, windows flavored widechars) */
        /* Unicode name of client is padded to 32 bytes */
        stream.in_uni_to_ascii_str(client_info->hostname, 32);
        LOG(LOG_INFO, "core_data: hostname = %s\n", client_info->hostname);

        uint32_t keyboard_type = stream.in_uint32_le();
        LOG(LOG_INFO, "core_data: keyboard_type = %x\n", keyboard_type);
        uint32_t keyboard_subtype = stream.in_uint32_le();
        LOG(LOG_INFO, "core_data: keyboard_subtype = %x\n", keyboard_subtype);
        uint32_t keyboard_functionkeys = stream.in_uint32_le();
        LOG(LOG_INFO, "core_data: keyboard_functionkeys = %x\n", keyboard_functionkeys);
        stream.skip_uint8(64);

        client_info->bpp = 8;
        int i = stream.in_uint16_le();
        switch (i) {
        case 0xca01:
        {
            uint16_t clientProductId = stream.in_uint16_le();
            uint32_t serialNumber = stream.in_uint32_le();
            uint16_t rdp_bpp = stream.in_uint16_le();
            uint16_t supportedColorDepths = stream.in_uint16_le();
            if (rdp_bpp <= 24){
                client_info->bpp = rdp_bpp;
            }
            else {
                client_info->bpp = 24;
            }
        }
            break;
        case 0xca02:
            client_info->bpp = 15;
            break;
        case 0xca03:
            client_info->bpp = 16;
            break;
        case 0xca04:
            client_info->bpp = 24;
            break;
        }
        LOG(LOG_INFO, "core_data: bpp = %u\n", client_info->bpp);
    }

    // 2.2.1.3.3 Client Security Data (TS_UD_CS_SEC)
    // ---------------------------------------------
    // The TS_UD_CS_SEC data block contains security-related information used to
    // advertise client cryptographic support. This information is only relevant
    // when Standard RDP Security mechanisms (section 5.3) will be used. See
    // sections 3 and 5.3.2 for a detailed discussion of how this information is
    // used.

    // header (4 bytes): GCC user data block header as described in User Data
    //                   Header (section 2.2.1.3.1). The User Data Header type
    //                   field MUST be set to CS_SECURITY (0xC002).

    // encryptionMethods (4 bytes): A 32-bit, unsigned integer. Cryptographic
    //                              encryption methods supported by the client
    //                              and used in conjunction with Standard RDP
    //                              Security The server MUST select one of these
    //                              methods. Section 5.3.2 describes how the
    //                              client and server negotiate the security
    //                              parameters for a given connection.
    //
    //           Value                           Meaning
    // -------------------------------------------------------------------------
    //    40BIT_ENCRYPTION_FLAG   40-bit session keys MUST be used to encrypt
    //       0x00000001           data (with RC4) and generate Message
    //                            Authentication Codes (MAC).
    // -------------------------------------------------------------------------
    //    128BIT_ENCRYPTION_FLAG  128-bit session keys MUST be used to encrypt
    //       0x00000002           data (with RC4) and generate MACs.
    // -------------------------------------------------------------------------
    //    56BIT_ENCRYPTION_FLAG   56-bit session keys MUST be used to encrypt
    //       0x00000008           data (with RC4) and generate MACs.
    // -------------------------------------------------------------------------
    //   FIPS_ENCRYPTION_FLAG All encryption and Message Authentication Code
    //                            generation routines MUST be Federal
    //       0x00000010           Information Processing Standard (FIPS) 140-1
    //                            compliant.

    // extEncryptionMethods (4 bytes): A 32-bit, unsigned integer. This field is
    //                               used exclusively for the French locale.
    //                               In French locale clients, encryptionMethods
    //                               MUST be set to 0 and extEncryptionMethods
    //                               MUST be set to the value to which
    //                               encryptionMethods would have been set.
    //                               For non-French locale clients, this field
    //                               MUST be set to 0

    void server_sec_parse_mcs_data_cs_security(Stream & stream)
    {
        LOG(LOG_INFO, "CS_SECURITY\n");
    }

    // 2.2.1.3.4 Client Network Data (TS_UD_CS_NET)
    // --------------------------------------------
    // The TS_UD_CS_NET packet contains a list of requested virtual channels.

    // header (4 bytes): A 32-bit, unsigned integer. GCC user data block header,
    //                   as specified in User Data Header (section 2.2.1.3.1).
    //                   The User Data Header type field MUST be set to CS_NET
    //                   (0xC003).

    // channelCount (4 bytes): A 32-bit, unsigned integer. The number of
    //                         requested static virtual channels (the maximum
    //                         allowed is 31).

    // channelDefArray (variable): A variable-length array containing the
    //                             information for requested static virtual
    //                             channels encapsulated in CHANNEL_DEF
    //                             structures (section 2.2.1.3.4.1). The number
    //                             of CHANNEL_DEF structures which follows is
    //                             given by the channelCount field.

    // 2.2.1.3.4.1 Channel Definition Structure (CHANNEL_DEF)
    // ------------------------------------------------------
    // The CHANNEL_DEF packet contains information for a particular static
    // virtual channel.

    // name (8 bytes): An 8-byte array containing a null-terminated collection
    //                 of seven ANSI characters that uniquely identify the
    //                 channel.

    // options (4 bytes): A 32-bit, unsigned integer. Channel option flags.
    //
    //           Flag                             Meaning
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_INITIALIZED   Absence of this flag indicates that this
    //        0x80000000            channel is a placeholder and that the
    //                              server MUST NOT set it up.
    // ------------------------------------------------------------------------
    // CHANNEL_OPTION_ENCRYPT_RDP   This flag is unused and its value MUST be
    //        0x40000000            ignored by the server.
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_ENCRYPT_SC    This flag is unused and its value MUST be
    //        0x20000000            ignored by the server.
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_ENCRYPT_CS    This flag is unused and its value MUST be
    //        0x10000000            ignored by the server.
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_PRI_HIGH      Channel data MUST be sent with high MCS
    //        0x08000000            priority.
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_PRI_MED       Channel data MUST be sent with medium
    //        0x04000000            MCS priority.
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_PRI_LOW       Channel data MUST be sent with low MCS
    //        0x02000000            priority.
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_COMPRESS_RDP  Virtual channel data MUST be compressed
    //        0x00800000            if RDP data is being compressed.
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_COMPRESS      Virtual channel data MUST be compressed,
    //        0x00400000            regardless of RDP compression settings.
    // -------------------------------------------------------------------------
    // CHANNEL_OPTION_SHOW_PROTOCOL The value of this flag MUST be ignored by
    //        0x00200000            the server. The visibility of the Channel
    //                              PDU Header (section 2.2.6.1.1) is
    //                              determined by the CHANNEL_FLAG_SHOW_PROTOCOL
    //                              (0x00000010) flag as defined in the flags
    //                              field (section 2.2.6.1.1).
    // -------------------------------------------------------------------------
    //REMOTE_CONTROL_PERSISTENT     Channel MUST be persistent across remote
    //                              control 0x00100000 transactions.

    // this adds the mcs channels in the list of channels to be used when
    // creating the server mcs data
    void server_sec_parse_mcs_data_cs_net(Stream & stream, ClientInfo * client_info)
    {
        LOG(LOG_INFO, "CS_NET\n");
        // this is an option set in rdpproxy.ini
        // to disable all channels (no clipboard, no device redirection, etc)
        if (client_info->channel_code != 1) { /* are channels on? */
            return;
        }
        uint32_t channelCount = stream.in_uint32_le();

        #warning make an object with the channel list and let it manage creation of channels
        for (uint32_t index = 0; index < channelCount; index++) {
            struct mcs_channel_item *channel_item = new mcs_channel_item; /* zeroed */
            memcpy(channel_item->name, stream.in_uint8p(8), 8);
            channel_item->flags = stream.in_uint32_be();
            channel_item->chanid = MCS_GLOBAL_CHANNEL + (index + 1);
            this->channel_list.push_back(channel_item);
        }
    }


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

    // The version values cannot be combined; only one value MUST be specified
    // if the REDIRECTED_SESSIONID_FIELD_VALID (0x00000002) flag is present in
    // the Flags field.

    // RedirectedSessionID (4 bytes): A 32-bit unsigned integer. If the
    //                                REDIRECTED_SESSIONID_FIELD_VALID flag is
    //                                set in the Flags field, then the
    //                                RedirectedSessionID field contains a valid
    //                                session identifier to which the client
    //                                requests to connect.

    // This is this header that contains the console flag (undocumented ?)
    void server_sec_parse_mcs_data_cs_cluster(Stream & stream, ClientInfo * client_info)
    {
        LOG(LOG_INFO, "CS_CLUSTER\n");
        uint32_t flags = stream.in_uint32_le();
        LOG(LOG_INFO, "cluster_data: flags = %x\n", flags);
        client_info->console_session = (flags & 0x2) != 0;
    }

    // 2.2.1.3.6 Client Monitor Data (TS_UD_CS_MONITOR)
    // ------------------------------------------------
    // The TS_UD_CS_MONITOR packet describes the client-side display monitor
    // layout. This packet is an Extended Client Data Block and MUST NOT be sent
    // to a server which does not advertise support for Extended Client Data
    // Blocks by using the EXTENDED_CLIENT_DATA_SUPPORTED flag (0x00000001) as
    // described in section 2.2.1.2.1.

    // header (4 bytes): GCC user data block header, as specified in User Data
    //                   Header (section 2.2.1.3.1). The User Data Header type
    //                   field MUST be set to CS_MONITOR (0xC005).

    // flags (4 bytes): A 32-bit, unsigned integer. This field is unused and
    //                  reserved for future use.

    // monitorCount (4 bytes): A 32-bit, unsigned integer. The number of display
    //                         monitor definitions in the monitorDefArray field
    //                        (the maximum allowed is 16).

    // monitorDefArray (variable): A variable-length array containing a series
    //                             of TS_MONITOR_DEF structures (section
    //                             2.2.1.3.6.1) which describe the display
    //                             monitor layout of the client. The number of
    //                             TS_MONITOR_DEF structures is given by the
    //                             monitorCount field.


    // 2.2.1.3.6.1 Monitor Definition (TS_MONITOR_DEF)
    // -----------------------------------------------
    // The TS_MONITOR_DEF packet describes the configuration of a client-side
    // display monitor. The x and y coordinates used to describe the monitor
    // position MUST be relative to the upper-left corner of the monitor
    // designated as the "primary display monitor" (the upper-left corner of the
    // primary monitor is always (0, 0)).

    // left (4 bytes): A 32-bit, unsigned integer. Specifies the x-coordinate of
    //                 the upper-left corner of the display monitor.

    // top (4 bytes): A 32-bit, unsigned integer. Specifies the y-coordinate of
    //                the upper-left corner of the display monitor.

    // right (4 bytes): A 32-bit, unsigned integer. Specifies the x-coordinate
    //                  of the lower-right corner of the display monitor.

    // bottom (4 bytes): A 32-bit, unsigned integer. Specifies the y-coordinate
    //                   of the lower-right corner of the display monitor.

    // flags (4 bytes): A 32-bit, unsigned integer. Monitor configuration flags.

    //        Value                          Meaning
    // -------------------------------------------------------------------------
    // TS_MONITOR_PRIMARY            The top, left, right and bottom fields
    //      0x00000001               describe the position of the primary
    //                               monitor.
    // -------------------------------------------------------------------------

    void server_sec_parse_mcs_data_cs_monitor(Stream & stream)
    {
        LOG(LOG_INFO, "CS_MONITOR\n");
    }

    // 2.2.1.4.2 Server Core Data (TS_UD_SC_CORE)
    void server_sec_parse_mcs_data_sc_core(Stream & stream)
    {
        LOG(LOG_INFO, "SC_CORE\n");
    }

    // 2.2.1.4.3 Server Security Data (TS_UD_SC_SEC1)
    void server_sec_parse_mcs_data_sc_security(Stream & stream)
    {
        LOG(LOG_INFO, "SC_SECURITY\n");
    }

    // 2.2.1.4.4 Server Network Data (TS_UD_SC_NET)
    void server_sec_parse_mcs_data_sc_net(Stream & stream)
    {
        LOG(LOG_INFO, "SC_NET\n");
    }

    /*****************************************************************************/
    void server_sec_disconnect(Transport * trans)
    {
        Stream stream(8192);
        X224Out tpdu(X224Packet::DT_TPDU, stream);

        stream.out_uint8((MCS_DPUM << 2) | 1);
        stream.out_uint8(0x80);

        tpdu.end();
        tpdu.send(trans);
    }


// 2.2.1.3 Client MCS Connect Initial PDU with GCC Conference Create Request
// =========================================================================

// The MCS Connect Initial PDU is an RDP Connection Sequence PDU sent from
// client to server during the Basic Settings Exchange phase (see section
// 1.3.1.1). It is sent after receiving the X.224 Connection Confirm PDU
// (section 2.2.1.2). The MCS Connect Initial PDU encapsulates a GCC Conference
// Create Request, which encapsulates concatenated blocks of settings data. A
// basic high-level overview of the nested structure for the Client MCS Connect
// Initial PDU is illustrated in section 1.3.1.1, in the figure specifying MCS
// Connect Initial PDU. Note that the order of the settings data blocks is
// allowed to vary from that shown in the previously mentioned figure and the
// message syntax layout that follows. This is possible because each data block
// is identified by a User Data Header structure (section 2.2.1.3.1).

// tpktHeader (4 bytes): A TPKT Header, as specified in [T123] section 8.

// x224Data (3 bytes): An X.224 Class 0 Data TPDU, as specified in [X224]
//   section 13.7.

// mcsCi (variable): Variable-length BER-encoded MCS Connect Initial structure
//   (using definite-length encoding) as described in [T125] (the ASN.1
//   structure definition is detailed in [T125] section 7, part 2). The userData
//   field of the MCS Connect Initial encapsulates the GCC Conference Create
//   Request data (contained in the gccCCrq and subsequent fields). The maximum
//   allowed size of this user data is 1024 bytes, which implies that the
//   combined size of the gccCCrq and subsequent fields MUST be less than 1024
//   bytes.

// gccCCrq (variable): Variable-length Packed Encoding Rule encoded
//   (PER-encoded) GCC Connect Data structure, which encapsulates a Connect GCC
//   PDU that contains a GCC Conference Create Request structure as described in
//   [T124] (the ASN.1 structure definitions are detailed in [T124] section 8.7)
//   appended as user data to the MCS Connect Initial (using the format
//   described in [T124] sections 9.5 and 9.6). The userData field of the GCC
//   Conference Create Request contains one user data set consisting of
//   concatenated client data blocks.

// clientCoreData (216 bytes): Client Core Data structure (section 2.2.1.3.2).

// clientSecurityData (12 bytes): Client Security Data structure (section
//   2.2.1.3.3).

// clientNetworkData (variable): Optional and variable-length Client Network
//   Data structure (section 2.2.1.3.4).

// clientClusterData (12 bytes): Optional Client Cluster Data structure (section
//   2.2.1.3.5).

// clientMonitorData (variable): Optional Client Monitor Data structure (section
//   2.2.1.3.6). This field MUST NOT be included if the server does not
//   advertise support for extended client data blocks by using the
//   EXTENDED_CLIENT_DATA_SUPPORTED flag (0x00000001) as described in section
//   2.2.1.2.1.

    void recv_connection_initial(Transport * trans, Stream & data)
    {
        Stream stream(8192);
        X224In(trans, stream);

        if (stream.in_uint16_be() != BER_TAG_MCS_CONNECT_INITIAL) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        int len = stream.in_ber_len();
        if (stream.in_uint8() != BER_TAG_OCTET_STRING) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
        stream.skip_uint8(len);

        if (stream.in_uint8() != BER_TAG_OCTET_STRING) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
        stream.skip_uint8(len);
        if (stream.in_uint8() != BER_TAG_BOOLEAN) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
        stream.skip_uint8(len);

        if (stream.in_uint8() != BER_TAG_MCS_DOMAIN_PARAMS) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
        stream.skip_uint8(len);

        if (stream.in_uint8() != BER_TAG_MCS_DOMAIN_PARAMS) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
        stream.skip_uint8(len);

        if (stream.in_uint8() != BER_TAG_MCS_DOMAIN_PARAMS) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
        stream.skip_uint8(len);

        if (stream.in_uint8() != BER_TAG_OCTET_STRING) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();

        /* make a copy of client mcs data */
        data.init(len);
        data.out_copy_bytes(stream.p, len);
        data.mark_end();
        stream.skip_uint8(len);
    }

    void send_connect_response(Stream & data, Transport * trans) throw(Error)
    {
//        LOG(LOG_INFO, send_connect_response");
        #warning why don't we build directly in final data buffer ? Instead of building in data and copying in stream ?
        Stream stream(8192);
        X224Out tpdu(X224Packet::DT_TPDU, stream);

        int data_len = data.end - data.data;
        stream.out_uint16_be(BER_TAG_MCS_CONNECT_RESPONSE);
        stream.out_ber_len(data_len + 38);

        stream.out_uint8(BER_TAG_RESULT);
        stream.out_uint8(1);
        stream.out_uint8(0);

        stream.out_uint8(BER_TAG_INTEGER);
        stream.out_uint8(1);
        stream.out_uint8(0);

        stream.out_uint8(BER_TAG_MCS_DOMAIN_PARAMS);
        stream.out_uint8(26);
        stream.out_ber_int8(22); // max_channels
        stream.out_ber_int8(3); // max_users
        stream.out_ber_int8(0); // max_tokens
        stream.out_ber_int8(1);
        stream.out_ber_int8(0);
        stream.out_ber_int8(1);
        stream.out_ber_int24(0xfff8); // max_pdu_size
        stream.out_ber_int8(2);

        stream.out_uint8(BER_TAG_OCTET_STRING);
        stream.out_ber_len(data_len);
        /* mcs data */
        stream.out_copy_bytes(data.data, data_len);

        tpdu.end();
        tpdu.send(trans);
    }


    /*****************************************************************************/
    /* prepare server mcs data to send in mcs layer */
    void server_sec_out_mcs_data(Stream & stream, ClientInfo * client_info)
    {
        /* Same code above using list_test */
        int num_channels = (int) this->channel_list.size();
        int num_channels_even = num_channels + (num_channels & 1);

        this->data.init(512);

        this->data.out_uint16_be(5);
        this->data.out_uint16_be(0x14);
        this->data.out_uint8(0x7c);
        this->data.out_uint16_be(1);
        this->data.out_uint8(0x2a);
        this->data.out_uint8(0x14);
        this->data.out_uint8(0x76);
        this->data.out_uint8(0x0a);
        this->data.out_uint8(1);
        this->data.out_uint8(1);
        this->data.out_uint8(0);
        this->data.out_uint16_le(0xc001);
        this->data.out_uint8(0);
        this->data.out_uint8(0x4d); /* M */
        this->data.out_uint8(0x63); /* c */
        this->data.out_uint8(0x44); /* D */
        this->data.out_uint8(0x6e); /* n */
        this->data.out_uint16_be(0x80fc + (num_channels_even * 2));
        this->data.out_uint16_le(SEC_TAG_SRV_INFO);
        this->data.out_uint16_le(8); /* len */
        this->data.out_uint8(4); /* 4 = rdp5 1 = rdp4 */
        this->data.out_uint8(0);
        this->data.out_uint8(8);
        this->data.out_uint8(0);
        this->data.out_uint16_le(SEC_TAG_SRV_CHANNELS);
        this->data.out_uint16_le(8 + (num_channels_even * 2)); /* len */
        this->data.out_uint16_le(MCS_GLOBAL_CHANNEL); /* 1003, 0x03eb main channel */
        this->data.out_uint16_le(num_channels); /* number of other channels */

        for (int index = 0; index < num_channels_even; index++) {
            if (index < num_channels) {
                this->data.out_uint16_le(MCS_GLOBAL_CHANNEL + (index + 1));
            } else {
                this->data.out_uint16_le( 0);
            }
        }
        this->data.out_uint16_le(SEC_TAG_SRV_CRYPT);
        this->data.out_uint16_le(0x00ec); /* len is 236 */
        this->data.out_uint32_le(this->rc4_key_size); /* key len 1 = 40 bit 2 = 128 bit */
        this->data.out_uint32_le(client_info->crypt_level); /* crypt level 1 = low 2 = medium */
        /* 3 = high */
        this->data.out_uint32_le(32);     /* 32 bytes random len */
        this->data.out_uint32_le(0xb8);   /* 184 bytes rsa info(certificate) len */
        this->data.out_copy_bytes(this->server_random, 32);
        /* here to end is certificate */
        /* HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\ */
        /* TermService\Parameters\Certificate */
        this->data.out_uint32_le(1);
        this->data.out_uint32_le(1);
        this->data.out_uint32_le(1);
        this->data.out_uint16_le(SEC_TAG_PUBKEY);
        this->data.out_uint16_le(0x005c); /* 92 bytes length of SEC_TAG_PUBKEY */
        this->data.out_uint32_le(SEC_RSA_MAGIC);
        this->data.out_uint32_le(0x48); /* 72 bytes modulus len */
        this->data.out_uint32_be(0x00020000);
        this->data.out_uint32_be(0x3f000000);
        this->data.out_copy_bytes(this->pub_exp, 4); /* pub exp */
        this->data.out_copy_bytes(this->pub_mod, 64); /* pub mod */
        this->data.out_clear_bytes(8); /* pad */
        this->data.out_uint16_le(SEC_TAG_KEYSIG);
        this->data.out_uint16_le(72); /* len */
        this->data.out_copy_bytes(this->pub_sig, 64); /* pub sig */
        this->data.out_clear_bytes(8); /* pad */
        /* end certificate */
        this->data.mark_end();
    }


    void rdp_lic_generate_hwid(uint8_t* hwid, const char * hostname)
    {
        buf_out_uint32(hwid, 2);
        memcpy(hwid + 4, hostname, LICENCE_HWID_SIZE - 4);
    }

    void rdp_lic_process_authreq(Transport * trans, Stream & stream, const char * hostname)
    {

        ssllib ssl;

        const uint8_t* in_token;
        const uint8_t* in_sig;
        uint8_t out_token[LICENCE_TOKEN_SIZE];
        uint8_t decrypt_token[LICENCE_TOKEN_SIZE];
        uint8_t hwid[LICENCE_HWID_SIZE];
        uint8_t crypt_hwid[LICENCE_HWID_SIZE];
        uint8_t sealed_buffer[LICENCE_TOKEN_SIZE + LICENCE_HWID_SIZE];
        uint8_t out_sig[LICENCE_SIGNATURE_SIZE];

        in_token = 0;
        in_sig = 0;
        /* Parse incoming packet and save the encrypted token */
        stream.skip_uint8(6); /* unknown: f8 3d 15 00 04 f6 */

        int tokenlen = stream.in_uint16_le();
        if (tokenlen != LICENCE_TOKEN_SIZE) {
            LOG(LOG_ERR, "token len = %d, expected %d\n", tokenlen, LICENCE_TOKEN_SIZE);
        }
        else{
            in_token = stream.in_uint8p(tokenlen);
            in_sig = stream.in_uint8p(LICENCE_SIGNATURE_SIZE);
            stream.check_end();
        }

        memcpy(out_token, in_token, LICENCE_TOKEN_SIZE);
        /* Decrypt the token. It should read TEST in Unicode. */
        SSL_RC4 crypt_key;
        ssl.rc4_set_key(crypt_key, this->lic_layer.licence_key, 16);
        memcpy(decrypt_token, in_token, LICENCE_TOKEN_SIZE);
        ssl.rc4_crypt(crypt_key, decrypt_token, decrypt_token, LICENCE_TOKEN_SIZE);
        /* Generate a signature for a buffer of token and HWID */
        this->rdp_lic_generate_hwid(hwid, hostname);
        memcpy(sealed_buffer, decrypt_token, LICENCE_TOKEN_SIZE);
        memcpy(sealed_buffer + LICENCE_TOKEN_SIZE, hwid, LICENCE_HWID_SIZE);
        sec_sign(out_sig, 16, this->lic_layer.licence_sign_key, 16, sealed_buffer, sizeof(sealed_buffer));
        /* Now encrypt the HWID */
        ssl.rc4_set_key(crypt_key, this->lic_layer.licence_key, 16);
        memcpy(crypt_hwid, hwid, LICENCE_HWID_SIZE);
        ssl.rc4_crypt(crypt_key, crypt_hwid, crypt_hwid, LICENCE_HWID_SIZE);

        this->rdp_lic_send_authresp(trans, out_token, crypt_hwid, out_sig);
    }

    void rdp_lic_send_authresp(Transport * trans, uint8_t* token, uint8_t* crypt_hwid, uint8_t* signature)
    {
        int length = 58;

        Stream stream(8192);
        X224Out tpdu(X224Packet::DT_TPDU, stream);
        McsOut sdrq_out(stream, MCS_SDRQ, this->userid, MCS_GLOBAL_CHANNEL);

        stream.out_uint8(this->lic_layer.licence_issued
                         ? LICENCE_TAG_AUTHRESP
                         : SEC_LICENCE_NEG);

        stream.out_uint8(2); /* version */
        stream.out_uint16_le(length);
        stream.out_uint16_le(1);
        stream.out_uint16_le(LICENCE_TOKEN_SIZE);
        stream.out_copy_bytes(token, LICENCE_TOKEN_SIZE);
        stream.out_uint16_le(1);
        stream.out_uint16_le(LICENCE_HWID_SIZE);
        stream.out_copy_bytes(crypt_hwid, LICENCE_HWID_SIZE);
        stream.out_copy_bytes(signature, LICENCE_SIGNATURE_SIZE);

        sdrq_out.end();
        tpdu.end();

        tpdu.send(trans);
    }

    void rdp_lic_process_demand(Transport * trans, Stream & stream, const char * hostname, const char * username)
    {
        uint8_t null_data[SEC_MODULUS_SIZE];
        uint8_t signature[LICENCE_SIGNATURE_SIZE];
        uint8_t hwid[LICENCE_HWID_SIZE];
        uint8_t* licence_data;

        licence_data = 0;
        /* Retrieve the server random from the incoming packet */
        const uint8_t * server_random = stream.in_uint8p(SEC_RANDOM_SIZE);

        // RDP licence generate key
        {
            /* We currently use null client keys. This is a bit naughty but, hey,
               the security of licence negotiation isn't exactly paramount. */
            memset(null_data, 0, sizeof(null_data));
            uint8_t* client_random = null_data;
            uint8_t* pre_master_secret = null_data;
            uint8_t master_secret[48];
            uint8_t key_block[48];

            /* Generate master secret and then key material */
            this->sec_hash_48(master_secret, pre_master_secret, client_random, server_random, 65);
            this->sec_hash_48(key_block, master_secret, server_random, client_random, 65);
            /* Store first 16 bytes of session key as MAC secret */
            memcpy(this->lic_layer.licence_sign_key, key_block, 16);
            /* Generate RC4 key from next 16 bytes */
            this->sec_hash_16(this->lic_layer.licence_key, key_block + 16, client_random, server_random);
        }

        if (this->licence_size > 0) {
            /* Generate a signature for the HWID buffer */
            this->rdp_lic_generate_hwid(hwid, hostname);
            sec_sign(signature, 16, this->lic_layer.licence_sign_key, 16, hwid, sizeof(hwid));
            /* Now encrypt the HWID */
            ssllib ssl;

            SSL_RC4 crypt_key;
            ssl.rc4_set_key(crypt_key, this->lic_layer.licence_key, 16);
            ssl.rc4_crypt(crypt_key, hwid, hwid, sizeof(hwid));

            this->rdp_lic_present(trans, null_data, null_data,
                                  this->licence_data,
                                  this->licence_size,
                                  hwid, signature);
        }
        else {
            this->rdp_lic_send_request(trans, null_data, null_data, hostname, username);
        }
    }

    void rdp_lic_send_request(Transport * trans, uint8_t* client_random, uint8_t* rsa_data, const char * hostname, const char * username)
    {
        int userlen = strlen(username) + 1;
        int hostlen = strlen(hostname) + 1;
        int length = 128 + userlen + hostlen;

        Stream stream(8192);
        X224Out tpdu(X224Packet::DT_TPDU, stream);
        McsOut sdrq_out(stream, MCS_SDRQ, this->userid, MCS_GLOBAL_CHANNEL);

        #warning if we are performing licence request doesn't it mean that licence has not been issued ?
        stream.out_uint8(this->lic_layer.licence_issued?LICENCE_TAG_REQUEST:SEC_LICENCE_NEG);
        stream.out_uint8(2); /* version */
        stream.out_uint16_le(length);
        stream.out_uint32_le(1);
        stream.out_uint16_le(0);
        stream.out_uint16_le(0xff01);
        stream.out_copy_bytes(client_random, SEC_RANDOM_SIZE);
        stream.out_uint16_le(0);
        stream.out_uint16_le((SEC_MODULUS_SIZE + SEC_PADDING_SIZE));
        stream.out_copy_bytes(rsa_data, SEC_MODULUS_SIZE);
        stream.out_clear_bytes(SEC_PADDING_SIZE);

        stream.out_uint16_le(LICENCE_TAG_USER);
        stream.out_uint16_le(userlen);
        stream.out_copy_bytes(username, userlen);

        stream.out_uint16_le(LICENCE_TAG_HOST);
        stream.out_uint16_le(hostlen);
        stream.out_copy_bytes(hostname, hostlen);

        sdrq_out.end();
        tpdu.end();
        tpdu.send(trans);
    }

    void rdp_lic_present(Transport * trans, uint8_t* client_random, uint8_t* rsa_data,
                uint8_t* licence_data, int licence_size, uint8_t* hwid,
                uint8_t* signature)
    {
        Stream stream(8192);
        X224Out tpdu(X224Packet::DT_TPDU, stream);
        McsOut sdrq_out(stream, MCS_SDRQ, this->userid, MCS_GLOBAL_CHANNEL);

        int length = 16 + SEC_RANDOM_SIZE + SEC_MODULUS_SIZE + SEC_PADDING_SIZE +
                 licence_size + LICENCE_HWID_SIZE + LICENCE_SIGNATURE_SIZE;

        stream.out_uint8(this->lic_layer.licence_issued ?LICENCE_TAG_PRESENT:SEC_LICENCE_NEG);
        stream.out_uint8(2); /* version */
        stream.out_uint16_le(length);
        stream.out_uint32_le(1);
        stream.out_uint16_le(0);
        stream.out_uint16_le(0x0201);
        stream.out_copy_bytes(client_random, SEC_RANDOM_SIZE);
        stream.out_uint16_le(0);
        stream.out_uint16_le((SEC_MODULUS_SIZE + SEC_PADDING_SIZE));
        stream.out_copy_bytes(rsa_data, SEC_MODULUS_SIZE);
        stream.out_clear_bytes( SEC_PADDING_SIZE);
        stream.out_uint16_le(1);
        stream.out_uint16_le(licence_size);
        stream.out_copy_bytes(licence_data, licence_size);
        stream.out_uint16_le(1);
        stream.out_uint16_le(LICENCE_HWID_SIZE);
        stream.out_copy_bytes(hwid, LICENCE_HWID_SIZE);
        stream.out_copy_bytes(signature, LICENCE_SIGNATURE_SIZE);

        sdrq_out.end();
        tpdu.end();
        tpdu.send(trans);
    }

    #warning this is not supported yet, but using rdp_save_licence we would keep a local copy of the licence of a remote server thus avoiding to ask it every time we connect. Anyway the use of files to stoe licences should be abstracted.
    void rdp_save_licence(uint8_t *data, int length, const char * hostname)
    {
      int fd;
      char* path = NULL;
      char* tmppath = NULL;

      path = new char[256];
      /* TODO: verify if location that we've stablished is right or not */
      sprintf(path, "/etc/xrdp./xrdp/licence.%s", hostname);

      if ((mkdir(path, 0700) == -1))
      {
        if (errno != EEXIST){
          perror(path);
          return;
        }
      }

      /* write licence to licence.hostname.new and after rename to licence.hostname */

      sprintf(path, "/etc/xrdp./xrdp/licence.%s", hostname);
      tmppath = new char[256];
      strcpy(tmppath, path);
      strcat(tmppath, ".new");

      fd = open(tmppath, O_WRONLY | O_CREAT | O_TRUNC, 0600);

      if (fd == -1){
        perror(tmppath);
        return;
      }
      if (write(fd, data, length) != length){
        perror(tmppath);
        unlink(tmppath);
      }
      else if (rename(tmppath, path) == -1){
        printf("Error renaming licence file\n");
        unlink(tmppath);
      }
      close(fd);
      delete [] tmppath;
      delete [] path;
    }

    void rdp_lic_process_issue(Stream & stream, const char * hostname)
    {
        stream.skip_uint8(2); /* 3d 45 - unknown */
        int length = stream.in_uint16_le();
        if (!stream.check_rem(length)) {
            return;
        }
        ssllib ssl;
        SSL_RC4 crypt_key;
        ssl.rc4_set_key(crypt_key, this->lic_layer.licence_key, 16);
        ssl.rc4_crypt(crypt_key, stream.p, stream.p, length);
        int check = stream.in_uint16_le();
        if (check != 0) {
            return;
        }
        this->lic_layer.licence_issued = 1;
        stream.skip_uint8(2); /* pad */
        /* advance to fourth string */
        length = 0;
        for (int i = 0; i < 4; i++) {
            stream.skip_uint8(length);
            length = stream.in_uint32_le();
            if (!stream.check_rem(length)) {
                return;
            }
        }
        /* todo save_licence(stream.p, length); */
        this->rdp_save_licence(stream.p, length, hostname);
    }

    void rdp_lic_process(Transport * trans, Stream & stream, const char * hostname, const char * username)
    {
        uint8_t tag = stream.in_uint8();
        stream.skip_uint8(3); /* version, length */
        switch (tag) {
        case LICENCE_TAG_DEMAND:
            this->rdp_lic_process_demand(trans, stream, hostname, username);
            break;
        case LICENCE_TAG_AUTHREQ:
            this->rdp_lic_process_authreq(trans, stream, hostname);
            break;
        case LICENCE_TAG_ISSUE:
            this->rdp_lic_process_issue(stream, hostname);
            break;
        case LICENCE_TAG_REISSUE:
        case LICENCE_TAG_RESULT:
            break;
        default:
            break;
            /* todo unimpl("licence tag 0x%x\n", tag); */
        }
    }


    /*****************************************************************************/
    void rdp_sec_generate_keys(uint8_t *client_random, uint8_t *server_random, uint32_t rc4_key_size)
    {
        uint8_t pre_master_secret[48];
        uint8_t master_secret[48];
        uint8_t key_block[48];

        /* Construct pre-master secret (session key) */
        memcpy(pre_master_secret, client_random, 24);
        memcpy(pre_master_secret + 24, server_random, 24);

        /* Generate master secret and then key material */
        this->sec_hash_48(master_secret, pre_master_secret, client_random, server_random, 'A');
        this->sec_hash_48(key_block, master_secret, client_random, server_random, 'X');

        /* First 16 bytes of key material is MAC secret */
        memcpy(this->encrypt.sign_key, key_block, 16);

        /* Generate export keys from next two blocks of 16 bytes */
        this->sec_hash_16(this->decrypt.key, &key_block[16], client_random, server_random);
        this->sec_hash_16(this->encrypt.key, &key_block[32], client_random, server_random);

        if (rc4_key_size == 1) {
            // LOG(LOG_DEBUG, "40-bit encryption enabled\n");
            sec_make_40bit(this->encrypt.sign_key);
            sec_make_40bit(this->decrypt.key);
            sec_make_40bit(this->encrypt.key);
            this->decrypt.rc4_key_len = 8;
            this->encrypt.rc4_key_len = 8;
        }
        else {
            //LOG(LOG_DEBUG, "rc_4_key_size == %d, 128-bit encryption enabled\n", rc4_key_size);
            this->decrypt.rc4_key_len = 16;
            this->encrypt.rc4_key_len = 16;
        }

        /* Save initial RC4 keys as update keys */
        memcpy(this->decrypt.update_key, this->decrypt.key, 16);
        memcpy(this->encrypt.update_key, this->encrypt.key, 16);

        ssllib ssl;

        ssl.rc4_set_key(this->decrypt.rc4_info, this->decrypt.key, this->decrypt.rc4_key_len);
        ssl.rc4_set_key(this->encrypt.rc4_info, this->encrypt.key, this->encrypt.rc4_key_len);
    }

    /* Perform an RSA public key encryption operation */
    static void rdp_sec_rsa_encrypt(uint8_t * out, uint8_t * in, uint8_t len, uint32_t modulus_size, uint8_t * modulus, uint8_t * exponent)
    {
        ssllib ssl;

        ssl.rsa_encrypt(out, in, len, modulus_size, modulus, exponent);
    }

    /* Parse a public key structure */
    void rdp_sec_parse_public_key(Stream & stream, uint8_t* modulus, uint8_t* exponent)
    {
        uint32_t magic;
        uint32_t modulus_len;

        magic = stream.in_uint32_le();
        if (magic != SEC_RSA_MAGIC) {
            LOG(LOG_WARNING, "RSA magic 0x%x\n", magic);
            throw Error(ERR_SEC_PARSE_PUB_KEY_MAGIC_NOT_OK);
        }
        modulus_len = stream.in_uint32_le();
        modulus_len -= SEC_PADDING_SIZE;

        if ((modulus_len < SEC_MODULUS_SIZE)
        ||  (modulus_len > SEC_MAX_MODULUS_SIZE)) {
            LOG(LOG_WARNING, "Bad server public key size (%u bits)\n", modulus_len * 8);
            throw Error(ERR_SEC_PARSE_PUB_KEY_MODUL_NOT_OK);
        }
        stream.skip_uint8(8); /* modulus_bits, unknown */
        memcpy(exponent, stream.in_uint8p(SEC_EXPONENT_SIZE), SEC_EXPONENT_SIZE);
        memcpy(modulus, stream.in_uint8p(modulus_len), modulus_len);
        stream.skip_uint8(SEC_PADDING_SIZE);
        this->server_public_key_len = modulus_len;

        if (!stream.check()){
            throw Error(ERR_SEC_PARSE_PUB_KEY_ERROR_CHECKING_STREAM);
        }
    }


    /* Parse a public key structure */
    int rdp_sec_parse_public_sig(Stream & stream, int len, uint8_t* modulus, uint8_t* exponent)
    {
        uint8_t signature[SEC_MAX_MODULUS_SIZE];
        uint32_t sig_len;

        #warning check that. Why is it ok if signature len is not of the right size ?
        #warning Use Exception instead of return value for error cases.
        if (len != 72){
            return 1;
        }
        memset(signature, 0, sizeof(signature));
        sig_len = len - 8;
        memcpy(signature, stream.in_uint8p(sig_len), sig_len);
        return ssl_sig_ok(exponent, SEC_EXPONENT_SIZE,
                        modulus, this->server_public_key_len,
                        signature, sig_len);
    }


    /* Parse a crypto information structure */
    int rdp_sec_parse_crypt_info(Stream & stream, uint32_t *rc4_key_size,
                                  uint8_t * server_random,
                                  uint8_t* modulus, uint8_t* exponent)
    {
        uint32_t random_len;
        uint32_t rsa_info_len;
        uint32_t cacert_len;
        uint32_t cert_len;
        uint32_t flags;
        SSL_CERT *cacert;
        SSL_CERT *server_cert;
        SSL_RKEY *server_public_key;
        uint16_t tag;
        uint16_t length;
        uint8_t* next_tag;
        uint8_t* end;

        *rc4_key_size = stream.in_uint32_le(); /* 1 = 40-bit, 2 = 128-bit */
        this->crypt_level = stream.in_uint32_le(); /* 1 = low, 2 = medium, 3 = high */
        if (this->crypt_level == 0) { /* no encryption */
            LOG(LOG_INFO, "No encryption");
            return 0;
        }
        random_len = stream.in_uint32_le();
        rsa_info_len = stream.in_uint32_le();
        if (random_len != SEC_RANDOM_SIZE) {
            LOG(LOG_ERR,
                "parse_crypt_info_error: random len %d, expected %d\n",
                random_len, SEC_RANDOM_SIZE);
            return 0;
        }
        memcpy(server_random, stream.in_uint8p(random_len), random_len);

        /* RSA info */
        end = stream.p + rsa_info_len;
        if (end > stream.end) {
            return 0;
        }

        flags = stream.in_uint32_le(); /* 1 = RDP4-style, 0x80000002 = X.509 */
        LOG(LOG_INFO, "crypt flags %x\n", flags);
        if (flags & 1) {

            LOG(LOG_DEBUG, "We're going for the RDP4-style encryption\n");
            stream.skip_uint8(8); /* unknown */

            while (stream.p < end) {
                tag = stream.in_uint16_le();
                length = stream.in_uint16_le();

                next_tag = stream.p + length;

                switch (tag) {
                case SEC_TAG_PUBKEY:
                    #warning exception style should be used throughout the code, not an horrible mixup as below
                    try {
                        this->rdp_sec_parse_public_key(stream, modulus, exponent);
                    }
                    catch (...) {
                        return 0;
                    }
                    LOG(LOG_DEBUG, "Got Public key, RDP4-style\n");
                    break;
                case SEC_TAG_KEYSIG:
                    LOG(LOG_DEBUG, "SEC_TAG_KEYSIG RDP4-style\n");
                    //if (!this->rdp_sec_parse_public_sig(stream, length, modulus, exponent)){
                    //    return 0;
                    //}
                    break;
                default:
                    LOG(LOG_DEBUG, "unimplemented: crypt tag 0x%x\n", tag);
                    return 0;
                    break;
                }
                stream.p = next_tag;
            }
        }
        else {
            try {
                LOG(LOG_DEBUG, "We're going for the RDP5-style encryption\n");
                LOG(LOG_DEBUG, "RDP5-style encryption with certificates not available\n");
                uint32_t certcount = stream.in_uint32_le();
                if (certcount < 2){
                    LOG(LOG_DEBUG, "Server didn't send enough X509 certificates\n");
                    throw Error(ERR_SEC_PARSE_CRYPT_INFO_CERT_NOK);
                }
                for (; certcount > 2; certcount--){
                    /* ignore all the certificates between the root and the signing CA */
                    LOG(LOG_WARNING, " Ignored certs left: %d\n", certcount);
                    uint32_t ignorelen = stream.in_uint32_le();
                    LOG(LOG_WARNING, "Ignored Certificate length is %d\n", ignorelen);
                    SSL_CERT *ignorecert = ssl_cert_read(stream.p, ignorelen);
                    stream.skip_uint8(ignorelen);
                    if (ignorecert == NULL){
                        LOG(LOG_WARNING,
                            "got a bad cert: this will probably screw up"
                            " the rest of the communication\n");
                    }
                }

                /* Do da funky X.509 stuffy

               "How did I find out about this?  I looked up and saw a
               bright light and when I came to I had a scar on my forehead
               and knew about X.500"
               - Peter Gutman in a early version of
               http://www.cs.auckland.ac.nz/~pgut001/pubs/x509guide.txt
               */

                /* Loading CA_Certificate from server*/
                cacert_len = stream.in_uint32_le();
                LOG(LOG_DEBUG, "CA Certificate length is %d\n", cacert_len);
                cacert = ssl_cert_read(stream.p, cacert_len);
                stream.skip_uint8(cacert_len);
                if (NULL == cacert){
                    LOG(LOG_DEBUG, "Couldn't load CA Certificate from server\n");
                    throw Error(ERR_SEC_PARSE_CRYPT_INFO_CACERT_NULL);
                }

                ssllib ssl;

                /* Loading Certificate from server*/
                cert_len = stream.in_uint32_le();
                LOG(LOG_DEBUG, "Certificate length is %d\n", cert_len);
                server_cert = ssl_cert_read(stream.p, cert_len);
                stream.skip_uint8(cert_len);
                if (NULL == server_cert){
                    ssl_cert_free(cacert);
                    LOG(LOG_DEBUG, "Couldn't load Certificate from server\n");
                    throw Error(ERR_SEC_PARSE_CRYPT_INFO_CACERT_NOT_LOADED);
                }
                /* Matching certificates */
                if (!ssl_certs_ok(server_cert,cacert)){
                    ssl_cert_free(server_cert);
                    ssl_cert_free(cacert);
                    LOG(LOG_DEBUG, "Security error CA Certificate invalid\n");
                    throw Error(ERR_SEC_PARSE_CRYPT_INFO_CACERT_NOT_MATCH);
                }
                ssl_cert_free(cacert);
                stream.skip_uint8(16); /* Padding */
                server_public_key = ssl_cert_to_rkey(server_cert, this->server_public_key_len);
                if (NULL == server_public_key){
                    LOG(LOG_DEBUG, "Didn't parse X509 correctly\n");
                    ssl_cert_free(server_cert);
                    throw Error(ERR_SEC_PARSE_CRYPT_INFO_X509_NOT_PARSED);

                }
                ssl_cert_free(server_cert);
                LOG(LOG_INFO, "server_public_key_len=%d, MODULUS_SIZE=%d MAX_MODULUS_SIZE=%d\n", this->server_public_key_len, SEC_MODULUS_SIZE, SEC_MAX_MODULUS_SIZE);
                if ((this->server_public_key_len < SEC_MODULUS_SIZE) ||
                    (this->server_public_key_len > SEC_MAX_MODULUS_SIZE)){
                    LOG(LOG_DEBUG, "Bad server public key size (%u bits)\n",
                        this->server_public_key_len * 8);
                    ssl.rkey_free(server_public_key);
                    throw Error(ERR_SEC_PARSE_CRYPT_INFO_MOD_SIZE_NOT_OK);
                }
                if (ssl_rkey_get_exp_mod(server_public_key, exponent, SEC_EXPONENT_SIZE,
                    modulus, SEC_MAX_MODULUS_SIZE) != 0){
                    LOG(LOG_DEBUG, "Problem extracting RSA exponent, modulus");
                    ssl.rkey_free(server_public_key);
                    throw Error(ERR_SEC_PARSE_CRYPT_INFO_RSA_EXP_NOT_OK);

                }
                ssl.rkey_free(server_public_key);
                return 1; /* There's some garbage here we don't care about */
            }
            catch (...){
                return 0;
            };
        }
        if (!stream.check_end()) {
            throw Error(ERR_SEC_PARSE_CRYPT_INFO_ERROR_CHECKING_STREAM);
        }
        return 1;
    }

    /*****************************************************************************/
    /* Process crypto information blob */
    void rdp_sec_process_crypt_info(Stream & stream)
    {
        uint8_t server_random[SEC_RANDOM_SIZE];
        uint8_t client_random[SEC_RANDOM_SIZE];
        uint8_t modulus[SEC_MAX_MODULUS_SIZE];
        uint8_t exponent[SEC_EXPONENT_SIZE];
        uint32_t rc4_key_size;

        memset(modulus, 0, sizeof(modulus));
        memset(exponent, 0, sizeof(exponent));
        memset(client_random, 0, sizeof(SEC_RANDOM_SIZE));
        #warning check for the true size
        memset(server_random, 0, SEC_RANDOM_SIZE);
        if (!this->rdp_sec_parse_crypt_info(stream, &rc4_key_size, server_random, modulus, exponent)){
            return;
        }
        /* Generate a client random, and determine encryption keys */
        int fd;

        memset(client_random, 0x44, SEC_RANDOM_SIZE);
        fd = open("/dev/urandom", O_RDONLY);
        if (fd == -1) {
            fd = open("/dev/random", O_RDONLY);
        }
        if (fd != -1) {
            if (read(fd, client_random, SEC_RANDOM_SIZE) != SEC_RANDOM_SIZE) {
                LOG(LOG_WARNING, "random source failed to provide random data\n");
            }
            close(fd);
        }
        else {
            LOG(LOG_WARNING, "random source failed to provide random data : couldn't open device\n");
        }
        #warning see order of parameters, (always buffer len of len, buffer, but not both)
        this->rdp_sec_rsa_encrypt(this->client_crypt_random, client_random, SEC_RANDOM_SIZE, this->server_public_key_len, modulus, exponent);
        this->rdp_sec_generate_keys(client_random, server_random, rc4_key_size);
    }




//    /*****************************************************************************/
//    static void rdp_sec_rsa_op(uint8_t* out, uint8_t* in, uint8_t* mod, uint8_t* exp)
//    {
//        ssl_mod_exp(out, SEC_MODULUS_SIZE, /* 64 */
//                    in, SEC_RANDOM_SIZE, /* 32 */
//                    mod, SEC_MODULUS_SIZE, /* 64 */
//                    exp, SEC_EXPONENT_SIZE); /* 4 */
//    }



    /* Process connect response data blob */
    void rdp_sec_process_mcs_data(Stream & stream, vector<mcs_channel_item*> channel_list, int & use_rdp5)
    {
        stream.skip_uint8(21); /* header (T.124 ConferenceCreateResponse) */
        uint8_t len = stream.in_uint8();

        if (len & 0x80) {
            len = stream.in_uint8();
        }
        while (stream.p < stream.end) {
            uint16_t tag = stream.in_uint16_le();
            uint16_t length = stream.in_uint16_le();
            if (length <= 4) {
                return;
            }
            uint8_t *next_tag = (stream.p + length) - 4;
            switch (tag) {
            case SEC_TAG_SRV_INFO:
                this->rdp_sec_process_srv_info(stream, use_rdp5);
                break;
            case SEC_TAG_SRV_CRYPT:
                this->rdp_sec_process_crypt_info(stream);
                break;
            case SEC_TAG_SRV_CHANNELS:
            /*  This is what rdesktop says in comment:
                FIXME: We should parse this information and
                use it to map RDP5 channels to MCS
                channels
                rdesktop does not call the function below
            */
                #warning rdesktop does not call the function below
                 this->rdp_sec_process_srv_channels(stream, channel_list);
                break;
            default:
                LOG(LOG_WARNING, "response tag 0x%x\n", tag);
                break;
            }
            stream.p = next_tag;
        }
    }

    void recv_connect_response(Stream & stream, Transport * trans) throw(Error)
    {
        X224In(trans, stream);
        if (stream.in_uint16_be() != BER_TAG_MCS_CONNECT_RESPONSE) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        int len = stream.in_ber_len();

        if (stream.in_uint8() != BER_TAG_RESULT) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();

        int res = stream.in_uint8();

        if (res != 0) {
            throw Error(ERR_MCS_RECV_CONNECTION_REP_RES_NOT_0);
        }
        if (stream.in_uint8() != BER_TAG_INTEGER) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
        stream.skip_uint8(len); /* connect id */

        if (stream.in_uint8() != BER_TAG_MCS_DOMAIN_PARAMS) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
        stream.skip_uint8(len);

        if (stream.in_uint8() != BER_TAG_OCTET_STRING) {
            throw Error(ERR_MCS_BER_HEADER_UNEXPECTED_TAG);
        }
        len = stream.in_ber_len();
    }

// 2.2.1.4  Server MCS Connect Response PDU with GCC Conference Create Response
// ----------------------------------------------------------------------------
// The MCS Connect Response PDU is an RDP Connection Sequence PDU sent from
// server to client during the Basic Settings Exchange phase (see section
// 1.3.1.1). It is sent as a response to the MCS Connect Initial PDU (section
// 2.2.1.3). The MCS Connect Response PDU encapsulates a GCC Conference Create
// Response, which encapsulates concatenated blocks of settings data.

// A basic high-level overview of the nested structure for the Server MCS
// Connect Response PDU is illustrated in section 1.3.1.1, in the figure
// specifying MCS Connect Response PDU. Note that the order of the settings
// data blocks is allowed to vary from that shown in the previously mentioned
// figure and the message syntax layout that follows. This is possible because
// each data block is identified by a User Data Header structure (section
// 2.2.1.4.1).

// tpktHeader (4 bytes): A TPKT Header, as specified in [T123] section 8.

// x224Data (3 bytes): An X.224 Class 0 Data TPDU, as specified in [X224]
// section 13.7.

// mcsCrsp (variable): Variable-length BER-encoded MCS Connect Response
//   structure (using definite-length encoding) as described in [T125]
//   (the ASN.1 structure definition is detailed in [T125] section 7, part 2).
//   The userData field of the MCS Connect Response encapsulates the GCC
//   Conference Create Response data (contained in the gccCCrsp and subsequent
//   fields).

// gccCCrsp (variable): Variable-length PER-encoded GCC Connect Data structure
//   which encapsulates a Connect GCC PDU that contains a GCC Conference Create
//   Response structure as described in [T124] (the ASN.1 structure definitions
//   are specified in [T124] section 8.7) appended as user data to the MCS
//   Connect Response (using the format specified in [T124] sections 9.5 and
//   9.6). The userData field of the GCC Conference Create Response contains
//   one user data set consisting of concatenated server data blocks.

// serverCoreData (12 bytes): Server Core Data structure (section 2.2.1.4.2).

// serverSecurityData (variable): Variable-length Server Security Data structure
//   (section 2.2.1.4.3).

// serverNetworkData (variable): Variable-length Server Network Data structure
//   (section 2.2.1.4.4).


// 2.2.1.3.2 Client Core Data (TS_UD_CS_CORE)
// ------------------------------------------

//The TS_UD_CS_CORE data block contains core client connection-related
// information.

//header (4 bytes): GCC user data block header, as specified in section
//                  2.2.1.3.1. The User Data Header type field MUST be set to
//                  CS_CORE (0xC001).

// version (4 bytes): A 32-bit, unsigned integer. Client version number for the
//                    RDP. The major version number is stored in the high 2
//                    bytes, while the minor version number is stored in the
//                    low 2 bytes.
// +------------+------------------------------------+
// |   Value    |    Meaning                         |
// +------------+------------------------------------+
// | 0x00080001 | RDP 4.0 clients                    |
// +------------+------------------------------------+
// | 0x00080004 | RDP 5.0, 5.1, 5.2, and 6.0 clients |
// +------------+------------------------------------+

// desktopWidth (2 bytes): A 16-bit, unsigned integer. The requested desktop
//                         width in pixels (up to a maximum value of 4096
//                         pixels).

// desktopHeight (2 bytes): A 16-bit, unsigned integer. The requested desktop
//                          height in pixels (up to a maximum value of 2048
//                          pixels).

// colorDepth (2 bytes): A 16-bit, unsigned integer. The requested color depth.
//                       Values in this field MUST be ignored if the
//                       postBeta2ColorDepth field is present.
// +--------------------------+-------------------------+
// |     Value                |        Meaning          |
// +--------------------------+-------------------------+
// | 0xCA00 RNS_UD_COLOR_4BPP | 4 bits-per-pixel (bpp)  |
// +--------------------------+-------------------------+
// | 0xCA01 RNS_UD_COLOR_8BPP | 8 bpp                   |
// +--------------------------+-------------------------+

// SASSequence (2 bytes): A 16-bit, unsigned integer. Secure access sequence.
//                        This field SHOULD be set to RNS_UD_SAS_DEL (0xAA03).

// keyboardLayout (4 bytes): A 32-bit, unsigned integer. Keyboard layout (active
//                           input locale identifier). For a list of possible
//                           input locales, see [MSDN-MUI].

// clientBuild (4 bytes): A 32-bit, unsigned integer. The build number of the
//                        client.

// clientName (32 bytes): Name of the client computer. This field contains up to
//                        15 Unicode characters plus a null terminator.

// keyboardType (4 bytes): A 32-bit, unsigned integer. The keyboard type.
// +-------+--------------------------------------------+
// | Value |              Meaning                       |
// +-------+--------------------------------------------+
// |   1   | IBM PC/XT or compatible (83-key) keyboard  |
// +-------+--------------------------------------------+
// |   2   | Olivetti "ICO" (102-key) keyboard          |
// +-------+--------------------------------------------+
// |   3   | IBM PC/AT (84-key) and similar keyboards   |
// +-------+--------------------------------------------+
// |   4   | IBM enhanced (101- or 102-key) keyboard    |
// +-------+--------------------------------------------+
// |   5   | Nokia 1050 and similar keyboards           |
// +-------+--------------------------------------------+
// |   6   | Nokia 9140 and similar keyboards           |
// +-------+--------------------------------------------+
// |   7   | Japanese keyboard                          |
// +-------+--------------------------------------------+

// keyboardSubType (4 bytes): A 32-bit, unsigned integer. The keyboard subtype
//                            (an original equipment manufacturer-dependent
//                            value).

// keyboardFunctionKey (4 bytes): A 32-bit, unsigned integer. The number of
//                                function keys on the keyboard.

// imeFileName (64 bytes): A 64-byte field. The Input Method Editor (IME) file
//                         name associated with the input locale. This field
//                         contains up to 31 Unicode characters plus a null
//                         terminator.

// postBeta2ColorDepth (2 bytes): A 16-bit, unsigned integer. The requested
//                                color depth. Values in this field MUST be
//                                ignored if the highColorDepth field is
//                                present.
// +--------------------------+-------------------------+
// |      Value               |         Meaning         |
// +--------------------------+-------------------------+
// | 0xCA00 RNS_UD_COLOR_4BPP | 4 bits-per-pixel (bpp)  |
// +--------------------------+-------------------------+
// | 0xCA01 RNS_UD_COLOR_8BPP | 8 bpp                   |
// +--------------------------+-------------------------+
// If this field is present, then all of the preceding fields MUST also be
// present. If this field is not present, then none of the subsequent fields
// MUST be present.

// clientProductId (2 bytes): A 16-bit, unsigned integer. The client product ID.
//                            This field SHOULD be initialized to 1. If this
//                            field is present, then all of the preceding fields
//                            MUST also be present. If this field is not
//                            present, then none of the subsequent fields MUST
//                            be present.

// serialNumber (4 bytes): A 32-bit, unsigned integer. Serial number. This field
//                         SHOULD be initialized to 0. If this field is present,
//                         then all of the preceding fields MUST also be
//                         present. If this field is not present, then none of
//                         the subsequent fields MUST be present.

// highColorDepth (2 bytes): A 16-bit, unsigned integer. The requested color
//                           depth.
// +-------+-------------------------------------------------------------------+
// | Value |                      Meaning                                      |
// +-------+-------------------------------------------------------------------+
// |     4 |   4 bpp                                                           |
// +-------+-------------------------------------------------------------------+
// |     8 |   8 bpp                                                           |
// +-------+-------------------------------------------------------------------+
// |    15 |  15-bit 555 RGB mask                                              |
// |       |  (5 bits for red, 5 bits for green, and 5 bits for blue)          |
// +-------+-------------------------------------------------------------------+
// |    16 |  16-bit 565 RGB mask                                              |
// |       |  (5 bits for red, 6 bits for green, and 5 bits for blue)          |
// +-------+-------------------------------------------------------------------+
// |    24 |  24-bit RGB mask                                                  |
// |       |  (8 bits for red, 8 bits for green, and 8 bits for blue)          |
// +-------+-------------------------------------------------------------------+
// If this field is present, then all of the preceding fields MUST also be
// present. If this field is not present, then none of the subsequent fields
// MUST be present.

// supportedColorDepths (2 bytes): A 16-bit, unsigned integer. Specifies the
//                                 high color depths that the client is capable
//                                 of supporting.
// +-----------------------------+---------------------------------------------+
// |          Flag               |                Meaning                      |
// +-----------------------------+---------------------------------------------+
// | 0x0001 RNS_UD_24BPP_SUPPORT | 24-bit RGB mask                             |
// |                             | (8 bits for red, 8 bits for green,          |
// |                             | and 8 bits for blue)                        |
// +-----------------------------+---------------------------------------------+
// | 0x0002 RNS_UD_16BPP_SUPPORT | 16-bit 565 RGB mask                         |
// |                             | (5 bits for red, 6 bits for green,          |
// |                             | and 5 bits for blue)                        |
// +-----------------------------+---------------------------------------------+
// | 0x0004 RNS_UD_15BPP_SUPPORT | 15-bit 555 RGB mask                         |
// |                             | (5 bits for red, 5 bits for green,          |
// |                             | and 5 bits for blue)                        |
// +-----------------------------+---------------------------------------------+
// | 0x0008 RNS_UD_32BPP_SUPPORT | 32-bit RGB mask                             |
// |                             | (8 bits for the alpha channel,              |
// |                             | 8 bits for red, 8 bits for green,           |
// |                             | and 8 bits for blue)                        |
// +-----------------------------+---------------------------------------------+
// If this field is present, then all of the preceding fields MUST also be
// present. If this field is not present, then none of the subsequent fields
// MUST be present.

// earlyCapabilityFlags (2 bytes): A 16-bit, unsigned integer. It specifies
// capabilities early in the connection sequence.
// +---------------------------------------------+-----------------------------|
// |                Flag                         |              Meaning        |
// +---------------------------------------------+-----------------------------|
// | 0x0001 RNS_UD_CS_SUPPORT_ERRINFO_PDU        | Indicates that the client   |
// |                                             | supports the Set Error Info |
// |                                             | PDU (section 2.2.5.1).      |
// +---------------------------------------------+-----------------------------|
// | 0x0002 RNS_UD_CS_WANT_32BPP_SESSION         | Indicates that the client is|
// |                                             | requesting a session color  |
// |                                             | depth of 32 bpp. This flag  |
// |                                             | is necessary because the    |
// |                                             | highColorDepth field does   |
// |                                             | not support a value of 32.  |
// |                                             | If this flag is set, the    |
// |                                             | highColorDepth field SHOULD |
// |                                             | be set to 24 to provide an  |
// |                                             | acceptable fallback for the |
// |                                             | scenario where the server   |
// |                                             | does not support 32 bpp     |
// |                                             | color.                      |
// +---------------------------------------------+-----------------------------|
// | 0x0004 RNS_UD_CS_SUPPORT_STATUSINFO_PDU     | Indicates that the client   |
// |                                             | supports the Server Status  |
// |                                             | Info PDU (section 2.2.5.2). |
// +---------------------------------------------+-----------------------------|
// | 0x0008 RNS_UD_CS_STRONG_ASYMMETRIC_KEYS     | Indicates that the client   |
// |                                             | supports asymmetric keys    |
// |                                             | larger than 512 bits for use|
// |                                             | with the Server Certificate |
// |                                             | (section 2.2.1.4.3.1) sent  |
// |                                             | in the Server Security Data |
// |                                             | block (section 2.2.1.4.3).  |
// +---------------------------------------------+-----------------------------|
// | 0x0020 RNS_UD_CS_RESERVED1                  | Reserved for future use.    |
// |                                             | This flag is ignored by the |
// |                                             | server.                     |
// +---------------------------------------------+-----------------------------+
// | 0x0040 RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU | Indicates that the client   |
// |                                             | supports the Monitor Layout |
// |                                             | PDU (section 2.2.12.1).     |
// +---------------------------------------------+-----------------------------|
// If this field is present, then all of the preceding fields MUST also be
// present. If this field is not present, then none of the subsequent fields
// MUST be present.

// clientDigProductId (64 bytes): Contains a value that uniquely identifies the
//                                client. If this field is present, then all of
//                                the preceding fields MUST also be present. If
//                                this field is not present, then none of the
//                                subsequent fields MUST be present.

// pad2octets (2 bytes): A 16-bit, unsigned integer. Padding to align the
//   serverSelectedProtocol field on the correct byte boundary.
// If this field is present, then all of the preceding fields MUST also be
// present. If this field is not present, then none of the subsequent fields
// MUST be present.

// serverSelectedProtocol (4 bytes): A 32-bit, unsigned integer. It contains the value returned
//   by the server in the selectedProtocol field of the RDP Negotiation Response structure
//   (section 2.2.1.2.1). In the event that an RDP Negotiation Response structure was not sent,
//   this field MUST be initialized to PROTOCOL_RDP (0). If this field is present, then all of the
//   preceding fields MUST also be present.

    /*****************************************************************************/
    /* Establish a secure connection */
    void rdp_sec_connect(Transport * trans, vector<mcs_channel_item*> channel_list,
                        int width, int height,
                        int rdp_bpp, int keylayout,
                        bool console_session, int & use_rdp5, char * hostname) throw(Error)
    {
        Stream out(65536);
        X224Out crtpdu(X224Packet::CR_TPDU, out);

        #warning looks like this strange cookie thing is in fact useless, see MSFT-SDLBTS
        // USER DATA
        // out.out_concat("Cookie: mstshash=");
        // out.out_concat(username);
        // out.out_concat("\r\n");
        // crtpdu.extend_tpdu_hdr();
        crtpdu.end();
        crtpdu.send(trans);

        Stream in;
        X224In cctpdu(trans, in);
        if (cctpdu.tpkt.version != 3){
            throw Error(ERR_T123_EXPECTED_TPKT_VERSION_3);
        }
        if (cctpdu.tpdu_hdr.code != X224Packet::CC_TPDU){
            throw Error(ERR_X224_EXPECTED_CONNECTION_CONFIRM);
        }

        {
            int length = 158 + 76 + 12 + 4;

            if (channel_list.size() > 0){
                length += channel_list.size() * 12 + 8;
            }

            /* Generic Conference Control (T.124) ConferenceCreateRequest */
            out.out_uint16_be(5);
            out.out_uint16_be(0x14);
            out.out_uint8(0x7c);
            out.out_uint16_be(1);

            out.out_uint16_be((length | 0x8000)); /* remaining length */

            out.out_uint16_be(8); /* length? */
            out.out_uint16_be(16);
            out.out_uint8(0);
            out.out_uint16_le(0xc001);
            out.out_uint8(0);

            out.out_uint32_le(0x61637544); /* OEM ID: "Duca", as in Ducati. */
            out.out_uint16_be(((length - 14) | 0x8000)); /* remaining length */

            /* Client information */
            out.out_uint16_le(CS_CORE);
            LOG(LOG_INFO, "Sending Client Core Data to remote server\n");
            out.out_uint16_le(212); /* length */
            LOG(LOG_INFO, "core::header::length = %u\n", 212);
            out.out_uint32_le(0x00080004); // RDP version. 1 == RDP4, 4 == RDP5.
            LOG(LOG_INFO, "core::header::version (0x00080004 = RDP 5.0, 5.1, 5.2, and 6.0 clients)");
            out.out_uint16_le(width);
            LOG(LOG_INFO, "core::desktopWidth = %u\n", width);
            out.out_uint16_le(height);
            LOG(LOG_INFO, "core::desktopHeight = %u\n", height);
            out.out_uint16_le(0xca01);
            LOG(LOG_INFO, "core::colorDepth = RNS_UD_COLOR_8BPP (superseded by postBeta2ColorDepth)");
            out.out_uint16_le(0xaa03);
            LOG(LOG_INFO, "core::SASSequence = RNS_UD_SAS_DEL");
            out.out_uint32_le(keylayout);
            LOG(LOG_INFO, "core::keyboardLayout = %x", keylayout);
            out.out_uint32_le(2600); /* Client build. We are now 2600 compatible :-) */
            LOG(LOG_INFO, "core::clientBuild = 2600");
            LOG(LOG_INFO, "core::clientName=%s\n", hostname);

            /* Added in order to limit hostlen and hostname size */
            int hostlen = 2 * strlen(hostname);
            if (hostlen > 30){
                hostlen = 30;
            }
            /* Unicode name of client, padded to 30 bytes */
            out.out_unistr(hostname);
            out.out_clear_bytes(30 - hostlen);

        /* See
        http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wceddk40/html/cxtsksupportingremotedesktopprotocol.asp */
        #warning code should be updated to take care of keyboard type
            out.out_uint32_le(4); // g_keyboard_type
            LOG(LOG_INFO, "core::keyboardType = IBM enhanced (101- or 102-key) keyboard");
            out.out_uint32_le(0); // g_keyboard_subtype
            LOG(LOG_INFO, "core::keyboardSubType = 0");
            out.out_uint32_le(12); // g_keyboard_functionkeys
            LOG(LOG_INFO, "core::keyboardFunctionKey = 12 function keys");
            out.out_clear_bytes(64); /* imeFileName */
            LOG(LOG_INFO, "core::imeFileName = \"\"");
            out.out_uint16_le(0xca01); /* color depth 8bpp */
            LOG(LOG_INFO, "core::postBeta2ColorDepth = RNS_UD_COLOR_8BPP (superseded by highColorDepth)");
            out.out_uint16_le(1);
            LOG(LOG_INFO, "core::clientProductId = 1");
            out.out_uint32_le(0);
            LOG(LOG_INFO, "core::serialNumber = 0");
            out.out_uint16_le(rdp_bpp);
            LOG(LOG_INFO, "core::highColorDepth = %u", rdp_bpp);
            out.out_uint16_le(0x0007);
            LOG(LOG_INFO, "core::supportedColorDepths = 24/16/15");
            out.out_uint16_le(1);
            LOG(LOG_INFO, "core::earlyCapabilityFlags = RNS_UD_CS_SUPPORT_ERRINFO_PDU");
            out.out_clear_bytes(64);
            LOG(LOG_INFO, "core::clientDigProductId = \"\"");
            out.out_clear_bytes(2);
            LOG(LOG_INFO, "core::pad2octets");
    //        out.out_uint32_le(0); // optional
    //        LOG(LOG_INFO, "core::serverSelectedProtocol = 0");
            /* End of client info */

            out.out_uint16_le(CS_CLUSTER);
            out.out_uint16_le(12);
            #warning check that should depend on g_console_session
            out.out_uint32_le(console_session ? 0xb : 9);
            out.out_uint32_le(0);

            /* Client encryption settings */
            out.out_uint16_le(CS_SECURITY);
            out.out_uint16_le(12); /* length */
            #warning check that, should depend on g_encryption
            /* encryption supported, 128-bit supported */
            out.out_uint32_le(0x3);
            out.out_uint32_le(0); /* Unknown */

            /* Here we need to put channel information in order to redirect channel data
            from client to server passing through the "proxy" */
            size_t num_channels = channel_list.size();

            if (num_channels > 0) {
                out.out_uint16_le(CS_NET);
                out.out_uint16_le(num_channels * 12 + 8); /* length */
                out.out_uint32_le(num_channels); /* number of virtual channels */
                for (size_t i = 0; i < num_channels; i++){
                    const mcs_channel_item* channel_item = channel_list[i];

                    LOG(LOG_DEBUG, "Requesting channel %s\n", channel_item->name);
                    memcpy(out.p, channel_item->name, 8);
                    out.p += 8;

                    out.out_uint32_be(channel_item->flags);
                }
            }
            out.mark_end();
        }

        try {

//2.2.1.1    Client X.224 Connection Request PDU
//==============================================

// The X.224 Connection Request PDU is an RDP Connection Sequence PDU sent from
// client to server during the Connection Initiation phase (see section
// 1.3.1.1).

// tpktHeader (4 bytes): A TPKT Header, as specified in [T123] section 8.

// x224Crq (7 bytes): An X.224 Class 0 Connection Request transport protocol
//   data unit (TPDU), as specified in [X224] section 13.3.

// routingToken (variable): Optional and variable-length routing token bytes
//   used for load balancing terminated by a carriage-return (CR) and line-feed
//   (LF) ANSI sequence. For more information, see [MSFT-SDLBTS]. The length of
//   the routing token and CR+LF sequence is included in the X.224 Connection
//   Request Length Indicator field.

// rdpNegData (8 bytes): An optional RDP Negotiation Request (section 2.2.1.1.1)
//   structure. The length of this negotiation structure is included in the
//   X.224 Connection Request Length Indicator field.


            Stream out;
            X224Out crtpdu(X224Packet::CR_TPDU, out);

            #warning looks like this strange cookie thing is in fact useless, see MSFT-SDLBTS
            // USER DATA
//            out.out_concat("Cookie: mstshash=");
//            out.out_concat(this->username);
//            out.out_concat("\r\n");
//            crtpdu.extend_tpdu_hdr();
            crtpdu.end();

            crtpdu.send(trans);

            Stream in;
            X224In cctpdu(trans, in);
            if (cctpdu.tpkt.version != 3){
                throw Error(ERR_T123_EXPECTED_TPKT_VERSION_3);
            }
            if (cctpdu.tpdu_hdr.code != X224Packet::CC_TPDU){
                throw Error(ERR_X224_EXPECTED_CONNECTION_CONFIRM);
            }
        } catch (Error) {
            try {
                trans->disconnect();
            } catch (Error){
                // rethrow the first error, not the error we could get disconnecting
            }
            throw;
        }
//2.2.1.1    Client X.224 Connection Request PDU
//==============================================

// The X.224 Connection Request PDU is an RDP Connection Sequence PDU sent from
// client to server during the Connection Initiation phase (see section
// 1.3.1.1).

// tpktHeader (4 bytes): A TPKT Header, as specified in [T123] section 8.

// x224Crq (7 bytes): An X.224 Class 0 Connection Request transport protocol
//   data unit (TPDU), as specified in [X224] section 13.3.

// routingToken (variable): Optional and variable-length routing token bytes
//   used for load balancing terminated by a carriage-return (CR) and line-feed
//   (LF) ANSI sequence. For more information, see [MSFT-SDLBTS]. The length of
//   the routing token and CR+LF sequence is included in the X.224 Connection
//   Request Length Indicator field.

// rdpNegData (8 bytes): An optional RDP Negotiation Request (section 2.2.1.1.1)
//   structure. The length of this negotiation structure is included in the
//   X.224 Connection Request Length Indicator field.


        try{
            {
                int data_len = out.end - out.data;
                int len = 7 + 3 * 34 + 4 + data_len;

                Stream stream(8192);
                X224Out tpdu(X224Packet::DT_TPDU, stream);

                stream.out_uint16_be(BER_TAG_MCS_CONNECT_INITIAL);
                stream.out_ber_len(len);
                stream.out_uint8(BER_TAG_OCTET_STRING);
                stream.out_ber_len(0); /* calling domain */
                stream.out_uint8(BER_TAG_OCTET_STRING);
                stream.out_ber_len(0); /* called domain */
                stream.out_uint8(BER_TAG_BOOLEAN);
                stream.out_ber_len(1);
                stream.out_uint8(0xff); /* upward flag */

                // target params
                stream.out_uint8(BER_TAG_MCS_DOMAIN_PARAMS);
                stream.out_ber_len(32);
                stream.out_ber_int16(34);     // max_channels
                stream.out_ber_int16(2);      // max_users
                stream.out_ber_int16(0);      // max_tokens
                stream.out_ber_int16(1);
                stream.out_ber_int16(0);
                stream.out_ber_int16(1);
                stream.out_ber_int16(0xffff); // max_pdu_size
                stream.out_ber_int16(2);

                // min params
                stream.out_uint8(BER_TAG_MCS_DOMAIN_PARAMS);
                stream.out_ber_len(32);
                stream.out_ber_int16(1);     // max_channels
                stream.out_ber_int16(1);     // max_users
                stream.out_ber_int16(1);     // max_tokens
                stream.out_ber_int16(1);
                stream.out_ber_int16(0);
                stream.out_ber_int16(1);
                stream.out_ber_int16(0x420); // max_pdu_size
                stream.out_ber_int16(2);

                // max params
                stream.out_uint8(BER_TAG_MCS_DOMAIN_PARAMS);
                stream.out_ber_len(32);
                stream.out_ber_int16(0xffff); // max_channels
                stream.out_ber_int16(0xfc17); // max_users
                stream.out_ber_int16(0xffff); // max_tokens
                stream.out_ber_int16(1);
                stream.out_ber_int16(0);
                stream.out_ber_int16(1);
                stream.out_ber_int16(0xffff); // max_pdu_size
                stream.out_ber_int16(2);

                stream.out_uint8(BER_TAG_OCTET_STRING);
                stream.out_ber_len(data_len);
                stream.out_copy_bytes(out.data, data_len);

                tpdu.end();
                tpdu.send(trans);
            }
            Stream stream(8192);
            this->recv_connect_response(stream, trans);

            LOG(LOG_INFO, "rdp_sec_process_mcs_data\n");
            this->rdp_sec_process_mcs_data(stream, channel_list, use_rdp5);

            {
                Stream stream(8192);
                X224Out tpdu(X224Packet::DT_TPDU, stream);
                stream.out_uint8((MCS_EDRQ << 2));
                stream.out_uint16_be(0x100); /* height */
                stream.out_uint16_be(0x100); /* interval */
                tpdu.end();
                tpdu.send(trans);
            }
            {
                Stream stream(8192);
                X224Out tpdu(X224Packet::DT_TPDU, stream);
                stream.out_uint8((MCS_AURQ << 2));
                tpdu.end();
                tpdu.send(trans);
            }
            {
                Stream stream(8192);
                X224In(trans, stream);
                int opcode = stream.in_uint8();
                if ((opcode >> 2) != MCS_AUCF) {
                    throw Error(ERR_MCS_RECV_AUCF_OPCODE_NOT_OK);
                }
                int res = stream.in_uint8();
                if (res != 0) {
                    throw Error(ERR_MCS_RECV_AUCF_RES_NOT_0);
                }
                if (opcode & 2) {
                    this->userid = stream.in_uint16_be();
                }
                if (!(stream.check_end())) {
                    throw Error(ERR_MCS_RECV_AUCF_ERROR_CHECKING_STREAM);
                }
            }

            #warning the array size below is arbitrary, it should be checked to avoid buffer overflow
            uint16_t channels[100];

            channels[0] = this->userid + 1001;
            channels[1] = MCS_GLOBAL_CHANNEL;
            size_t num_channels = this->channel_list.size();
            for (size_t index = 0; index < num_channels; index++){
                const mcs_channel_item* channel_item = this->channel_list[index];
                channels[2+index] = channel_item->chanid;
            }

            // 2.2.1.8 Client MCS Channel Join Request PDU
            // -------------------------------------------
            // The MCS Channel Join Request PDU is an RDP Connection Sequence PDU sent
            // from client to server during the Channel Connection phase (see section
            // 1.3.1.1). It is sent after receiving the MCS Attach User Confirm PDU
            // (section 2.2.1.7). The client uses the MCS Channel Join Request PDU to
            // join the user channel obtained from the Attach User Confirm PDU, the
            // I/O channel and all of the static virtual channels obtained from the
            // Server Network Data structure (section 2.2.1.4.4).

            // tpktHeader (4 bytes): A TPKT Header, as specified in [T123] section 8.

            // x224Data (3 bytes): An X.224 Class 0 Data TPDU, as specified in [X224]
            //                     section 13.7.

            // mcsCJrq (5 bytes): PER-encoded MCS Domain PDU which encapsulates an
            //                    MCS Channel Join Request structure as specified in
            //                    [T125] sections 10.19 and I.3 (the ASN.1 structure
            //                    definitions are given in [T125] section 7, parts 6
            //                    and 10).

            // ChannelJoinRequest ::= [APPLICATION 14] IMPLICIT SEQUENCE
            // {
            //     initiator UserId
            //     channelId ChannelId
            //               -- may be zero
            // }

            for (size_t index = 0; index < num_channels+2; index++){
                {
                    Stream stream(8192);
                    X224Out tpdu(X224Packet::DT_TPDU, stream);
                    stream.out_uint8((MCS_CJRQ << 2));
                    stream.out_uint16_be(this->userid);
                    stream.out_uint16_be(channels[index]);
                    tpdu.end();
                    tpdu.send(trans);
                }
                {
                    Stream stream(8192);
                    X224In(trans, stream);
                    int opcode = stream.in_uint8();
                    if ((opcode >> 2) != MCS_CJCF) {
                        throw Error(ERR_MCS_RECV_CJCF_OPCODE_NOT_CJCF);
                    }
                    if (0 != stream.in_uint8()) {
                        throw Error(ERR_MCS_RECV_CJCF_EMPTY);
                    }
                    stream.skip_uint8(4); /* mcs_userid, req_chanid */
                    if (opcode & 2) {
                        stream.skip_uint8(2); /* join_chanid */
                    }
                    if (!stream.check_end()) {
                        throw Error(ERR_MCS_RECV_CJCF_ERROR_CHECKING_STREAM);
                    }
                }
            }
        }
        catch(...){
            Stream stream(11);
            X224Out tpdu(X224Packet::DR_TPDU, stream);
            tpdu.end();
            tpdu.send(trans);
            throw;
        }

        LOG(LOG_INFO, "Iso Layer : setting encryption\n");
        /* Send the client random to the server */
//      if (this->encryption)
        {
            Stream stream(8192);
            X224Out tpdu(X224Packet::DT_TPDU, stream);
            McsOut sdrq_out(stream, MCS_SDRQ, this->userid, MCS_GLOBAL_CHANNEL);

            int length = this->server_public_key_len + SEC_PADDING_SIZE;
            stream.out_uint32_le(SEC_CLIENT_RANDOM);
            stream.out_uint32_le(length);
            LOG(LOG_INFO, "Server public key is %d bytes long", this->server_public_key_len);
            stream.out_copy_bytes(this->client_crypt_random, this->server_public_key_len);
            stream.out_clear_bytes(SEC_PADDING_SIZE);

            sdrq_out.end();
            tpdu.end();
            tpdu.send(trans);
        }
    }



    /* this adds the mcs channels in the list of channels to be used when creating the server mcs data */
    void rdp_sec_process_srv_channels(Stream & stream, vector<mcs_channel_item*> channel_list)
    {
        int base_channel = stream.in_uint16_le();
        size_t num_channels = stream.in_uint16_le();

        /* We assume that the channel_id array is confirmed in the same order
        that it has been sent. If there are any channels not confirmed, they're
        going to be the last channels on the array sent in MCS Connect Initial */
        for (size_t index = 0; index < num_channels; index++){
            mcs_channel_item *channel_item_cli = channel_list[index];
            #warning check matching delete, valgrind say memory leak
            mcs_channel_item *channel_item_srv = new mcs_channel_item;
            int chanid = stream.in_uint16_le();
            channel_item_srv->chanid = chanid;
            strcpy(channel_item_srv->name, channel_item_cli->name);
            channel_item_srv->flags = channel_item_cli->flags;
            this->channel_list.push_back(channel_item_srv);
        }
    }

    /******************************************************************************/

    /* TODO: this function is not working well because it is stopping copy / paste
       what is required is to stop data from server to client. What we need to do is
       to recover clip_flags, send it to rdp_process_redirect_pdu. After that, we
       need to pass this flags to session_send_to_channel and before doing the
       stream.out_uint8a(data, data_len), we need to do stream.out_uint16_le(clip_flags)*/

    int clipboard_check(char* name, bool clipboard)
    {
      if (!clipboard)
      {
        if (strcmp("cliprdr", name) == 0)
        {
          return 1;
        }
      }
      return 0;
    }

    /* Process SRV_INFO, find RDP version supported by server */
    void rdp_sec_process_srv_info(Stream & stream, int & use_rdp5)
    {
        uint16_t rdp_version = stream.in_uint16_le();
        LOG(LOG_DEBUG, "Server RDP version is %d\n", rdp_version);
        if (1 == rdp_version){ // can't use rdp5
            use_rdp5 = 0;
        #warning why caring of server_depth here ? Quite strange
        //        this->server_depth = 8;
        }
    }

    /* Output connect initial data blob */
    void rdp_sec_out_mcs_data(
            Stream & client_mcs_data,
            vector<mcs_channel_item*> channel_list,
            int width, int height, int rdp_bpp, int keylayout, bool console_session, char * hostname)
    {
        int length = 158 + 76 + 12 + 4;

        if (channel_list.size() > 0){
            length += channel_list.size() * 12 + 8;
        }

        /* Generic Conference Control (T.124) ConferenceCreateRequest */
        client_mcs_data.out_uint16_be(5);
        client_mcs_data.out_uint16_be(0x14);
        client_mcs_data.out_uint8(0x7c);
        client_mcs_data.out_uint16_be(1);

        client_mcs_data.out_uint16_be((length | 0x8000)); /* remaining length */

        client_mcs_data.out_uint16_be(8); /* length? */
        client_mcs_data.out_uint16_be(16);
        client_mcs_data.out_uint8(0);
        client_mcs_data.out_uint16_le(0xc001);
        client_mcs_data.out_uint8(0);

        client_mcs_data.out_uint32_le(0x61637544); /* OEM ID: "Duca", as in Ducati. */
        client_mcs_data.out_uint16_be(((length - 14) | 0x8000)); /* remaining length */

        /* Client information */
        client_mcs_data.out_uint16_le(CS_CORE);
        LOG(LOG_INFO, "Sending Client Core Data to remote server\n");
        client_mcs_data.out_uint16_le(212); /* length */
        LOG(LOG_INFO, "core::header::length = %u\n", 212);
        client_mcs_data.out_uint32_le(0x00080004); // RDP version. 1 == RDP4, 4 == RDP5.
        LOG(LOG_INFO, "core::header::version (0x00080004 = RDP 5.0, 5.1, 5.2, and 6.0 clients)");
        client_mcs_data.out_uint16_le(width);
        LOG(LOG_INFO, "core::desktopWidth = %u\n", width);
        client_mcs_data.out_uint16_le(height);
        LOG(LOG_INFO, "core::desktopHeight = %u\n", height);
        client_mcs_data.out_uint16_le(0xca01);
        LOG(LOG_INFO, "core::colorDepth = RNS_UD_COLOR_8BPP (superseded by postBeta2ColorDepth)");
        client_mcs_data.out_uint16_le(0xaa03);
        LOG(LOG_INFO, "core::SASSequence = RNS_UD_SAS_DEL");
        client_mcs_data.out_uint32_le(keylayout);
        LOG(LOG_INFO, "core::keyboardLayout = %x", keylayout);
        client_mcs_data.out_uint32_le(2600); /* Client build. We are now 2600 compatible :-) */
        LOG(LOG_INFO, "core::clientBuild = 2600");
        LOG(LOG_INFO, "core::clientName=%s\n", hostname);

        /* Added in order to limit hostlen and hostname size */
        int hostlen = 2 * strlen(hostname);
        if (hostlen > 30){
            hostlen = 30;
        }
        /* Unicode name of client, padded to 30 bytes */
        client_mcs_data.out_unistr(hostname);
        client_mcs_data.out_clear_bytes(30 - hostlen);

    /* See
    http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wceddk40/html/cxtsksupportingremotedesktopprotocol.asp */
    #warning code should be updated to take care of keyboard type
        client_mcs_data.out_uint32_le(4); // g_keyboard_type
        LOG(LOG_INFO, "core::keyboardType = IBM enhanced (101- or 102-key) keyboard");
        client_mcs_data.out_uint32_le(0); // g_keyboard_subtype
        LOG(LOG_INFO, "core::keyboardSubType = 0");
        client_mcs_data.out_uint32_le(12); // g_keyboard_functionkeys
        LOG(LOG_INFO, "core::keyboardFunctionKey = 12 function keys");
        client_mcs_data.out_clear_bytes(64); /* imeFileName */
        LOG(LOG_INFO, "core::imeFileName = \"\"");
        client_mcs_data.out_uint16_le(0xca01); /* color depth 8bpp */
        LOG(LOG_INFO, "core::postBeta2ColorDepth = RNS_UD_COLOR_8BPP (superseded by highColorDepth)");
        client_mcs_data.out_uint16_le(1);
        LOG(LOG_INFO, "core::clientProductId = 1");
        client_mcs_data.out_uint32_le(0);
        LOG(LOG_INFO, "core::serialNumber = 0");
        client_mcs_data.out_uint16_le(rdp_bpp);
        LOG(LOG_INFO, "core::highColorDepth = %u", rdp_bpp);
        client_mcs_data.out_uint16_le(0x0007);
        LOG(LOG_INFO, "core::supportedColorDepths = 24/16/15");
        client_mcs_data.out_uint16_le(1);
        LOG(LOG_INFO, "core::earlyCapabilityFlags = RNS_UD_CS_SUPPORT_ERRINFO_PDU");
        client_mcs_data.out_clear_bytes(64);
        LOG(LOG_INFO, "core::clientDigProductId = \"\"");
        client_mcs_data.out_clear_bytes(2);
        LOG(LOG_INFO, "core::pad2octets");
//        client_mcs_data.out_uint32_le(0); // optional
//        LOG(LOG_INFO, "core::serverSelectedProtocol = 0");
        /* End of client info */

        client_mcs_data.out_uint16_le(CS_CLUSTER);
        client_mcs_data.out_uint16_le(12);
        #warning check that should depend on g_console_session
        client_mcs_data.out_uint32_le(console_session ? 0xb : 9);
        client_mcs_data.out_uint32_le(0);

        /* Client encryption settings */
        client_mcs_data.out_uint16_le(CS_SECURITY);
        client_mcs_data.out_uint16_le(12); /* length */
        #warning check that, should depend on g_encryption
        /* encryption supported, 128-bit supported */
        client_mcs_data.out_uint32_le(0x3);
        client_mcs_data.out_uint32_le(0); /* Unknown */

        /* Here we need to put channel information in order to redirect channel data
        from client to server passing through the "proxy" */
        size_t num_channels = channel_list.size();

        if (num_channels > 0) {
            client_mcs_data.out_uint16_le(CS_NET);
            client_mcs_data.out_uint16_le(num_channels * 12 + 8); /* length */
            client_mcs_data.out_uint32_le(num_channels); /* number of virtual channels */
            for (size_t i = 0; i < num_channels; i++){
                const mcs_channel_item* channel_item = channel_list[i];

                LOG(LOG_DEBUG, "Requesting channel %s\n", channel_item->name);
                memcpy(client_mcs_data.p, channel_item->name, 8);
                client_mcs_data.p += 8;

                client_mcs_data.out_uint32_be(channel_item->flags);
         }
        client_mcs_data.mark_end();
     }
    }

// 2.2.1.4  Server MCS Connect Response PDU with GCC Conference Create Response
// ----------------------------------------------------------------------------
// The MCS Connect Response PDU is an RDP Connection Sequence PDU sent from
// server to client during the Basic Settings Exchange phase (see section
// 1.3.1.1). It is sent as a response to the MCS Connect Initial PDU (section
// 2.2.1.3). The MCS Connect Response PDU encapsulates a GCC Conference Create
// Response, which encapsulates concatenated blocks of settings data.

// A basic high-level overview of the nested structure for the Server MCS
// Connect Response PDU is illustrated in section 1.3.1.1, in the figure
// specifying MCS Connect Response PDU. Note that the order of the settings
// data blocks is allowed to vary from that shown in the previously mentioned
// figure and the message syntax layout that follows. This is possible because
// each data block is identified by a User Data Header structure (section
// 2.2.1.4.1).

// tpktHeader (4 bytes): A TPKT Header, as specified in [T123] section 8.

// x224Data (3 bytes): An X.224 Class 0 Data TPDU, as specified in [X224]
// section 13.7.

// mcsCrsp (variable): Variable-length BER-encoded MCS Connect Response
//   structure (using definite-length encoding) as described in [T125]
//   (the ASN.1 structure definition is detailed in [T125] section 7, part 2).
//   The userData field of the MCS Connect Response encapsulates the GCC
//   Conference Create Response data (contained in the gccCCrsp and subsequent
//   fields).

// gccCCrsp (variable): Variable-length PER-encoded GCC Connect Data structure
//   which encapsulates a Connect GCC PDU that contains a GCC Conference Create
//   Response structure as described in [T124] (the ASN.1 structure definitions
//   are specified in [T124] section 8.7) appended as user data to the MCS
//   Connect Response (using the format specified in [T124] sections 9.5 and
//   9.6). The userData field of the GCC Conference Create Response contains
//   one user data set consisting of concatenated server data blocks.

// serverCoreData (12 bytes): Server Core Data structure (section 2.2.1.4.2).

// serverSecurityData (variable): Variable-length Server Security Data structure
//   (section 2.2.1.4.3).

// serverNetworkData (variable): Variable-length Server Network Data structure
//   (section 2.2.1.4.4).

    /*****************************************************************************/
    /* Establish a secure connection */
    void rdp_sec_connect(Transport * trans, vector<mcs_channel_item*> channel_list,
                        int width, int height,
                        int rdp_bpp, int keylayout,
                        bool console_session, int & use_rdp5, char * hostname, char * username) throw(Error)
    {
        Stream client_mcs_data(512);

        this->rdp_sec_out_mcs_data(
                            client_mcs_data,
                            channel_list,
                            width, height, rdp_bpp,
                            keylayout, console_session, hostname);

        LOG(LOG_INFO, "Iso Layer : connect %s\n", username);

        try {

//2.2.1.1    Client X.224 Connection Request PDU
//==============================================

// The X.224 Connection Request PDU is an RDP Connection Sequence PDU sent from
// client to server during the Connection Initiation phase (see section
// 1.3.1.1).

// tpktHeader (4 bytes): A TPKT Header, as specified in [T123] section 8.

// x224Crq (7 bytes): An X.224 Class 0 Connection Request transport protocol
//   data unit (TPDU), as specified in [X224] section 13.3.

// routingToken (variable): Optional and variable-length routing token bytes
//   used for load balancing terminated by a carriage-return (CR) and line-feed
//   (LF) ANSI sequence. For more information, see [MSFT-SDLBTS]. The length of
//   the routing token and CR+LF sequence is included in the X.224 Connection
//   Request Length Indicator field.

// rdpNegData (8 bytes): An optional RDP Negotiation Request (section 2.2.1.1.1)
//   structure. The length of this negotiation structure is included in the
//   X.224 Connection Request Length Indicator field.


            Stream out;
            X224Out crtpdu(X224Packet::CR_TPDU, out);

            #warning looks like this strange cookie thing is in fact useless, see MSFT-SDLBTS
            // USER DATA
//            out.out_concat("Cookie: mstshash=");
//            out.out_concat(this->username);
//            out.out_concat("\r\n");
//            crtpdu.extend_tpdu_hdr();
            crtpdu.end();

            crtpdu.send(trans);

            Stream in;
            X224In cctpdu(trans, in);
            if (cctpdu.tpkt.version != 3){
                throw Error(ERR_T123_EXPECTED_TPKT_VERSION_3);
            }
            if (cctpdu.tpdu_hdr.code != X224Packet::CC_TPDU){
                throw Error(ERR_X224_EXPECTED_CONNECTION_CONFIRM);
            }
        } catch (Error) {
            try {
                trans->disconnect();
            } catch (Error){
                // rethrow the first error, not the error we could get disconnecting
            }
            throw;
        }

        try{
            {
                int data_len = client_mcs_data.end - client_mcs_data.data;
                int len = 7 + 3 * 34 + 4 + data_len;

                Stream stream(8192);
                X224Out tpdu(X224Packet::DT_TPDU, stream);

                stream.out_uint16_be(BER_TAG_MCS_CONNECT_INITIAL);
                stream.out_ber_len(len);
                stream.out_uint8(BER_TAG_OCTET_STRING);
                stream.out_ber_len(0); /* calling domain */
                stream.out_uint8(BER_TAG_OCTET_STRING);
                stream.out_ber_len(0); /* called domain */
                stream.out_uint8(BER_TAG_BOOLEAN);
                stream.out_ber_len(1);
                stream.out_uint8(0xff); /* upward flag */

                // target params
                stream.out_uint8(BER_TAG_MCS_DOMAIN_PARAMS);
                stream.out_ber_len(32);
                stream.out_ber_int16(34);     // max_channels
                stream.out_ber_int16(2);      // max_users
                stream.out_ber_int16(0);      // max_tokens
                stream.out_ber_int16(1);
                stream.out_ber_int16(0);
                stream.out_ber_int16(1);
                stream.out_ber_int16(0xffff); // max_pdu_size
                stream.out_ber_int16(2);

                // min params
                stream.out_uint8(BER_TAG_MCS_DOMAIN_PARAMS);
                stream.out_ber_len(32);
                stream.out_ber_int16(1);     // max_channels
                stream.out_ber_int16(1);     // max_users
                stream.out_ber_int16(1);     // max_tokens
                stream.out_ber_int16(1);
                stream.out_ber_int16(0);
                stream.out_ber_int16(1);
                stream.out_ber_int16(0x420); // max_pdu_size
                stream.out_ber_int16(2);

                // max params
                stream.out_uint8(BER_TAG_MCS_DOMAIN_PARAMS);
                stream.out_ber_len(32);
                stream.out_ber_int16(0xffff); // max_channels
                stream.out_ber_int16(0xfc17); // max_users
                stream.out_ber_int16(0xffff); // max_tokens
                stream.out_ber_int16(1);
                stream.out_ber_int16(0);
                stream.out_ber_int16(1);
                stream.out_ber_int16(0xffff); // max_pdu_size
                stream.out_ber_int16(2);

                stream.out_uint8(BER_TAG_OCTET_STRING);
                stream.out_ber_len(data_len);
                stream.out_copy_bytes(client_mcs_data.data, data_len);

                tpdu.end();
                tpdu.send(trans);
            }
            Stream stream(8192);
            this->recv_connect_response(stream, trans);

            LOG(LOG_INFO, "rdp_sec_process_mcs_data\n");
            this->rdp_sec_process_mcs_data(stream, channel_list, use_rdp5);

            {
                Stream stream(8192);
                X224Out tpdu(X224Packet::DT_TPDU, stream);
                stream.out_uint8((MCS_EDRQ << 2));
                stream.out_uint16_be(0x100); /* height */
                stream.out_uint16_be(0x100); /* interval */
                tpdu.end();
                tpdu.send(trans);
            }
            {
                Stream stream(8192);
                X224Out tpdu(X224Packet::DT_TPDU, stream);
                stream.out_uint8((MCS_AURQ << 2));
                tpdu.end();
                tpdu.send(trans);
            }
            {
                Stream stream(8192);
                X224In(trans, stream);
                int opcode = stream.in_uint8();
                if ((opcode >> 2) != MCS_AUCF) {
                    throw Error(ERR_MCS_RECV_AUCF_OPCODE_NOT_OK);
                }
                int res = stream.in_uint8();
                if (res != 0) {
                    throw Error(ERR_MCS_RECV_AUCF_RES_NOT_0);
                }
                if (opcode & 2) {
                    this->userid = stream.in_uint16_be();
                }
                if (!(stream.check_end())) {
                    throw Error(ERR_MCS_RECV_AUCF_ERROR_CHECKING_STREAM);
                }
            }

            #warning the array size below is arbitrary, it should be checked to avoid buffer overflow
            uint16_t channels[100];

            channels[0] = this->userid + 1001;
            channels[1] = MCS_GLOBAL_CHANNEL;
            size_t num_channels = this->channel_list.size();
            for (size_t index = 0; index < num_channels; index++){
                const mcs_channel_item* channel_item = this->channel_list[index];
                channels[2+index] = channel_item->chanid;
            }

            // 2.2.1.8 Client MCS Channel Join Request PDU
            // -------------------------------------------
            // The MCS Channel Join Request PDU is an RDP Connection Sequence PDU sent
            // from client to server during the Channel Connection phase (see section
            // 1.3.1.1). It is sent after receiving the MCS Attach User Confirm PDU
            // (section 2.2.1.7). The client uses the MCS Channel Join Request PDU to
            // join the user channel obtained from the Attach User Confirm PDU, the
            // I/O channel and all of the static virtual channels obtained from the
            // Server Network Data structure (section 2.2.1.4.4).

            // tpktHeader (4 bytes): A TPKT Header, as specified in [T123] section 8.

            // x224Data (3 bytes): An X.224 Class 0 Data TPDU, as specified in [X224]
            //                     section 13.7.

            // mcsCJrq (5 bytes): PER-encoded MCS Domain PDU which encapsulates an
            //                    MCS Channel Join Request structure as specified in
            //                    [T125] sections 10.19 and I.3 (the ASN.1 structure
            //                    definitions are given in [T125] section 7, parts 6
            //                    and 10).

            // ChannelJoinRequest ::= [APPLICATION 14] IMPLICIT SEQUENCE
            // {
            //     initiator UserId
            //     channelId ChannelId
            //               -- may be zero
            // }

            for (size_t index = 0; index < num_channels+2; index++){
                {
                    Stream stream(8192);
                    X224Out tpdu(X224Packet::DT_TPDU, stream);
                    stream.out_uint8((MCS_CJRQ << 2));
                    stream.out_uint16_be(this->userid);
                    stream.out_uint16_be(channels[index]);
                    tpdu.end();
                    tpdu.send(trans);
                }
                {
                    Stream stream(8192);
                    X224In(trans, stream);
                    int opcode = stream.in_uint8();
                    if ((opcode >> 2) != MCS_CJCF) {
                        throw Error(ERR_MCS_RECV_CJCF_OPCODE_NOT_CJCF);
                    }
                    if (0 != stream.in_uint8()) {
                        throw Error(ERR_MCS_RECV_CJCF_EMPTY);
                    }
                    stream.skip_uint8(4); /* mcs_userid, req_chanid */
                    if (opcode & 2) {
                        stream.skip_uint8(2); /* join_chanid */
                    }
                    if (!stream.check_end()) {
                        throw Error(ERR_MCS_RECV_CJCF_ERROR_CHECKING_STREAM);
                    }
                }
            }
        }
        catch(...){
            Stream stream(11);
            X224Out tpdu(X224Packet::DR_TPDU, stream);
            tpdu.end();
            tpdu.send(trans);
            throw;
        }

        LOG(LOG_INFO, "Iso Layer : setting encryption\n");
        /* Send the client random to the server */
//      if (this->encryption)
        {
            Stream stream(8192);
            X224Out tpdu(X224Packet::DT_TPDU, stream);
            McsOut sdrq_out(stream, MCS_SDRQ, this->userid, MCS_GLOBAL_CHANNEL);

            int length = this->server_public_key_len + SEC_PADDING_SIZE;

            stream.out_uint32_le(SEC_CLIENT_RANDOM);

            stream.out_uint32_le(length);
            LOG(LOG_INFO, "Server public key is %d bytes long", this->server_public_key_len);
            stream.out_copy_bytes(this->client_crypt_random, this->server_public_key_len);
            stream.out_clear_bytes(SEC_PADDING_SIZE);

            sdrq_out.end();
            tpdu.end();
            tpdu.send(trans);
        }
    }

};


class SecOut
{
    Stream & stream;
    uint16_t offhdr;
    uint8_t crypt_level;
    CryptContext & crypt;
    public:
    SecOut(Stream & stream, uint8_t crypt_level, uint32_t flags, CryptContext & crypt)
        : stream(stream), offhdr(stream.p - stream.data), crypt_level(crypt_level), crypt(crypt)
    {
        if (crypt_level > 1){
            this->stream.out_uint32_le(flags);
            this->stream.skip_uint8(8);
        }
        else {
            this->stream.out_uint32_le(0);
        }
    }

    void end(){
        if (crypt_level > 1){
            uint8_t * data = this->stream.data + this->offhdr + 12;
            int datalen = this->stream.p - data;
            this->crypt.sign(this->stream.data + this->offhdr + 4, 8, data, datalen);
            this->crypt.encrypt(data, datalen);
        }
    }
};


class SecIn
{
    public:
    uint32_t flags;
    SecIn(Stream & stream, uint8_t crypt_level, CryptContext & crypt)
    {
        this->flags = stream.in_uint32_le();
        if (this->flags & SEC_ENCRYPT){
            #warning shouldn't we check signature ?
            stream.skip_uint8(8); /* signature */
            crypt.decrypt(stream.p, stream.end - stream.p); // decrypting to the end of tpdu ?
        }
    }
};

#endif