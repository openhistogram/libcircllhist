module(..., package.seeall)

local ffi = require('ffi');
local histo_lib = require "ffi_libcircllhist"

local tester = {}
local histo
local writer = io.write
local success = function() writer("SUCCESS\n") end
local failed = false
local fail = function() writer("FAIL\n"); failed = true end

function tester.setup(scratch)
  histo = histo_lib.hist_alloc();
end

function tester.teardown(scratch)
  histo_lib.hist_free(histo);
end

function tester.test1_default_empty()
  writer("test1_default_empty()...")
  if histo_lib.hist_bucket_count(histo) == 0 then
    success()
  else
    fail()
  end
end

function tester.test2_single_bucket()
  writer("test2_single_bucket()...\n")
  writer("  single insert, single bucket...")
  histo_lib.hist_insert(histo, 3.1, 5)
  if histo_lib.hist_bucket_count(histo) == 1 then
    success()
  else
    fail()
  end

  writer("  single bucket, approx mean...")
  if histo_lib.hist_approx_mean(histo) == 3.15 then
    success()
  else
    fail()
  end

  local value =  ffi.new("double[1]")
  local count =  ffi.new("uint64_t[1]")
  histo_lib.hist_bucket_idx(histo, 0, value, count)

  writer("  single bucket, value read...")
  if value[0] == 3.1 then
    success()
  else
    fail()
  end

  writer("  single bucket, count read...")
  if count[0] == 5 then
    success()
  else
    fail()
  end
end

function runTests()
    tester.setup()
    tester.test1_default_empty()
    tester.test2_single_bucket()
    tester.teardown()
    if failed then os.exit(-1) end
end
