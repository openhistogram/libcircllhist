/*
 * Copyright (c) 2012-2015, Circonus, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name Circonus, Inc. nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "circllhist_config.h"
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <math.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "circllhist.h"

static union {
   uint64_t  private_nan_internal_rep;
   double    private_nan_double_rep;
} private_nan_union = { .private_nan_internal_rep = 0x7fffffffffffffff };

#define MAX_HIST_BINS (2 + 2 * 90 * 256)
#define private_nan private_nan_union.private_nan_double_rep

static double power_of_ten[256] = {
  1, 10, 100, 1000, 10000, 100000, 1e+06, 1e+07, 1e+08, 1e+09, 1e+10,
  1e+11, 1e+12, 1e+13, 1e+14, 1e+15, 1e+16, 1e+17, 1e+18, 1e+19, 1e+20,
  1e+21, 1e+22, 1e+23, 1e+24, 1e+25, 1e+26, 1e+27, 1e+28, 1e+29, 1e+30,
  1e+31, 1e+32, 1e+33, 1e+34, 1e+35, 1e+36, 1e+37, 1e+38, 1e+39, 1e+40,
  1e+41, 1e+42, 1e+43, 1e+44, 1e+45, 1e+46, 1e+47, 1e+48, 1e+49, 1e+50,
  1e+51, 1e+52, 1e+53, 1e+54, 1e+55, 1e+56, 1e+57, 1e+58, 1e+59, 1e+60,
  1e+61, 1e+62, 1e+63, 1e+64, 1e+65, 1e+66, 1e+67, 1e+68, 1e+69, 1e+70,
  1e+71, 1e+72, 1e+73, 1e+74, 1e+75, 1e+76, 1e+77, 1e+78, 1e+79, 1e+80,
  1e+81, 1e+82, 1e+83, 1e+84, 1e+85, 1e+86, 1e+87, 1e+88, 1e+89, 1e+90,
  1e+91, 1e+92, 1e+93, 1e+94, 1e+95, 1e+96, 1e+97, 1e+98, 1e+99, 1e+100,
  1e+101, 1e+102, 1e+103, 1e+104, 1e+105, 1e+106, 1e+107, 1e+108, 1e+109,
  1e+110, 1e+111, 1e+112, 1e+113, 1e+114, 1e+115, 1e+116, 1e+117, 1e+118,
  1e+119, 1e+120, 1e+121, 1e+122, 1e+123, 1e+124, 1e+125, 1e+126, 1e+127,
  1e-128, 1e-127, 1e-126, 1e-125, 1e-124, 1e-123, 1e-122, 1e-121, 1e-120,
  1e-119, 1e-118, 1e-117, 1e-116, 1e-115, 1e-114, 1e-113, 1e-112, 1e-111,
  1e-110, 1e-109, 1e-108, 1e-107, 1e-106, 1e-105, 1e-104, 1e-103, 1e-102,
  1e-101, 1e-100, 1e-99, 1e-98, 1e-97, 1e-96,
  1e-95, 1e-94, 1e-93, 1e-92, 1e-91, 1e-90, 1e-89, 1e-88, 1e-87, 1e-86,
  1e-85, 1e-84, 1e-83, 1e-82, 1e-81, 1e-80, 1e-79, 1e-78, 1e-77, 1e-76,
  1e-75, 1e-74, 1e-73, 1e-72, 1e-71, 1e-70, 1e-69, 1e-68, 1e-67, 1e-66,
  1e-65, 1e-64, 1e-63, 1e-62, 1e-61, 1e-60, 1e-59, 1e-58, 1e-57, 1e-56,
  1e-55, 1e-54, 1e-53, 1e-52, 1e-51, 1e-50, 1e-49, 1e-48, 1e-47, 1e-46,
  1e-45, 1e-44, 1e-43, 1e-42, 1e-41, 1e-40, 1e-39, 1e-38, 1e-37, 1e-36,
  1e-35, 1e-34, 1e-33, 1e-32, 1e-31, 1e-30, 1e-29, 1e-28, 1e-27, 1e-26,
  1e-25, 1e-24, 1e-23, 1e-22, 1e-21, 1e-20, 1e-19, 1e-18, 1e-17, 1e-16,
  1e-15, 1e-14, 1e-13, 1e-12, 1e-11, 1e-10, 1e-09, 1e-08, 1e-07, 1e-06,
  1e-05, 0.0001, 0.001, 0.01, 0.1
};

static const char __b64[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', 0x00 };

struct hist_flevel {
  uint8_t l2;
  uint8_t l1;
};

//! A bucket-count pair
struct hist_bv_pair {
  hist_bucket_t bucket;
  uint64_t count;
}__attribute__((packed));

//! The histogram structure
//! Internals are regarded private and might change with version.
//! Only use the public methods to operate on this structure.
struct histogram {
  uint16_t allocd; //!< number of allocated bv pairs
  uint16_t used;   //!< number of used bv pairs
  uint32_t fast: 1;
  struct hist_bv_pair *bvs; //!< pointer to bv-pairs
};

struct histogram_fast {
  struct histogram internal;
  uint16_t *faster[256];
};
uint64_t bvl_limits[7] = {
  0x00000000000000ffULL, 0x0000000000000ffffULL,
  0x0000000000ffffffULL, 0x00000000fffffffffULL,
  0x000000ffffffffffULL, 0x0000fffffffffffffULL,
  0x00ffffffffffffffULL
};
typedef enum {
  BVL1 = 0,
  BVL2 = 1,
  BVL3 = 2,
  BVL4 = 3,
  BVL5 = 4,
  BVL6 = 5,
  BVL7 = 6,
  BVL8 = 7
} bvdatum_t;

static ssize_t
bv_size(const histogram_t *h, int idx) {
  int i;
  for(i=0; i<BVL8; i++)
    if(h->bvs[idx].count <= bvl_limits[i]) return 3 + i + 1;
  return 3+8;
}

static ssize_t
bv_write(const histogram_t *h, int idx, void *buff, ssize_t size) {
  int i;
  uint8_t *cp;
  ssize_t needed;
  bvdatum_t tgt_type = BVL8;
  for(i=0; i<BVL8; i++)
    if(h->bvs[idx].count <= bvl_limits[i]) {
      tgt_type = i;
      break;
    }
  needed = 3 + tgt_type + 1;
  if(needed > size) return -1;
  cp = buff;
  cp[0] = h->bvs[idx].bucket.val;
  cp[1] = h->bvs[idx].bucket.exp;
  cp[2] = tgt_type;
  for(i=tgt_type;i>=0;i--)
    cp[i+3] = ((h->bvs[idx].count >> (i * 8)) & 0xff);
  return needed;
}
static ssize_t
bv_read(histogram_t *h, int idx, const void *buff, ssize_t len) {
  const uint8_t *cp;
  uint64_t count = 0;
  bvdatum_t tgt_type;
  int i;

  assert(idx == h->used);
  if(len < 3) return -1;
  cp = buff;
  tgt_type = cp[2];
  if(tgt_type > BVL8) return -1;
  if(len < 3 + tgt_type + 1) return -1;
  for(i=tgt_type;i>=0;i--)
    count |= ((uint64_t)cp[i+3]) << (i * 8);
  if(count != 0) {
    h->bvs[idx].bucket.val = cp[0];
    h->bvs[idx].bucket.exp = cp[1];
    h->bvs[idx].count = count;
    h->used++;
  }
  return 3 + tgt_type + 1;
}

ssize_t
hist_serialize_estimate(const histogram_t *h) {
  /* worst case if 2 for the length + 3+8 * used */
  int i;
  ssize_t len = 2;
  if(h == NULL) return len;
  for(i=0;i<h->used;i++) {
    if(h->bvs[i].count != 0) {
      len += bv_size(h, i);
    }
  }
  return len;
}

#ifndef SKIP_LIBMTEV
ssize_t
hist_serialize_b64_estimate(const histogram_t *h) {
  ssize_t len = hist_serialize_estimate(h);
  // base 64 <=> 1 char == 6 bit <=> 4 chars = 3 Byte ==> n Bytpe = 4*ceil(len/3.) chars
  return 4*(len/3+1);
}
#endif

#define ADVANCE(tracker, n) cp += (n), tracker += (n), len -= (n)
ssize_t
hist_serialize(const histogram_t *h, void *buff, ssize_t len) {
  ssize_t written = 0;
  uint8_t *cp = buff;
  uint16_t nlen;
  int i;

  if(len < 2) return -1;
  nlen = htons(h ? h->used : 0);
  memcpy(cp, &nlen, sizeof(nlen));
  ADVANCE(written, 2);
  for(i=0;h && i<h->used;i++) {
    ssize_t incr_written;
    if(h->bvs[i].count) {
      incr_written = bv_write(h, i, cp, len);
      if(incr_written < 0) return -1;
      ADVANCE(written, incr_written);
    }
  }
  return written;
}

static int
copy_of_mtev_b64_encode(const unsigned char *src, size_t src_len,
                        char *dest, size_t dest_len) {
  const unsigned char *bptr = src;
  char *eptr = dest;
  int len = src_len;
  int n = (((src_len + 2) / 3) * 4);

  if(dest_len < n) return 0;

  while(len > 2) {
    *eptr++ = __b64[bptr[0] >> 2];
    *eptr++ = __b64[((bptr[0] & 0x03) << 4) + (bptr[1] >> 4)];
    *eptr++ = __b64[((bptr[1] & 0x0f) << 2) + (bptr[2] >> 6)];
    *eptr++ = __b64[bptr[2] & 0x3f];
    bptr += 3;
    len -= 3;
  }
  if(len != 0) {
    *eptr++ = __b64[bptr[0] >> 2];
    if(len > 1) {
      *eptr++ = __b64[((bptr[0] & 0x03) << 4) + (bptr[1] >> 4)];
      *eptr++ = __b64[(bptr[1] & 0x0f) << 2];
      *eptr = '=';
    } else {
      *eptr++ = __b64[(bptr[0] & 0x03) << 4];
      *eptr++ = '=';
      *eptr = '=';
    }
  }
  return n;
}

ssize_t
hist_serialize_b64(const histogram_t *h, char *b64_serialized_histo_buff, ssize_t buff_len) {
  ssize_t serialize_buff_length = hist_serialize_estimate(h);
  void *serialize_buff = alloca(serialize_buff_length);
  ssize_t serialized_length = hist_serialize(h, serialize_buff, serialize_buff_length);

  return copy_of_mtev_b64_encode(serialize_buff, serialized_length, b64_serialized_histo_buff, buff_len);
}

ssize_t
hist_deserialize(histogram_t *h, const void *buff, ssize_t len) {
  const uint8_t *cp = buff;
  ssize_t bytes_read = 0;
  uint16_t nlen, cnt;
  if(len < 2) goto bad_read;
  if(h->bvs) free(h->bvs);
  h->bvs = NULL;
  memcpy(&nlen, cp, sizeof(nlen));
  ADVANCE(bytes_read, 2);
  h->used = 0;
  cnt = ntohs(nlen);
  h->allocd = cnt;
  if(h->allocd == 0) return bytes_read;
  h->bvs = calloc(h->allocd, sizeof(*h->bvs));
  if(!h->bvs) goto bad_read; /* yeah, yeah... bad label name */
  while(len > 0 && cnt > 0) {
    ssize_t incr_read = 0;
    if(h->used >= h->allocd) {
      return -1;
    }
    incr_read = bv_read(h, h->used, cp, len);
    if(incr_read < 0) goto bad_read;
    ADVANCE(bytes_read, incr_read);
    cnt--;
  }
  return bytes_read;

 bad_read:
  if(h->bvs) free(h->bvs);
  h->bvs = NULL;
  h->used = h->allocd = 0;
  return -1;
}

static int
copy_of_mtev_b64_decode(const char *src, size_t src_len,
                        unsigned char *dest, size_t dest_len) {
  const unsigned char *cp = (unsigned char *)src;
  unsigned char *dcp = dest;
  unsigned char ch, in[4], out[3];
  int ib = 0, ob = 3, needed = (((src_len / 4) * 3) - 2);

  if(dest_len < needed) return 0;
  while(cp <= ((unsigned char *)src+src_len)) {
    if((*cp >= 'A') && (*cp <= 'Z')) ch = *cp - 'A';
    else if((*cp >= 'a') && (*cp <= 'z')) ch = *cp - 'a' + 26;
    else if((*cp >= '0') && (*cp <= '9')) ch = *cp - '0' + 52;
    else if(*cp == '+') ch = 62;
    else if(*cp == '/') ch = 63;
    else if(*cp == '=') ch = 0xff;
    else if(isspace((int)*cp)) { cp++; continue; }
    else break;
    cp++;
    if(ch == 0xff) {
      if(ib == 0) break;
      if(ib == 1 || ib == 2) ob = 1;
      else ob = 2;
      ib = 3;
    }
    in[ib++] = ch;
    if(ib == 4) {
      out[0] = (in[0] << 2) | ((in[1] & 0x30) >> 4);
      out[1] = ((in[1] & 0x0f) << 4) | ((in[2] & 0x3c) >> 2);
      out[2] = ((in[2] & 0x03) << 6) | (in[3] & 0x3f);
      for(ib = 0; ib < ob; ib++)
        *dcp++ = out[ib];
      ib = 0;
    }
  }
  return dcp - (unsigned char *)dest;
}

ssize_t hist_deserialize_b64(histogram_t *h, const void *b64_string, ssize_t b64_string_len) {
    int decoded_hist_len;
    unsigned char* decoded_hist = alloca(b64_string_len);

    decoded_hist_len = copy_of_mtev_b64_decode(b64_string, b64_string_len, decoded_hist, b64_string_len);

    if (decoded_hist_len < 2) {
      return -1;
    }

    ssize_t bytes_read = hist_deserialize(h, decoded_hist, decoded_hist_len);
    if (bytes_read != decoded_hist_len) {
      return -1;
    }
    return bytes_read;
}

static inline
int hist_bucket_cmp(hist_bucket_t h1, hist_bucket_t h2) {
  // checks if h1 < h2 on the real axis.
  if(*(uint16_t *)&h1 == *(uint16_t *)&h2) return 0;
  /* place NaNs at the beginning always */
  if(h1.val == (int8_t)0xff) return 1;
  if(h2.val == (int8_t)0xff) return -1;
  /* zero values need special treatment */
  if(h1.val == 0) return (h2.val > 0) ? 1 : -1;
  if(h2.val == 0) return (h1.val < 0) ? 1 : -1;
  /* opposite signs? */
  if(h1.val < 0 && h2.val > 0) return 1;
  if(h1.val > 0 && h2.val < 0) return -1;
  /* here they are either both positive or both negative */
  if(h1.exp == h2.exp) return (h1.val < h2.val) ? 1 : -1;
  if(h1.exp > h2.exp) return (h1.val < 0) ? 1 : -1;
  if(h1.exp < h2.exp) return (h1.val < 0) ? -1 : 1;
  /* unreachable */
  return 0;
}

double
hist_bucket_to_double(hist_bucket_t hb) {
  uint8_t *pidx;
  assert(private_nan != 0);
  pidx = (uint8_t *)&hb.exp;
  if(hb.val == (int8_t)0xff) return private_nan;
  if(hb.val > 99 || hb.val < -99) return private_nan;
  if(hb.val < 10 && hb.val > -10) return 0.0;
  return (((double)hb.val)/10.0) * power_of_ten[*pidx];
}

double
hist_bucket_to_double_bin_width(hist_bucket_t hb) {
  if(hb.val == (int8_t)0xff) return private_nan;
  if(hb.val > 99 || hb.val < -99) return private_nan;
  if(hb.val < 10 && hb.val > -10) return 0.0;
  uint8_t *pidx;
  pidx = (uint8_t *)&hb.exp;
  return power_of_ten[*pidx]/10.0;
}

double
hist_bucket_midpoint(hist_bucket_t in) {
  double out, interval;
  if(in.val > 99 || in.val < -99) return private_nan;
  out = hist_bucket_to_double(in);
  if(out == 0) return 0;
  interval = hist_bucket_to_double_bin_width(in);
  if(out < 0) interval *= -1.0;
  return out + interval/2.0;
}

/* This is used for quantile calculation,
 * where we want the side of the bucket closest to -inf */
static double
hist_bucket_left(hist_bucket_t in) {
  double out, interval;
  if(in.val > 99 || in.val < -99) return private_nan;
  out = hist_bucket_to_double(in);
  if(out == 0) return 0;
  if(out > 0) return out;
  /* out < 0 */
  interval = hist_bucket_to_double_bin_width(in);
  return out - interval;
}

double
hist_approx_mean(const histogram_t *hist) {
  int i;
  double divisor = 0.0;
  double sum = 0.0;
  for(i=0; i<hist->used; i++) {
    if(hist->bvs[i].bucket.val > 99 || hist->bvs[i].bucket.val < -99) continue;
    double midpoint = hist_bucket_midpoint(hist->bvs[i].bucket);
    double cardinality = (double)hist->bvs[i].count;
    divisor += cardinality;
    sum += midpoint * cardinality;
  }
  if(divisor == 0.0) return private_nan;
  return sum/divisor;
}

double
hist_approx_sum(const histogram_t *hist) {
  int i;
  double sum = 0.0;
  for(i=0; i<hist->used; i++) {
    if(hist->bvs[i].bucket.val > 99 || hist->bvs[i].bucket.val < -99) continue;
    double value = hist_bucket_midpoint(hist->bvs[i].bucket);
    double cardinality = (double)hist->bvs[i].count;
    sum += value * cardinality;
  }
  return sum;
}

/* 0 success,
 * -1 (empty histogram),
 * -2 (out of order quantile request)
 * -3 (out of bound quantile)
 */
int
hist_approx_quantile(const histogram_t *hist, double *q_in, int nq, double *q_out) {
  int i_q, i_b;
  double total_cnt = 0.0, bucket_width = 0.0,
         bucket_left = 0.0, lower_cnt = 0.0, upper_cnt = 0.0;
  if(nq < 1) return 0; /* nothing requested, easy to satisfy successfully */

  /* Sum up all samples from all the bins */
  for (i_b=0;i_b<hist->used;i_b++) {
    /* ignore NaN */
    if(hist->bvs[i_b].bucket.val < -99 || hist->bvs[i_b].bucket.val > 99)
      continue;
    total_cnt += (double)hist->bvs[i_b].count;
  }

  /* Run through the quantiles and make sure they are in order */
  for (i_q=1;i_q<nq;i_q++) if(q_in[i_q-1] > q_in[i_q]) return -2;
  /* We use q_out as temporary space to hold the count-normailzed quantiles */
  for (i_q=0;i_q<nq;i_q++) {
    if(q_in[i_q] < 0.0 || q_in[i_q] > 1.0) return -3;
    q_out[i_q] = total_cnt * q_in[i_q];
  }


  i_b = 0;
#define TRACK_VARS(idx) do { \
  bucket_width = hist_bucket_to_double_bin_width(hist->bvs[idx].bucket); \
  bucket_left = hist_bucket_left(hist->bvs[idx].bucket); \
  lower_cnt = upper_cnt; \
  upper_cnt = lower_cnt + hist->bvs[idx].count; \
} while(0)

  /* Find the least bin (first) */
  for(i_b=0;i_b<hist->used;i_b++) {
    /* We don't include NaNs */
    if(hist->bvs[i_b].bucket.val < -99 || hist->bvs[i_b].bucket.val > 99)
      continue;
    TRACK_VARS(i_b);
    break;
  }

  /* Next walk the bins and the quantiles together */
  for(i_q=0;i_q<nq;i_q++) {
    /* And within that, advance the bins as needed */
    while(i_b < (hist->used-1) && upper_cnt < q_out[i_q]) {
      i_b++;
      TRACK_VARS(i_b);
    }
    if(lower_cnt == q_out[i_q]) {
      q_out[i_q] = bucket_left;
    }
    else if(upper_cnt == q_out[i_q]) {
      q_out[i_q] = bucket_left + bucket_width;
    }
    else {
      if(bucket_width == 0) q_out[i_q] = bucket_left;
      else q_out[i_q] = bucket_left +
             (q_out[i_q] - lower_cnt) / (upper_cnt - lower_cnt) * bucket_width;
    }
  }
  return 0;
}

hist_bucket_t
int_scale_to_hist_bucket(int64_t value, int scale) {
  static hist_bucket_t hbnan = { (int8_t)0xff, 0 };
  hist_bucket_t hb = { 0, 0 };
  int sign = 1;
  if(value == 0) return hb;
  scale++;
  if(value < 0) {
    value = 0 - value;
    sign = -1;
  }
  if(value < 10) {
    value *= 10;
    scale -= 1;
  }
  while(value > 100) {
    value /= 10;
    scale++;
  }
  if(scale < -128) return hb;
  if(scale > 127) return hbnan;
  hb.val = sign * value;
  hb.exp = scale;
  return hb;
}

hist_bucket_t
double_to_hist_bucket(double d) {
  double d_copy = d;
  hist_bucket_t hb = { (int8_t)0xff, 0 }; // NaN
  assert(private_nan != 0);
  if(isnan(d) || isinf(d)) return hb;
  else if(d==0) hb.val = 0;
  else {
    int big_exp;
    uint8_t *pidx;
    int sign = (d < 0) ? -1 : 1;
    d = fabs(d);
    big_exp = (int32_t)floor(log10(d));
    hb.exp = (int8_t)big_exp;
    if(hb.exp != big_exp) { /* we rolled */
      if(big_exp < 0) {
        /* d is in [0 .. 1e-128). Return 0 */
        hb.val = 0;
        hb.exp = 0;
      } else {
        /* d is >= 1e128. Return NaN */
        hb.val = (int8_t)0xff;
        hb.exp = 0;
      }
      return hb;
    }
    pidx = (uint8_t *)&hb.exp;
    d /= power_of_ten[*pidx];
    d *= 10;
    // avoid rounding problem at the bucket boundary
    // e.g. d=0.11 results in hb.val = 10 (shoud be 11)
    // by allowing a error margin (in the order or magintude
    // of the exected rounding errors of the above transformations)
    hb.val = sign * (int)floor(d + 1e-13);
    if(hb.val == 100 || hb.val == -100) {
      if (hb.exp < 127) {
        hb.val /= 10;
        hb.exp++;
      } else { // can't increase exponent. Return NaN
        hb.val = (int8_t)0xff;
        hb.exp = 0;
      }
    }
    if(hb.val == 0) {
      hb.exp = 0;
      return hb;
    }
    if(!((hb.val >= 10 && hb.val < 100) ||
         (hb.val <= -10 && hb.val > -100))) {
      uint64_t double_pun = 0;
      memcpy(&double_pun, &d_copy, sizeof(d_copy));
      hb.val = (int8_t)0xff;
      hb.exp = 0;
    }
  }
  return hb;
}

static int
hist_internal_find(histogram_t *hist, hist_bucket_t hb, int *idx) {
  /* This is a simple binary search returning the idx in which
   * the specified bucket belongs... returning 1 if it is there
   * or 0 if the value would need to be inserted here (moving the
   * rest of the buckets forward one.
   */
  int rv = -1, l = 0, r = hist->used - 1;
  *idx = 0;
  if(hist->used == 0) return 0;
  if(hist->fast) {
    struct histogram_fast *hfast = (struct histogram_fast *)hist;
    struct hist_flevel *faster = (struct hist_flevel *)&hb;
    if(hfast->faster[faster->l1]) {
      *idx = hfast->faster[faster->l1][faster->l2];
      if(*idx) {
        (*idx)--;
        return 1;
      }
    }
  }
  while(l < r) {
    int check = (r+l)/2;
    rv = hist_bucket_cmp(hist->bvs[check].bucket, hb);
    if(rv == 0) l = r = check;
    else if(rv > 0) l = check + 1;
    else r = check - 1;
  }
  /* if rv == 0 we found a match, no need to compare again */
  if(rv != 0) rv = hist_bucket_cmp(hist->bvs[l].bucket, hb);
  *idx = l;
  if(rv == 0) return 1;   /* this is it */
  if(rv < 0) return 0;    /* it goes here (before) */
  (*idx)++;               /* it goes after here */
  assert(*idx >= 0 && *idx <= hist->used);
  return 0;
}

uint64_t
hist_insert_raw(histogram_t *hist, hist_bucket_t hb, uint64_t count) {
  int found, idx;
  if(hist->bvs == NULL) {
    hist->bvs = malloc(DEFAULT_HIST_SIZE * sizeof(*hist->bvs));
    hist->allocd = DEFAULT_HIST_SIZE;
  }
  found = hist_internal_find(hist, hb, &idx);
  if(!found) {
    int i;
    if(hist->used == hist->allocd) {
      /* A resize is required */
      histogram_t dummy;
      dummy.bvs = malloc((hist->allocd + DEFAULT_HIST_SIZE) *
                         sizeof(*hist->bvs));
      if(idx > 0)
        memcpy(dummy.bvs, hist->bvs, idx * sizeof(*hist->bvs));
      dummy.bvs[idx].bucket = hb;
      dummy.bvs[idx].count = count;
      if(idx < hist->used)
        memcpy(dummy.bvs + idx + 1, hist->bvs + idx,
               (hist->used - idx)*sizeof(*hist->bvs));
      free(hist->bvs);
      hist->bvs = dummy.bvs;
      hist->allocd += DEFAULT_HIST_SIZE;
    }
    else { // used !== alloced
      /* We need to shuffle out data to poke the new one in */
      memmove(hist->bvs + idx + 1, hist->bvs + idx,
              (hist->used - idx)*sizeof(*hist->bvs));
      hist->bvs[idx].bucket = hb;
      hist->bvs[idx].count = count;
    }
    hist->used++;
    if(hist->fast) {
      struct histogram_fast *hfast = (struct histogram_fast *)hist;
      /* reindex if in fast mode */
      for(i=idx;i<hist->used;i++) {
        struct hist_flevel *faster = (struct hist_flevel *)&hist->bvs[i].bucket;
        if(hfast->faster[faster->l1] == NULL)
          hfast->faster[faster->l1] = calloc(256, sizeof(uint16_t));
        hfast->faster[faster->l1][faster->l2] = i+1;
      }
    }
  }
  else { // found
    /* Just need to update the counters */
    uint64_t newval = hist->bvs[idx].count + count;
    if(newval < hist->bvs[idx].count) /* we rolled */
      newval = ~(uint64_t)0;
    count = newval - hist->bvs[idx].count;
    hist->bvs[idx].count = newval;
  }
  return count;
}

uint64_t
hist_insert(histogram_t *hist, double val, uint64_t count) {
  return hist_insert_raw(hist, double_to_hist_bucket(val), count);
}

uint64_t
hist_insert_intscale(histogram_t *hist, int64_t val, int scale, uint64_t count) {
  return hist_insert_raw(hist, int_scale_to_hist_bucket(val, scale), count);
}

uint64_t
hist_remove(histogram_t *hist, double val, uint64_t count) {
  hist_bucket_t hb;
  int idx;
  hb = double_to_hist_bucket(val);
  if(hist_internal_find(hist, hb, &idx)) {
    uint64_t newval = hist->bvs[idx].count - count;
    if(newval > hist->bvs[idx].count) newval = 0; /* we rolled */
    count = hist->bvs[idx].count - newval;
    hist->bvs[idx].count = newval;
    return count;
  }
  return 0;
}

uint64_t
hist_sample_count(const histogram_t *hist) {
  int i;
  uint64_t total = 0, last = 0;
  for(i=0;i<hist->used;i++) {
    last = total;
    total += hist->bvs[i].count;
    if(total < last) return ~((uint64_t)0);
  }
  return total;
}

int
hist_bucket_count(const histogram_t *hist) {
  return hist ? hist->used : 0;
}

int
hist_bucket_idx(const histogram_t *hist, int idx,
                double *bucket, uint64_t *count) {
  if(idx < 0 || idx >= hist->used) return 0;
  *bucket = hist_bucket_to_double(hist->bvs[idx].bucket);
  *count = hist->bvs[idx].count;
  return 1;
}

int
hist_bucket_idx_bucket(const histogram_t *hist, int idx,
                       hist_bucket_t *bucket, uint64_t *count) {
  if(idx < 0 || idx >= hist->used) return 0;
  *bucket = hist->bvs[idx].bucket;
  *count = hist->bvs[idx].count;
  return 1;
}

static int
hist_needed_merge_size_fc(histogram_t **hist, int cnt,
                          void (*f)(histogram_t *tgt, int tgtidx,
                                    histogram_t *src, int srcidx),
                          histogram_t *tgt) {
  unsigned short *idx;
  int i, count = 0;
  idx = alloca(cnt * sizeof(*idx));
  memset(idx, 0, cnt * sizeof(*idx));
  while(1) {
    hist_bucket_t smallest = { .exp = 0, .val = 0 };
    for(i=0;i<cnt;i++)
      if(hist[i] != NULL && idx[i] < hist[i]->used) {
        smallest = hist[i]->bvs[idx[i]].bucket;
        break;
      }
    if(i == cnt) break; /* there is no min -- no items */
    for(;i<cnt;i++) { /* see if this is the smallest. */
      if(hist[i] != NULL && idx[i] < hist[i]->used)
        if(hist_bucket_cmp(smallest, hist[i]->bvs[idx[i]].bucket) < 0)
          smallest = hist[i]->bvs[idx[i]].bucket;
    }
    /* Now zip back through and advanced all smallests */
    for(i=0;i<cnt;i++) {
      if(hist[i] != NULL && idx[i] < hist[i]->used &&
          hist_bucket_cmp(smallest, hist[i]->bvs[idx[i]].bucket) == 0) {
        if(f) f(tgt, count, hist[i], idx[i]);
        idx[i]++;
      }
    }
    count++;
  }
  return count;
}

static void
internal_bucket_accum(histogram_t *tgt, int tgtidx,
                      histogram_t *src, int srcidx) {
  uint64_t newval;
  assert(tgtidx < tgt->allocd);
  if(tgt->used == tgtidx) {
    tgt->bvs[tgtidx].bucket = src->bvs[srcidx].bucket;
    tgt->used++;
  }
  assert(hist_bucket_cmp(tgt->bvs[tgtidx].bucket,
                         src->bvs[srcidx].bucket) == 0);
  newval = tgt->bvs[tgtidx].count + src->bvs[srcidx].count;
  if(newval < tgt->bvs[tgtidx].count) newval = ~(uint64_t)0;
  tgt->bvs[tgtidx].count = newval;
}

static int
hist_needed_merge_size(histogram_t **hist, int cnt) {
  return hist_needed_merge_size_fc(hist, cnt, NULL, NULL);
}

int
hist_accumulate(histogram_t *tgt, const histogram_t* const *src, int cnt) {
  int tgtneeds;
  void *oldtgtbuff = tgt->bvs;
  histogram_t tgt_copy;
  histogram_t **inclusive_src = alloca(sizeof(histogram_t *) * (cnt+1));
  memcpy(&tgt_copy, tgt, sizeof(*tgt));
  memcpy(inclusive_src, src, sizeof(*src)*cnt);
  inclusive_src[cnt] = &tgt_copy;
  tgtneeds = hist_needed_merge_size(inclusive_src, cnt+1);
  tgt->allocd = tgtneeds;
  tgt->used = 0;
  tgt->bvs = calloc(tgt->allocd, sizeof(*tgt->bvs));
  hist_needed_merge_size_fc(inclusive_src, cnt+1, internal_bucket_accum, tgt);
  if(oldtgtbuff) free(oldtgtbuff);
  return tgt->used;
}

int
hist_num_buckets(const histogram_t *hist) {
  return hist->used;
}

void
hist_clear(histogram_t *hist) {
  int i;
  for(i=0;i<hist->used;i++)
    hist->bvs[i].count = 0;
  if(hist->fast) {
    struct histogram_fast *hfast = (struct histogram_fast *)hist;
    for(i=0;i<256;i++) {
      if(hfast->faster[i]) {
        memset(hfast->faster[i], 0, 256 * sizeof(uint16_t));
      }
    }
  }
}

histogram_t *
hist_alloc() {
  return hist_alloc_nbins(0);
}

histogram_t *
hist_alloc_nbins(int nbins) {
  histogram_t *tgt;
  if(nbins < 1) nbins = DEFAULT_HIST_SIZE;
  if(nbins > MAX_HIST_BINS) nbins = MAX_HIST_BINS;
  tgt = calloc(1, sizeof(histogram_t));
  tgt->allocd = nbins;
  tgt->bvs = calloc(tgt->allocd, sizeof(*tgt->bvs));
  return tgt;
}

histogram_t *
hist_fast_alloc() {
  return hist_fast_alloc_nbins(0);
}

histogram_t *
hist_fast_alloc_nbins(int nbins) {
  histogram_t *tgt;
  if(nbins < 1) nbins = DEFAULT_HIST_SIZE;
  if(nbins > MAX_HIST_BINS) nbins = MAX_HIST_BINS;
  tgt = calloc(1, sizeof(struct histogram_fast));
  tgt->allocd = nbins;
  tgt->bvs = calloc(tgt->allocd, sizeof(*tgt->bvs));
  tgt->fast = 1;
  return tgt;
}

void
hist_free(histogram_t *hist) {
  if(hist == NULL) return;
  if(hist->bvs != NULL) free(hist->bvs);
  if(hist->fast) {
    int i;
    struct histogram_fast *hfast = (struct histogram_fast *)hist;
    for(i=0;i<256;i++) free(hfast->faster[i]);
  }
  free(hist);
}

histogram_t *
hist_compress_mbe(histogram_t *hist, int8_t mbe) {
  histogram_t *hist_compressed = hist_alloc();
  int total = hist_bucket_count(hist);
  for(int idx=0; idx<total; idx++) {
    struct hist_bv_pair bv = hist->bvs[idx];
    // we know that stored buckets are valid (abs(val)>=10)
    // so it suffices to check the exponent
    if (bv.bucket.exp < mbe) {
      // merge into zero bucket
      hist_insert_raw(hist_compressed, (hist_bucket_t) {.exp = 0, .val = 0}, bv.count);
    }
    else if (bv.bucket.exp == mbe) {
      // re-bucket to val = 10, 20, ... 90
      hist_insert_raw(hist_compressed, (hist_bucket_t) {
        .exp = bv.bucket.exp,
        .val = (bv.bucket.val/10) * 10
      }, bv.count);
    }
    else {
      // copy over
      hist_insert_raw(hist_compressed, bv.bucket, bv.count);
    }
  }
  return hist_compressed;
}
