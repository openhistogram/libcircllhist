/** \file circllhist.h */
/*
 * Copyright (c) 2016-2021, Circonus, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/*! \mainpage A C implementation of Circonus log-linear histograms
* \ref circllhist.h
*/

#ifndef CIRCLLHIST_H
#define CIRCLLHIST_H

#if defined(WIN32)
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

#ifdef __cplusplus
extern "C" { /* FFI_SKIP */
#endif

#define DEFAULT_HIST_SIZE 100
//! Maximal size of hist bucket standard string format (inc. terminator)
#define HIST_BUCKET_MAX_STRING_SIZE 9
#define API_EXPORT(type) extern type
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>

typedef struct histogram histogram_t;
typedef struct hist_rollup_config hist_rollup_config_t;
//! A hist_bucket structure represents a histogram bucket with the following
//! dimensions:
//! - (val < -99 || 99 < val) => Invalid bucket
//! - (-10 < val && val < 10) => (-10^-127 .. +10^-127) zero bucket
//! - val > 0 => [ (val/10)*10^exp .. (val+1)/10*10^exp )
//! - val < 0 => ( (val-1)/10*10^exp .. (val/10)*10^exp ]
typedef struct hist_bucket {
  int8_t val; //!< value * 10
  int8_t exp; //!< exponent -128 .. 127
} hist_bucket_t;

typedef struct hist_allocator {
  void *(*malloc)(size_t);
  void *(*calloc)(size_t, size_t);
  void (*free)(void *);
} hist_allocator_t;

////////////////////////////////////////////////////////////////////////////////
// Histogram buckets

//! Returns the edge of the histogram bucket closer to zero
API_EXPORT(double) hist_bucket_to_double(hist_bucket_t hb);
//! Calculate mid-point of the bucket
API_EXPORT(double) hist_bucket_midpoint(hist_bucket_t in);
//! Get the width of the hist_bucket
API_EXPORT(double) hist_bucket_to_double_bin_width(hist_bucket_t hb);
//! Create the bucket that a value belongs to
API_EXPORT(hist_bucket_t) double_to_hist_bucket(double d);
//! Create the bucket that value * 10^(scale) belongs to
API_EXPORT(hist_bucket_t) int_scale_to_hist_bucket(int64_t value, int scale);
//! Writes a standardized string representation to buf
//! Buf must be of size HIST_BUCKET_MAX_STRING_SIZE or larger.
//! \return of characters of bytes written into the buffer excluding terminator
//!
//! Format spec: "sxxetyyy", where
//! - s = '+' or '-' global sign
//! - xx -- two digits representing val as decimal integer (in 10 .. 99)
//! - e = 'e' literal character
//! - t = '+' or '-' exponent sign
//! - yyy -- three digits representing exp as decimal integer with leading 0s
//!
//! Exception: The zero bucket is represented  as "0"
//! Exception: Invalid buckets are represented as "NaN"
//!
//! Examples:
//!      1  => '+10e-001'; 12    => '+12e+000';
//!  -0.23  => '-23e-003'; 23000 => '+23e+003';
API_EXPORT(int) hist_bucket_to_string(hist_bucket_t hb, char *buf);

////////////////////////////////////////////////////////////////////////////////
// Creating and destroying histograms

//! Create a new histogram, uses default allocator
API_EXPORT(histogram_t *) hist_alloc(void);
//! Create a new histogram with preallocated bins, uses default allocator
API_EXPORT(histogram_t *) hist_alloc_nbins(int nbins);
//! Create a fast-histogram
/*! Fast allocations consume 2kb + N * 512b more memory
 *  where N is the number of used exponents.  It allows for O(1) increments for
 *  prexisting keys, uses default allocator */
API_EXPORT(histogram_t *) hist_fast_alloc(void);
//! Create a fast-histogram with preallocated bins, uses default allocator
API_EXPORT(histogram_t *) hist_fast_alloc_nbins(int nbins);
//! Create an exact copy of other, uses default allocator
API_EXPORT(histogram_t *) hist_clone(const histogram_t *other);

//! Create a new histogram, uses custom allocator
API_EXPORT(histogram_t *) hist_alloc_with_allocator(const hist_allocator_t *alloc);
//! Create a new histogram with preallocated bins, uses custom allocator
API_EXPORT(histogram_t *) hist_alloc_nbins_with_allocator(int nbins, const hist_allocator_t *alloc);
//! Create a fast-histogram
/*! Fast allocations consume 2kb + N * 512b more memory
 *  where N is the number of used exponents.  It allows for O(1) increments for
 *  prexisting keys, uses custom allocator */
API_EXPORT(histogram_t *) hist_fast_alloc_with_allocator(const hist_allocator_t *alloc);
//! Create a fast-histogram with preallocated bins, uses custom allocator
API_EXPORT(histogram_t *) hist_fast_alloc_nbins_with_allocator(int nbins, const hist_allocator_t *alloc);
//! Create an exact copy of other, uses custom allocator
API_EXPORT(histogram_t *) hist_clone_with_allocator(const histogram_t *other, const hist_allocator_t *alloc);

//! Free a (fast-) histogram, frees with allocator chosen during the alloc/clone
API_EXPORT(void) hist_free(histogram_t *hist);

////////////////////////////////////////////////////////////////////////////////
// Getting data in and out of histograms

/*! Inserting double values converts from IEEE double to a small static integer
 *  base and can suffer from floating point math skew.  Using the intscale
 *  variant is more precise and significantly faster if you already have
 *  integer measurements. */
//! insert a value into a histogram count times
API_EXPORT(uint64_t) hist_insert(histogram_t *hist, double val, uint64_t count);
//! Remove data from a histogram count times, returns the actual count removed protecting from underflow
API_EXPORT(uint64_t) hist_remove(histogram_t *hist, double val, uint64_t count);
//! Remove data from a histogram count times, returns the actual count removed protecting from underflow
API_EXPORT(uint64_t) hist_remove_raw(histogram_t *hist, hist_bucket_t hb, uint64_t count);
//! Insert a single bucket + count into a histogram
//!
//! Updates counts if the bucket exists
//! Handles re-allocation of new buckets if needed
API_EXPORT(uint64_t) hist_insert_raw(histogram_t *hist, hist_bucket_t hb, uint64_t count);
//! Like hist_insert_raw, but optimizes for the case that this bin does not exist, there is room, and is
//! larger than any existing bin.
API_EXPORT(uint64_t) hist_insert_raw_end(histogram_t *hist, hist_bucket_t hb, uint64_t count);
//! Get the number of used buckets in a histogram
API_EXPORT(int) hist_bucket_count(const histogram_t *hist);
//! Same as hist_bucket_count
API_EXPORT(int) hist_num_buckets(const histogram_t *hist);
//! Get the total number of values stored in the histogram
API_EXPORT(uint64_t) hist_sample_count(const histogram_t *hist);
//! Get value+count for bucket at position idx. Valid positions are 0 .. hist_bucket_count()
API_EXPORT(int) hist_bucket_idx(const histogram_t *hist, int idx, double *v, uint64_t *c);
//! Get bucket+count for bucket at position idx. Valid positions are 0 .. hist_bucket_count()
API_EXPORT(int) hist_bucket_idx_bucket(const histogram_t *hist, int idx, hist_bucket_t *b, uint64_t *c);
//! Accumulate bins from each of cnt histograms in src onto tgt
API_EXPORT(int) hist_accumulate(histogram_t *tgt, const histogram_t * const *src, int cnt);
//! Subtract bins from each of cnt histograms in src from tgt, return -1 on underrun error
API_EXPORT(int) hist_subtract(histogram_t *tgt, const histogram_t * const *src, int cnt);
//! Subtract bins in src from tgt treating the result count as signed, return -1 on overflow error
API_EXPORT(int) hist_subtract_as_int64(histogram_t *tgt, const histogram_t *src);
//! Add bins in src from tgt treating src counts as signed, return -1 on overflow error
API_EXPORT(int) hist_add_as_int64(histogram_t *tgt, const histogram_t *src);
//! Downsample a histogram to a certain factor.
API_EXPORT(void) hist_downsample(histogram_t *tgt, double factor);
//! Clear data fast. Keeps buckets allocated.
API_EXPORT(void) hist_clear(histogram_t *hist);
//! Insert a value into a histogram value = val * 10^(scale)
API_EXPORT(uint64_t) hist_insert_intscale(histogram_t *hist, int64_t val, int scale, uint64_t count);

////////////////////////////////////////////////////////////////////////////////
// Serialization

//! Serialize histogram to binary data
API_EXPORT(ssize_t) hist_serialize(const histogram_t *h, void *buff, ssize_t len);
API_EXPORT(ssize_t) hist_deserialize(histogram_t *h, const void *buff, ssize_t len);
API_EXPORT(ssize_t) hist_serialize_estimate(const histogram_t *h);
//! Return histogram serialization as base64 encoded string
API_EXPORT(ssize_t) hist_serialize_b64(const histogram_t *h, char *b64_serialized_histo_buff, ssize_t buff_len);
API_EXPORT(ssize_t) hist_deserialize_b64(histogram_t *h, const void *b64_string, ssize_t b64_string_len);
API_EXPORT(ssize_t) hist_serialize_b64_estimate(const histogram_t *h);

API_EXPORT(void) hist_remove_zeroes(histogram_t *h);
//! Compress histogram by squshing together adjacent buckets
//!
//! This compression is lossy. mean/quantiles will be affected by compression.
//! Intended use cases is visualization.
//! \param hist
//! \param mbe the Minimum Bucket Exponent
//! \return the compressed histogram as new value
API_EXPORT(histogram_t *) hist_compress_mbe(const histogram_t *h, int8_t mbe);

////////////////////////////////////////////////////////////////////////////////
// Analytics

//! Approximate mean value of all values stored in the histogram
API_EXPORT(double) hist_approx_mean(const histogram_t *);
//! Approximate the sum of all values stored in the histogram
API_EXPORT(double) hist_approx_sum(const histogram_t *);
//! Approximate the standard deviation of all values stored in the histogram
API_EXPORT(double) hist_approx_stddev(const histogram_t *);
//! Approximate the k-th moment of all values stored in the histogram
//! \param hist
//! \param k
API_EXPORT(double) hist_approx_moment(const histogram_t *hist, double k);
//! Modifies the histogram to remove all counts for sample with values outside the provided range.
//! \param hist
//! \param lower
//! \param upper
API_EXPORT(void) hist_clamp(histogram_t *hist, double lower, double upper);
//! Returns the number of values in buckets that are entirely lower than or equal to threshold.  This is complementary to count_below such that above(x) + below(x) == count()
//! \param hist
//! \param threshold
API_EXPORT(uint64_t) hist_approx_count_below(const histogram_t *hist, double threshold);
//! Returns the number of values in buckets greater than the bucket containing the threshold.   This is complementary to count_above such that above(x) + below(x) == count()
//! \param hist
//! \param threshold
API_EXPORT(uint64_t) hist_approx_count_above(const histogram_t *hist, double threshold);
//! Returns the number of samples in the histogram that are in the same bucket as the provided value
//! \param hist
//! \param value
API_EXPORT(uint64_t) hist_approx_count_nearby(const histogram_t *hist, double value);
//! Approiximate n Type-1 quantiles of all values stored in the histogram
//! See Hyndman-Fan, Statistical Computing, 1996, for the definition of Type-1 Quantiles
//! \param *q_in array of quantiles to comute
//! \param nq length of quantile array
//! \param *q_out pre-allocated array where results shall be written to
API_EXPORT(int) hist_approx_quantile(const histogram_t *, const double *q_in, int nq, double *q_out);
//! Approiximate n Type-7 quantiles of all values stored in the histogram
//! See Hyndman-Fan, Statistical Computing, 1996, for the definition of Type-7 Quantiles
//! \param *q_in array of quantiles to comute
//! \param nq length of quantile array
//! \param *q_out pre-allocated array where results shall be written to
API_EXPORT(int) hist_approx_quantile7(const histogram_t *, const double *q_in, int nq, double *q_out);
//! Approiximate n inverse quantiles (ratio below threshold) of all values stored in the histogram
//! \param *iq_in array of inverse quantiles to compute
//! \param niq length of inverse quantile array
//! \param *iq_out pre-allocated array where results shall be written to
API_EXPORT(int) hist_approx_inverse_quantile(const histogram_t *, const double *iq_in, int niq, double *iq_out);

typedef struct {
  uint64_t count;
  double lower;
  double upper;
} histogram_adhoc_bin_t;

typedef enum {
  HIST_APPROX_MID = 0,
  HIST_APPROX_HARMONIC_MEAN,
  HIST_APPROX_HIGH,
  HIST_APPROX_LOW
} histogram_approx_mode_t;

//! Approximate a histogram based on a set of counts in adhoc bins
//! \param node how to choose bins.
//! \param *bins the adhoc bins
//! \param nbins the number of adhoc bins
//! \param sum the sum of all the sample if available, may be used as a hint
API_EXPORT(histogram_t *) hist_create_approximation_from_adhoc(histogram_approx_mode_t mode, const histogram_adhoc_bin_t *bins, size_t nbins, double sum);

#ifdef __cplusplus
} /* FFI_SKIP */
#endif

#endif
