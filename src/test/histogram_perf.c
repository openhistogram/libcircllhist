#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <circllhist.h>
#include <sys/time.h>
#include <assert.h>
#include <mtev_defines.h>

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
      printf("Running %d iters over %d values in %d bins...\n",
             iter, (size*iter), size);
      double *vals = buildNvals(size);
      gettimeofday(&start, NULL);
      for(idx=0; idx<iter; idx++) {
        hist = build(hist, vals, size);
        cnt += size;
      }
      gettimeofday(&finish, NULL);
      double elapsed = sub_timeval_d(finish, start);
      printf("ops: %ld, time: %gs, time-per-op: %0.2fns\n",
             cnt, elapsed, (elapsed / (double)cnt) * 1000000000.0);
      hist_free(hist);
      free(vals);
    }
  }
}
