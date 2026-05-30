## Tests for R/template.R helper functions
## These cover the template scaffolding logic that is not exercised by the
## C-level dispatch tests.

# ---------------------------------------------------------------------------
# rsd_sanitize_c_prefix
# ---------------------------------------------------------------------------

expect_equal(RsimdDispatch:::rsd_sanitize_c_prefix("mypkg"), "mypkg")
expect_equal(RsimdDispatch:::rsd_sanitize_c_prefix("MyPkg"), "my_pkg")
expect_equal(RsimdDispatch:::rsd_sanitize_c_prefix("my.pkg"), "my_pkg")
expect_equal(RsimdDispatch:::rsd_sanitize_c_prefix("my-pkg"), "my_pkg")
expect_equal(RsimdDispatch:::rsd_sanitize_c_prefix("123abc"), "_123abc")
expect_equal(RsimdDispatch:::rsd_sanitize_c_prefix("ABCDef"), "abcdef")

# ---------------------------------------------------------------------------
# rsd_camel_prefix
# ---------------------------------------------------------------------------

expect_equal(RsimdDispatch:::rsd_camel_prefix("mypkg"), "Mypkg")
expect_equal(RsimdDispatch:::rsd_camel_prefix("my_pkg"), "MyPkg")
expect_equal(RsimdDispatch:::rsd_camel_prefix("my_simd_pkg"), "MySimdPkg")
expect_equal(RsimdDispatch:::rsd_camel_prefix("_foo"), "Foo")

# ---------------------------------------------------------------------------
# rsd_template_substitute
# ---------------------------------------------------------------------------

lines <- c("sd_count", "SD_HAVE_SSE2", "Sd something", "RsimdDispatch", "RC_count")
out <- RsimdDispatch:::rsd_template_substitute(lines, pkg = "MyPkg", prefix = "mp")
expect_equal(out[1], "mp_count")
expect_equal(out[2], "MP_HAVE_SSE2")
expect_equal(out[3], "Mp something")
expect_equal(out[4], "MyPkg")
expect_equal(out[5], "MP_C_count")

# configure script: 'package = "MyPkg"' should be reverted to 'package = "RsimdDispatch"'
cfg_lines <- c('x <- system.file(package = "MyPkg")', 'y <- system.file(package="MyPkg")')
cfg_out <- RsimdDispatch:::rsd_template_substitute(
  cfg_lines, pkg = "MyPkg", prefix = "mp",
  file = file.path("tools", "configure-simd-dispatch.sh")
)
expect_true(grepl('package = "RsimdDispatch"', cfg_out[1]))
expect_true(grepl('package="RsimdDispatch"', cfg_out[2]))

# ---------------------------------------------------------------------------
# rsd_is_text_file
# ---------------------------------------------------------------------------

expect_true(RsimdDispatch:::rsd_is_text_file("foo.c"))
expect_true(RsimdDispatch:::rsd_is_text_file("foo.h"))
expect_true(RsimdDispatch:::rsd_is_text_file("configure"))
expect_true(RsimdDispatch:::rsd_is_text_file("configure.win"))
expect_true(RsimdDispatch:::rsd_is_text_file("foo.R"))
expect_true(RsimdDispatch:::rsd_is_text_file("foo.sh"))
expect_false(RsimdDispatch:::rsd_is_text_file("foo.so"))
expect_false(RsimdDispatch:::rsd_is_text_file("foo.o"))
expect_false(RsimdDispatch:::rsd_is_text_file("foo.rdb"))

# ---------------------------------------------------------------------------
# rsd_append_dep
# ---------------------------------------------------------------------------

expect_equal(RsimdDispatch:::rsd_append_dep(NA_character_, "RsimdDispatch"), "RsimdDispatch")
expect_equal(RsimdDispatch:::rsd_append_dep("", "RsimdDispatch"), "RsimdDispatch")
# Already present: no duplicate
dep1 <- RsimdDispatch:::rsd_append_dep("RsimdDispatch", "RsimdDispatch")
expect_true(sum(grepl("RsimdDispatch", strsplit(dep1, ",")[[1]])) == 1L)
# Not present: appended
dep2 <- RsimdDispatch:::rsd_append_dep("Rcpp (>= 1.0)", "RsimdDispatch")
expect_true(grepl("RsimdDispatch", dep2))
expect_true(grepl("Rcpp", dep2))

# ---------------------------------------------------------------------------
# rsd_append_csv
# ---------------------------------------------------------------------------

expect_equal(RsimdDispatch:::rsd_append_csv(NA_character_, "GNU make"), "GNU make")
expect_equal(RsimdDispatch:::rsd_append_csv("", "GNU make"), "GNU make")
# Already present: no duplicate
csv1 <- RsimdDispatch:::rsd_append_csv("GNU make", "GNU make")
expect_equal(sum(csv1 == "GNU make"), 1L)
# Not present: appended
csv2 <- RsimdDispatch:::rsd_append_csv("cmake", "GNU make")
expect_true(grepl("GNU make", csv2))
expect_true(grepl("cmake", csv2))

# ---------------------------------------------------------------------------
# rsd_append_unique_lines
# ---------------------------------------------------------------------------

tmp <- tempfile()
on.exit(unlink(tmp), add = TRUE)
# Creates file when absent
RsimdDispatch:::rsd_append_unique_lines(tmp, c("^src/Makevars$", "^src/config\\.h$"))
lines <- readLines(tmp, warn = FALSE)
expect_true("^src/Makevars$" %in% lines)
expect_true("^src/config\\.h$" %in% lines)
# No duplicate on second call
RsimdDispatch:::rsd_append_unique_lines(tmp, c("^src/Makevars$", "newline"))
lines2 <- readLines(tmp, warn = FALSE)
expect_equal(sum(lines2 == "^src/Makevars$"), 1L)
expect_true("newline" %in% lines2)

# ---------------------------------------------------------------------------
# use_simd_dispatch: end-to-end scaffold into a temp package directory
# ---------------------------------------------------------------------------

local({
  tmpdir <- file.path(tempdir(), "rsd_test_pkg")
  unlink(tmpdir, recursive = TRUE)
  dir.create(tmpdir, recursive = TRUE)
  on.exit(unlink(tmpdir, recursive = TRUE), add = TRUE)

  # Write a minimal DESCRIPTION
  writeLines(c(
    "Package: testsimd",
    "Version: 0.1.0",
    "Title: Test",
    "Description: Test.",
    "Authors@R: person('A', 'B', role = c('aut', 'cre'), email = 'a@b.com')",
    "License: MIT + file LICENSE"
  ), file.path(tmpdir, "DESCRIPTION"))

  copied <- use_simd_dispatch(path = tmpdir, quiet = TRUE)

  # Files were written
  expect_true(length(copied) > 0L)
  expect_true(all(file.exists(copied)))

  # configure and cleanup scripts were written
  expect_true(file.exists(file.path(tmpdir, "configure")))
  expect_true(file.exists(file.path(tmpdir, "cleanup")))

  # Key source files: core lib lives in tools/simdDispatch/ (not src/, not tools/lib/)
  expect_true(file.exists(file.path(tmpdir, "tools", "simdDispatch", "simd_dispatch.c")))
  expect_true(file.exists(file.path(tmpdir, "tools", "simdDispatch", "simd_dispatch.h")))
  expect_true(file.exists(file.path(tmpdir, "tools", "simdDispatch", "cpu_features.c")))
  expect_true(file.exists(file.path(tmpdir, "tools", "simdDispatch", "cpu_features.h")))
  expect_false(file.exists(file.path(tmpdir, "src", "simd_dispatch.c")))
  expect_false(file.exists(file.path(tmpdir, "src", "cpu_features.c")))
  expect_true(file.exists(file.path(tmpdir, "src", "r_api.c")))

  # Standalone-only files must NOT be copied into the R package scaffold
  expect_false(file.exists(file.path(tmpdir, "tools", "simdDispatch", "Makefile")))
  expect_false(file.exists(file.path(tmpdir, "tools", "simdDispatch", "examples", "example_count.c")))
  expect_false(file.exists(file.path(tmpdir, "tools", "simdDispatch", "test", "test_dispatch.c")))

  # Prefix substitution was applied (sd_ → testsimd_ prefix)
  dispatch_c <- readLines(file.path(tmpdir, "tools", "simdDispatch", "simd_dispatch.c"), warn = FALSE)
  expect_false(any(grepl("\\bsd_", dispatch_c)))

  r_api_c <- readLines(file.path(tmpdir, "src", "r_api.c"), warn = FALSE)
  expect_false(any(grepl("\\bRsimdDispatch\\b", r_api_c)))

  # DESCRIPTION was patched with LinkingTo and SystemRequirements
  desc <- read.dcf(file.path(tmpdir, "DESCRIPTION"), all = TRUE)
  expect_true(grepl("RsimdDispatch", desc[1, "LinkingTo"]))
  expect_true(grepl("GNU make", desc[1, "SystemRequirements"]))

  # .Rbuildignore and .gitignore were updated
  rbuildignore <- readLines(file.path(tmpdir, ".Rbuildignore"), warn = FALSE)
  expect_true(any(grepl("Makevars", rbuildignore)))
  gitignore <- readLines(file.path(tmpdir, ".gitignore"), warn = FALSE)
  expect_true(any(grepl("Makevars", gitignore)))

  # overwrite = FALSE raises an error the second time
  expect_error(use_simd_dispatch(path = tmpdir, quiet = TRUE), "overwrite")
})

# ---------------------------------------------------------------------------
# simde_info() fallback: rsd_simde_header_version()
# ---------------------------------------------------------------------------

# Smoke-test: the header is installed, so the fallback should return a valid
# version string even when called directly.
ver <- RsimdDispatch:::rsd_simde_header_version()
expect_true(is.character(ver) && length(ver) == 1L)
expect_true(is.na(ver) || grepl("^[0-9]+\\.[0-9]+\\.[0-9]+$", ver))
