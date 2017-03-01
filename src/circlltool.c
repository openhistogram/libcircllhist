#include "circllhist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void
histL_print(histogram_t *h) {
  int total = hist_bucket_count(h);
  for(int i=0; i<total; i++) {
    hist_bucket_t b;
    uint64_t bc;
    hist_bucket_idx_bucket(h, i, &b, &bc);
    double B = hist_bucket_to_double(b);
    double W = hist_bucket_to_double_bin_width(b);
    double l = B >= 0 ? B     : B - W;
    double r = B >= 0 ? B + W : B;
    printf("H[%g..%g]\t%llu\t|", l, r, bc);
    for(int j=0; j< (int)(bc); j++){
      putchar('#');
    }
    putchar('\n');
  }
}

char TBUF[1024] = {0};
char* gettok() {
  int i=0;
  int c;
  while ((c = getchar()) != EOF) {
    if(!(c==' ' || c == '\t' || c == '\n')) {
      TBUF[i++] = c;
    }
    else if (i==0) {
      // skip
    }
    else {
      TBUF[i] = 0;
      return TBUF;
    }
  }
  return NULL;
}

int
main() {
  histogram_t *h = hist_alloc();
  char *tok = NULL;
  while ((tok = gettok())!= NULL) {
    switch(*tok) {
    case 'p': //! p :  print
      histL_print(h);
      break;
    case 'j': //! j : print JSON representation
      do {
        char * json = hist_serialize_json(h);
        printf("%s\n", json);
        free(json);
      } while (0);
      break;
    case 'b': //! q : quit
      printf("%d\n", hist_bucket_count(h));
      break;
    case 'q': //! q : quit
      return 0;
      break;
    case 's': //! s : print serialization (base64-encoded)
      do {
        size_t len = hist_serialize_b64_estimate(h);
        char *buf = malloc(len);
        hist_serialize_b64(h, buf, len);
        fwrite(buf, len, 1, stdout);
        printf("\n");
      } while(0);
      break;
    case 'x': //! x %d : compress histogram with given mbe
      do {
        char *tok = gettok();
        char *endptr;
        long trunc = strtol(tok, &endptr, 10);
        if(endptr == tok) { // error
          fprintf(stderr, "Invalid mbe: %s\n", tok);
        } else {
          histogram_t *g = hist_compress_mbe(h, (int8_t) trunc);
          hist_free(h);
          h = g;
        }
      } while(0);
      break;
    default:
      do {
        double val;
        if(sscanf(tok, "%lf", &val)) {
          hist_insert(h, val, 1);
        } else {
          fprintf(stderr, "Unknown command: %s", tok);
        }
      } while (0);
    }
  }
  return 0;
}
