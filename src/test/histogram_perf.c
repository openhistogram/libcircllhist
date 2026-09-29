#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <circllhist.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>

typedef histogram_t *(*halloc_func)();

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
struct sval *buildNIvals(int n) {
  int i;
  struct sval *vals = malloc(sizeof(*vals) * n);
  for(i=0;i<n;i++) {
    vals[i].val = ((i%90)+10);
    vals[i].scale = i/90;
  }
  return vals;
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
}
