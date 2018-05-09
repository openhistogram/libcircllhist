import circllhist.ffi as ffi


class circllbin:
    def __init__(self, b):
        self.b = b

    @classmethod
    def new(cls):
        return cls(ffi.ffi.new("hist_bucket_t*"))

    @classmethod
    def from_number(cls, n):
        return cls(ffi.C.double_to_hist_bucket(n))

    def __str__(self):
        buf = ffi.ffi.new("char[]",9)
        ffi.C.hist_bucket_to_string(self.b, buf)
        return str(ffi.ffi.string(buf), "ASCII")

    def width(self):
        return ffi.C.hist_bucket_to_double_bin_width(self.b)

    def midpoint(self):
        return ffi.C.hist_bucket_midpoint(self.b)

    def edge(self):
        "Returns the edge of the histogram bucket that is closer to zero"
        return ffi.C.hist_bucket_to_double(self.b)


class circllhist:
    def __init__(self):
        self.h = ffi.C.hist_alloc()

    def count(self):
        "Returns the number of samples stored in the histogram"
        return ffi.C.hist_sample_count(self.h)

    def bin_count(self):
        "Returns the number of bins used by the histogram"
        return ffi.C.hist_bucket_count(self.h)

    def insert(self, value, count = 1):
        "Insert a value into the histogram"
        ffi.C.hist_insert(self.h, value, count)

    def insert_intscale(self, val, scale, count = 1):
        "Insert a value of val * 10^scale into this histogram. Use this if you can."
        ffi.C.hist_insert_intscale(self.h, val, scale, count)

    def clear(self):
        "Clear data. fast. Keep allocations."
        ffi.C.hist_clear(self.h)

    def __iter__(self):
        count = self.bin_count()
        for i in range(count):
            b = ffi.ffi.new("hist_bucket_t*")
            c = ffi.ffi.new("uint64_t*")
            ffi.C.hist_bucket_idx_bucket(self.h, i, b, c)
            yield (circllbin(b[0]), c[0])

    def mean(self):
        return ffi.C.hist_approx_mean(self.h)

    def sum(self):
        return ffi.C.hist_approx_sum(self.h)

    def quantile(self, q):
        q_in = ffi.ffi.new("double*", q)
        q_out = ffi.ffi.new("double*")
        ffi.C.hist_approx_quantile(self.h, q_in, 1, q_out)
        return q_out[0]

    def __str__(self):
        return ''.join("H[{}]->{}\n".format(b,c) for b,c in self)
