#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <circllhist.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>

typedef histogram_t *(*halloc_func)();
typedef int (*accumulate_fn)(histogram_t *, const histogram_t * const *, int);

halloc_func halloc = NULL;

histogram_t *build(histogram_t *out, double *vals, int nvals) {
  int i;
  if(out == NULL) out = halloc();
  for(i=0;i<nvals;i++)
    hist_insert(out, vals[i], 1);
  return out;
}
double *buildNvals(int n) {
  int i;
  double *vals = malloc(sizeof(*vals) * n);
  for(i=0;i<n;i++) {
    vals[i] = (0.1 + (double)(i % 10)) * pow(10,(i/10));
  }
  return vals;
}

struct sval {
  int64_t val;
  int scale;
};
histogram_t *buildI(histogram_t *out, struct sval *vals, int nvals) {
  int i;
  if(out == NULL) out = halloc();
  for(i=0;i<nvals;i++)
    hist_insert_raw(out, int_scale_to_hist_bucket(vals[i].val, vals[i].scale), 1);
  return out;
}
struct sval *buildNIvalsOffset(int n, int offset) {
  int i;
  struct sval *vals = malloc(sizeof(*vals) * n);
  for(i=0;i<n;i++) {
    int bucket = offset + i;
    vals[i].val = ((bucket%90)+10);
    vals[i].scale = bucket/90;
  }
  return vals;
}
struct sval *buildNIvals(int n) {
  return buildNIvalsOffset(n, 0);
}

static void
benchmark_accumulate(const char *hist_type, const char *accumulate_type,
                     accumulate_fn accumulate, int iter, int size,
                     int source_count, bool overlap) {
  struct timeval start, finish;
  histogram_t **sources = malloc(source_count * sizeof(*sources));
  histogram_t *target = halloc();

  for(int i=0; i<source_count; i++) {
    int offset = overlap ? 0 : i * size;
    struct sval *vals = buildNIvalsOffset(size, offset);
    sources[i] = buildI(NULL, vals, size);
    free(vals);
  }

  // Warm up the target before timing steady-state merges. For disjoint
  // sources, the target then contains every source bucket, so the timed
  // loop does not measure repeated growth. benchmark_accumulate_growth()
  // covers the growth-heavy worst case separately.
  accumulate(target, (const histogram_t * const *)sources, source_count);
  gettimeofday(&start, NULL);
  for(int idx=0; idx<iter; idx++)
    accumulate(target, (const histogram_t * const *)sources, source_count);
  gettimeofday(&finish, NULL);

  int expected_buckets = overlap ? size : size * source_count;
  if(hist_num_buckets(target) != expected_buckets) abort();
  double elapsed = finish.tv_sec - start.tv_sec;
  elapsed += (finish.tv_usec/1000000.0) - (start.tv_usec/1000000.0);
  printf("accumulate,%s,%s,%s,%d,%d,%d,%0.2f\n",
         hist_type, accumulate_type, overlap ? "overlap" : "disjoint",
         source_count, iter, size,
         (elapsed / (double)iter) * 1000000000.0);

  hist_free(target);
  for(int i=0; i<source_count; i++) hist_free(sources[i]);
  free(sources);
}

static hist_bucket_t
bucket_at(int idx) {
  const int negative_buckets = 90 * 256;
  if(idx == 0) return double_to_hist_bucket(NAN);
  idx--;
  if(idx < negative_buckets) {
    hist_bucket_t bucket = {
      .val = -99 + (idx % 90),
      .exp = 127 - (idx / 90)
    };
    return bucket;
  }
  idx -= negative_buckets;
  if(idx == 0) return (hist_bucket_t) { .val = 0, .exp = 0 };
  idx--;
  return (hist_bucket_t) {
    .val = 10 + (idx % 90),
    .exp = -128 + (idx / 90)
  };
}

/*
 * Measure regular one-bucket-at-a-time growth of the target histogram.
 * This is the incremental accumulator's worst case because each merge scans
 * the increasingly large target, and ascending and descending orders cover
 * growth at either end of the bucket range.
 */
static void
benchmark_accumulate_growth(const char *order, bool ascending,
                            const char *accumulate_type, accumulate_fn accumulate) {
  const int max_buckets = 2 + 2 * 90 * 256;
  struct timeval start, finish;
  histogram_t *source = hist_alloc_nbins(1);
  histogram_t *target = hist_alloc();
  const histogram_t *sources[] = { source };

  gettimeofday(&start, NULL);
  for(int i=0; i<max_buckets; i++) {
    int bucket_idx = ascending ? i : max_buckets - i - 1;
    hist_clear(source);
    hist_insert_raw(source, bucket_at(bucket_idx), 1);
    if(accumulate(target, sources, 1) < 0) abort();
  }
  gettimeofday(&finish, NULL);

  if(hist_num_buckets(target) != max_buckets) abort();
  double elapsed = finish.tv_sec - start.tv_sec;
  elapsed += (finish.tv_usec/1000000.0) - (start.tv_usec/1000000.0);
  printf("accumulate-growth-worst,%s,%s,%d,%0.0f,%0.2f\n",
         order, accumulate_type, max_buckets, elapsed * 1000000000.0,
         (elapsed / max_buckets) * 1000000000.0);

  hist_free(target);
  hist_free(source);
}

static volatile uint64_t summary_sink;

static const double summary_bounds[] = {
  0.5, 1, 5, 10, 25, 50, 100, 250, 500, 1000,
  2500, 5000, 10000, 30000, 60000, 300000, 600000, 1800000, 3600000
};

static void
benchmark_summary(const char *hist_type, const char *path, const histogram_t *hist,
                  const double *bounds, int bound_count, int iter) {
  uint64_t cumulative[sizeof(summary_bounds) / sizeof(*summary_bounds)];
  struct timeval start, finish;
  uint64_t total = 0;
  uint64_t overflow = 0;
  double sum = 0.0;

  gettimeofday(&start, NULL);
  for (int i = 0; i < iter; ++i) {
    if (strcmp(path, "separate") == 0) {
      total = hist_sample_count(hist);
      sum = hist_approx_sum(hist);
      hist_approx_count_below_inclusive_many(hist, bounds, bound_count, cumulative);
      overflow = bound_count > 0
                     ? hist_approx_count_above(hist, bounds[bound_count - 1])
                     : total;
    } else {
      hist_approx_summary_t result =
          hist_approx_summary(hist, bounds, bound_count, cumulative);
      assert(result.status == 0);
      total = result.sample_count;
      sum = result.sample_sum;
      overflow = bound_count > 0 ? total - cumulative[bound_count - 1] : total;
    }
    summary_sink += total + overflow + cumulative[bound_count - 1] + (uint64_t)sum;
  }
  gettimeofday(&finish, NULL);

  double elapsed = finish.tv_sec - start.tv_sec;
  elapsed += (finish.tv_usec / 1000000.0) - (start.tv_usec / 1000000.0);
  printf("summary,%s,%s,%d,%d,%0.2f\n", hist_type, path, hist_num_buckets(hist),
         iter, (elapsed / (double)iter) * 1000000000.0);
}

static void
benchmark_summary_percent(const char *hist_type, const char *path, const histogram_t *hist,
                          const double *bounds, int bound_count, double scale, int iter) {
  double scaled_bounds[sizeof(summary_bounds) / sizeof(*summary_bounds)];
  uint64_t cumulative[sizeof(summary_bounds) / sizeof(*summary_bounds)];
  struct timeval start, finish;
  uint64_t total = 0;
  uint64_t overflow = 0;
  double sum = 0.0;

  for (int i = 0; i < bound_count; ++i) scaled_bounds[i] = bounds[i] * scale;

  gettimeofday(&start, NULL);
  for (int i = 0; i < iter; ++i) {
    if (strcmp(path, "separate") == 0) {
      total = hist_sample_count(hist);
      sum = hist_approx_sum(hist) / scale;
      for (int j = 0; j < bound_count; ++j)
        cumulative[j] = hist_approx_count_below(hist, scaled_bounds[j]);
      overflow = hist_approx_count_above(hist, scaled_bounds[bound_count - 1]);
    } else {
      hist_approx_summary_t result =
          hist_approx_summary(hist, scaled_bounds, bound_count, cumulative);
      assert(result.status == 0);
      total = result.sample_count;
      sum = result.sample_sum / scale;
      overflow = total - cumulative[bound_count - 1];
    }
    summary_sink += total + overflow + cumulative[bound_count - 1] + (uint64_t)sum;
  }
  gettimeofday(&finish, NULL);

  double elapsed = finish.tv_sec - start.tv_sec;
  elapsed += (finish.tv_usec / 1000000.0) - (start.tv_usec / 1000000.0);
  printf("summary-percent,%s,%s,%d,%d,%0.2f\n", hist_type, path,
         hist_num_buckets(hist), iter, (elapsed / (double)iter) * 1000000000.0);
}

const int iters[] = { 100, 10000, 100000 };
const int sizes[] = { 31, 127, 255 };
int main() {
  int i, s;
  for(i=0;i<sizeof(iters)/sizeof(*iters);i++) {
    for(s=0;s<sizeof(sizes)/sizeof(*sizes);s++) {
      struct timeval start, finish;
      histogram_t *hist = NULL;
      int idx, ai;
      int iter = iters[i];
      int size = sizes[s];
      long cnt = 0;
      for(ai=0;ai<2;ai++) {
        halloc = (ai%2 == 0) ? hist_alloc: hist_fast_alloc;

{ // double
      cnt = 0;
      hist = NULL;
      printf("%s,%d,%d,%d,",
             (ai%2 == 0) ? "normal" : "fast",
             iter, (size*iter), size);
      double *vals = buildNvals(size);
      hist = build(hist, vals, size);
      gettimeofday(&start, NULL);
      for(idx=0; idx<iter; idx++) {
        hist = build(hist, vals, size);
        cnt += size;
      }
      assert(hist_num_buckets(hist) == size);
      gettimeofday(&finish, NULL);
      double elapsed = finish.tv_sec - start.tv_sec;
      elapsed += (finish.tv_usec/1000000.0) - (start.tv_usec/1000000.0);
      if(cnt != 0)
        printf("%0.2f\n",
               (elapsed / (double)cnt) * 1000000000.0);
      else 
        printf("cannot calculate benchmark, no work done!\n");
      hist_free(hist);
      free(vals);
}
{ // int
      hist = NULL;
      cnt = 0;
      printf("%s,%d,%d,%d,",
             (ai%2 == 0) ? "normal" : "fast",
             iter, (size*iter), size);
      struct sval *vals = buildNIvals(size);
      hist = buildI(hist, vals, size);
      gettimeofday(&start, NULL);
      for(idx=0; idx<iter; idx++) {
        hist = buildI(hist, vals, size);
        cnt += size;
      }
      assert(hist_num_buckets(hist) == size);
      gettimeofday(&finish, NULL);
      double elapsed = finish.tv_sec - start.tv_sec;
      elapsed += (finish.tv_usec/1000000.0) - (start.tv_usec/1000000.0);
      if(cnt != 0)
        printf("%0.2f\n",
               (elapsed / (double)cnt) * 1000000000.0);
      else
        printf("cannot calculate benchmark, no work done!\n");
      hist_free(hist);
      free(vals);
}
      const int source_counts[] = { 1, 8, 32 };
      for(int overlap=0; overlap<2; overlap++) {
        for(int source_idx=0;
            source_idx<sizeof(source_counts)/sizeof(*source_counts);
            source_idx++) {
          int source_count = source_counts[source_idx];
          int accumulate_iter = iter / (source_count * source_count);
          if(accumulate_iter < 10) accumulate_iter = 10;
          benchmark_accumulate((ai%2 == 0) ? "normal" : "fast", "accumulate",
                               hist_accumulate, accumulate_iter, size,
                               source_count, overlap);
        }
      }
      }
    }
  }

  const int summary_sizes[] = { 1, 2, 4, 8, 16, 32, 64, 127 };
  for (int ai = 0; ai < 2; ++ai) {
    halloc = (ai == 0) ? hist_alloc : hist_fast_alloc;
    const char *hist_type = (ai == 0) ? "normal" : "fast";
    for (size_t size_idx = 0; size_idx < sizeof(summary_sizes) / sizeof(*summary_sizes);
         ++size_idx) {
      const int size = summary_sizes[size_idx];
      struct sval *vals = buildNIvals(size);
      histogram_t *hist = buildI(NULL, vals, size);
      free(vals);
      int iter = 100000;
      benchmark_summary(hist_type, "separate", hist, summary_bounds,
                        sizeof(summary_bounds) / sizeof(*summary_bounds), iter);
      benchmark_summary(hist_type, "fused", hist, summary_bounds,
                        sizeof(summary_bounds) / sizeof(*summary_bounds), iter);
      benchmark_summary_percent(hist_type, "separate", hist, summary_bounds,
                                sizeof(summary_bounds) / sizeof(*summary_bounds), 100.0, iter);
      benchmark_summary_percent(hist_type, "fused", hist, summary_bounds,
                                sizeof(summary_bounds) / sizeof(*summary_bounds), 100.0, iter);
      hist_free(hist);
    }
  }

  benchmark_accumulate_growth("ascending", true, "accumulate", hist_accumulate);
  benchmark_accumulate_growth("descending", false, "accumulate", hist_accumulate);
}
