#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <circllhist.h>
#include <sys/time.h>
#include <assert.h>

histogram_t *build(histogram_t *out, double *vals, int nvals) {
  int i;
  if(out == NULL) out = hist_alloc();
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
  if(out == NULL) out = hist_alloc();
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

const int iters[] = { 10, 10000, 1000000 };
const int sizes[] = { 31, 127, 255 };
int main() {
  int i, s;
  for(i=0;i<sizeof(iters)/sizeof(*iters);i++) {
    for(s=0;s<sizeof(sizes)/sizeof(*sizes);s++) {
      struct timeval start, finish;
      histogram_t *hist = NULL;
      int idx;
      int iter = iters[i];
      int size = sizes[s];
      long cnt = 0;

{ // double
      printf("Running %d double iters over %d values in %d bins...\n",
             iter, (size*iter), size);
      double *vals = buildNvals(size);
      gettimeofday(&start, NULL);
      for(idx=0; idx<iter; idx++) {
        hist = build(hist, vals, size);
        cnt += size;
      }
      assert(hist_num_buckets(hist) == size);
      gettimeofday(&finish, NULL);
      double elapsed = finish.tv_sec - start.tv_sec;
      elapsed += (finish.tv_usec/1000000.0) - (start.tv_usec/1000000.0);
      printf("ops: %ld, time: %gs, time-per-op: %0.2fns\n",
             cnt, elapsed, (elapsed / (double)cnt) * 1000000000.0);
      hist_free(hist);
      free(vals);
}
{ // int
      hist = NULL;
      printf("Running %d int iters over %d values in %d bins...\n",
             iter, (size*iter), size);
      struct sval *vals = buildNIvals(size);
      gettimeofday(&start, NULL);
      for(idx=0; idx<iter; idx++) {
        hist = buildI(hist, vals, size);
        cnt += size;
      }
      assert(hist_num_buckets(hist) == size);
      gettimeofday(&finish, NULL);
      double elapsed = finish.tv_sec - start.tv_sec;
      elapsed += (finish.tv_usec/1000000.0) - (start.tv_usec/1000000.0);
      printf("ops: %ld, time: %gs, time-per-op: %0.2fns\n",
             cnt, elapsed, (elapsed / (double)cnt) * 1000000000.0);
      hist_free(hist);
      free(vals);
}

    }
  }
}
