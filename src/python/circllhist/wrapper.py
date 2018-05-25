"""
The circllhist python module provides a wrapper around the libcircllhist data structures
"""

import sys
import json
import circllhist.ffi as ffi

if sys.version_info[0] == 3:
    str_ascii = lambda b: str(b, "ASCII")
else:
    str_ascii = str

class Circllbin(object):
    "Wraps a histogram bin"

    __slots__ = ("_b",)

    def __init__(self, b):
        self._b = b

    @classmethod
    def from_number(cls, n):
        return cls(ffi.C.double_to_hist_bucket(n))

    def __str__(self):
        buf = ffi.ffi.new("char[]", 9)
        ffi.C.hist_bucket_to_string(self._b, buf)
        return str_ascii(ffi.ffi.string(buf))

    def width(self):
        return ffi.C.hist_bucket_to_double_bin_width(self._b)

    def midpoint(self):
        return ffi.C.hist_bucket_midpoint(self._b)

    def edge(self):
        "Returns the edge of the histogram bucket that is closer to zero"
        return ffi.C.hist_bucket_to_double(self._b)


class Circllhist(object):
    "Wraps a log-linear histogram"

    __slots__ = ("_h",)

    def __init__(self):
        self._h = ffi.ffi.gc(ffi.C.hist_alloc(), ffi.C.hist_free)

    def count(self):
        "Returns the number of samples stored in the histogram"
        return ffi.C.hist_sample_count(self._h)

    def bin_count(self):
        "Returns the number of bins used by the histogram"
        return ffi.C.hist_bucket_count(self._h)

    def insert(self, value, count=1):
        "Insert a value into the histogram"
        ffi.C.hist_insert(self._h, value, count)

    def insert_intscale(self, val, scale, count=1):
        "Insert a value of val * 10^scale into this histogram. Use this if you can."
        ffi.C.hist_insert_intscale(self._h, val, scale, count)

    def merge(self, hist):
        p = ffi.ffi.new("histogram_t **")
        p[0] = hist._h
        ffi.C.hist_accumulate(self._h, p, 1)

    def clear(self):
        "Clear data. fast. Keep allocations."
        ffi.C.hist_clear(self._h)

    def __iter__(self):
        count = self.bin_count()
        for i in range(count):
            b = ffi.ffi.new("hist_bucket_t*")
            c = ffi.ffi.new("uint64_t*")
            ffi.C.hist_bucket_idx_bucket(self._h, i, b, c)
            yield (Circllbin(b[0]), c[0])

    def mean(self):
        "Retuns an approximation of the mean value"
        return ffi.C.hist_approx_mean(self._h)

    def sum(self):
        "Returns an approximation of the sum"
        return ffi.C.hist_approx_sum(self._h)

    def stddev(self):
        "Returns an approximation of the standard deviation"
        return ffi.C.hist_approx_stddev(self._h)

    def moment(self, k):
        "Returns an approximation of the k-th moment"
        return ffi.C.hist_approx_moment(self._h, k)

    def count_below(self, threshold):
        "Returns the number of values in buckets that are entirely lower than or equal to threshold"
        return ffi.C.hist_approx_count_below(self._h, threshold)

    def count_above(self, threshold):
        "Returns the number of values in buckets that are entirely larger than or equal to threshold"
        return ffi.C.hist_approx_count_above(self._h, threshold)

    def count_nearby(self, value):
        "Returns the number of samples in the histogram that are in the same bucket as the provided value"
        return ffi.C.hist_approx_count_nearby(self._h, value)

    def quantile(self, q):
        "Returns an approximation of the q-quantile"
        q_in = ffi.ffi.new("double*", q)
        q_out = ffi.ffi.new("double*")
        ffi.C.hist_approx_quantile(self._h, q_in, 1, q_out)
        return q_out[0]

    def to_dict(self):
        "Use this to generate JSON output"
        d = {}
        for b, c in self:
            d[str(b)] = c
        return d

    @classmethod
    def from_dict(cls, d):
        h = cls()
        for k, v in d.items():
            h.insert(float(k), v)
        return h

    def to_b64(self):
        sz = ffi.C.hist_serialize_b64_estimate(self._h)
        buf = ffi.ffi.new("char[]", sz)
        ffi.C.hist_serialize_b64(self._h, buf, sz)
        return str_ascii(ffi.ffi.string(buf))

    @classmethod
    def from_b64(cls, b64):
        h = cls()
        buf = b64.encode("ASCII")
        sz = len(buf)
        ffi.C.hist_deserialize_b64(h._h, buf, sz)
        return h

    def __str__(self):
        return json.dumps(self.to_dict())
