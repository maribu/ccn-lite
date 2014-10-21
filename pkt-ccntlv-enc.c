/*
 * @f pkt-ccntlv-enc.c
 * @b CCN lite - CCNx pkt composing routines (TLV pkt format Nov 2013)
 *
 * Copyright (C) 2014, Christian Tschudin, University of Basel
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * File history:
 * 2014-03-05 created
 */

#ifndef PKT_CCNTLV_ENC_C
#define PKT_CCNTLV_ENC_C

#include "pkt-ccntlv.h"

int
ccnl_ccntlv_prependTL(unsigned int type, unsigned short len, int *offset, unsigned char *buf)
{
/*
    if (*offset < 4)
        return -1;
*/
    unsigned short *ip = (unsigned short*) (buf + *offset - 2);
    *ip-- = htons(len);
    *ip = htons(type);
    *offset -= 4;
    return 4;
}

int
ccnl_ccntlv_prependBlob(unsigned short type, unsigned char *blob,
                        unsigned short len, int *offset, unsigned char *buf)
{
    if (*offset < (len + 4))
        return -1;
    memcpy(buf + *offset - len, blob, len);
    *offset -= len;
    if (ccnl_ccntlv_prependTL(type, len, offset, buf) < 0)
        return -1;
    return len + 4;
}


// int
// ccnl_ccntlv_prependFixedHdr_ccnx201311(unsigned char ver, unsigned char msgtype,
//                                        unsigned short msglen,
//                                        int *offset, unsigned char *buf)
// {
//     struct ccnx_tlvhdr_ccnx201311_s *hp;

//     if (*offset < 8)
//         return -1;
//     *offset -= 8;
//     hp = (struct ccnx_tlvhdr_ccnx201311_s *)(buf + *offset);
//     hp->version = ver;
//     hp->msgtype = msgtype;
//     hp->msglen = htons(msglen);
//     hp->hdrlen = htons(hdrlen);
//     hp->reserved = 0;

//     return 8 + hdrlen + msglen;
// }

int
ccnl_ccntlv_prependFixedHdr(unsigned char ver, 
                            unsigned char packettype, 
                            unsigned short payloadlen, 
                            unsigned char hoplimit, 
                            int *offset, unsigned char *buf)
{
    struct ccnx_tlvhdr_ccnx201409_s *hp;

    // Currently there are no optional headers, only fixed header of size 8
    unsigned char hdrlen = 8;

    if (*offset < 8 || payloadlen < 0)
        return -1;
    *offset -= 8;
    hp = (struct ccnx_tlvhdr_ccnx201409_s *)(buf + *offset);
    hp->version = ver;
    hp->packettype = packettype;
    hp->payloadlen = htons(payloadlen);
    hp->hoplimit = hoplimit;
    hp->hdrlen = htons(hdrlen);
    hp->reserved = 0;

    return hdrlen + payloadlen;
}

int
ccnl_ccntlv_prependName(struct ccnl_prefix_s *name,
                        int *offset, unsigned char *buf)
{
    int oldoffset = *offset, cnt;

    //TODO
    // CCNX_TLV_N_CHUNK

    // optional: (not used)
    // CCNX_TLV_N_MetaData

#ifdef USE_NFN
    if (name->nfnflags & CCNL_PREFIX_NFN) {
        if (ccnl_ccntlv_prependBlob(CCNX_TLV_N_NameSegment,
                                (unsigned char*) "NFN", 3, offset, buf) < 0)
            return -1;
        if (name->nfnflags & CCNL_PREFIX_THUNK)
            if (ccnl_ccntlv_prependBlob(CCNX_TLV_N_NameSegment,
                                (unsigned char*) "THUNK", 5, offset, buf) < 0)
                return -1;
    }
#endif
    for (cnt = name->compcnt - 1; cnt >= 0; cnt--) {
        if (ccnl_ccntlv_prependBlob(CCNX_TLV_N_NameSegment, name->comp[cnt],
                                    name->complen[cnt], offset, buf) < 0)
            return -1;
    }
    if (ccnl_ccntlv_prependTL(CCNX_TLV_M_Name, oldoffset - *offset,
                              offset, buf) < 0)
        return -1;

    return 0;
}

int
ccnl_ccntlv_fillInterest(struct ccnl_prefix_s *name,
                         int *offset, unsigned char *buf)
{
    int oldoffset = *offset;

    if (ccnl_ccntlv_prependName(name, offset, buf))
        return -1;
    if (ccnl_ccntlv_prependTL(CCNX_TLV_TL_Interest,
                                        oldoffset - *offset, offset, buf) < 0)
        return -1;

    return oldoffset - *offset;
}

int
ccnl_ccntlv_fillInterestWithHdr(struct ccnl_prefix_s *name, 
                                int *offset, unsigned char *buf)
{
    int len;
    // setting hoplimit to max valid value
    unsigned char hoplimit = 255;

    len = ccnl_ccntlv_fillInterest(name, offset, buf);
    if(len >= 65536)
        return -1;
    len = ccnl_ccntlv_prependFixedHdr(CCNX_TLV_V0, CCNX_TLV_TL_Interest, len, hoplimit,
                                      offset, buf);

    return len;
}

int
ccnl_ccntlv_fillContent(struct ccnl_prefix_s *name, unsigned char *payload,
                        int paylen, int *offset, int *contentpos,
                        unsigned char *buf)
{
    int oldoffset = *offset;

    if (contentpos)
        *contentpos = *offset - paylen;

    // fill in backwards
    if (ccnl_ccntlv_prependBlob(CCNX_TLV_M_Payload, payload, paylen,
                                                        offset, buf) < 0)
        return -1;

    // TODO: CCNX_TLV_TL_MetaData

    if (ccnl_ccntlv_prependName(name, offset, buf))
        return -1;
    if (ccnl_ccntlv_prependTL(CCNX_TLV_TL_Object,
                                        oldoffset - *offset, offset, buf) < 0)
        return -1;

    if (contentpos)
        *contentpos -= *offset;

    return oldoffset - *offset;
}

int
ccnl_ccntlv_fillContentWithHdr(struct ccnl_prefix_s *name,
                               unsigned char *payload, int paylen,
                               int *offset, int *contentpos, unsigned char *buf)
{
    int len; // PayloadLengnth 

    // hoplimit unused for a content object
    // setting it to max to be sure
    unsigned char hoplimit = 255;


    len = ccnl_ccntlv_fillContent(name, payload, paylen, offset,
                                  contentpos, buf);

    if(len >= 65536)
        return -1;

    len = ccnl_ccntlv_prependFixedHdr(CCNX_TLV_V0, CCNX_TLV_TL_Object,
                                      len, hoplimit, offset, buf);

    return len;
}

#endif
// eof
