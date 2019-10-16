#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include "circllhist.h"

enum ofmt {
  JSON
} output_format;

void help() {
  printf("\t-h\t\thelp\n");
  printf("\t-f <type>\toutput format\n");
  printf("\t[hist1 [hist2 [...]]]\n\n");
  printf("If no hists are specified, stdin is read\n");
}

void print_hist(histogram_t *hist) {
  switch(output_format) {
    case JSON:
      printf("{");
      int cnt = hist_bucket_count(hist);
      for(int i=0; i<cnt; i++) {
        double v;
        uint64_t vc;
        hist_bucket_idx(hist, i, &v, &vc);
        printf("%s\"%g\":%zu", i ? "," : "", v, vc);
      }
      printf("}\n");
      break;
  }
}

void decode_print(char *buff) {
  histogram_t *hist = hist_alloc();
  if(hist_deserialize_b64(hist, buff, strlen(buff)) <= -1) {
    fprintf(stderr, "histogram invalid\n");
  }
  else {
    print_hist(hist);
  }
  hist_free(hist);
}

int main(int argc, char **argv) {
  char buff[256*1024];
  int opt;
  while((opt = getopt(argc, argv, "hf:")) != -1) {
    switch(opt) {
    case 'f':
      if(!strcmp(optarg, "json")) output_format = JSON;
      else {
        fprintf(stderr, "unrecognized format: %s\n", optarg);
        exit(-1);
      }
      break;
    case 'h':
      help();
      exit(0);
    default:
      fprintf(stderr, "unrecognized option: -%c\n", opt);
      exit(-1);
      break;
    }
  }
  if(optind < argc) {
    for(int i=optind; i<argc; i++) {
      if(strlen(argv[i]) > sizeof(buff)-1) {
        fprintf(stderr, "histogram too large\n");
        exit(-1);
      }
      decode_print(argv[i]);
    }
  } else {
    while(NULL != fgets(buff, sizeof(buff), stdin)) {
      decode_print(buff);
    }
  }
  return 0;
}
