x <- as.raw(c(0, 1, 2, 0, 255, 0, 7))
expect_equal(count_nonzero(x), 4)
expect_equal(count_nonzero(raw()), 0)
expect_error(count_nonzero(1:3), "raw vector")

slow_convolve <- function(a, b) {
  ab <- double(length(a) + length(b) - 1L)
  for (i in seq_along(a)) {
    for (j in seq_along(b)) {
      ab[i + j - 1L] <- ab[i + j - 1L] + a[i] * b[j]
    }
  }
  ab
}
conv_a <- seq(-2, 3, length.out = 17)
conv_b <- c(0.25, -0.5, 0.75, 1.25, -0.125)
conv_ref <- slow_convolve(conv_a, conv_b)
expect_equal(convolve1d(conv_a, conv_b), conv_ref, tolerance = 1e-12)
expect_equal(convolve1d(numeric(), conv_b), numeric())
expect_equal(convolve1d(conv_a, numeric()), numeric())

info <- simd_info()
expect_true(is.list(info))
expect_true(all(c(
  "dispatch_mode", "requested_backend", "selected_backend",
  "compiled_backends", "cpu_supported_backends", "available_backends",
  "simde_native_backends", "operations", "operation_backends",
  "operation_selected_backends", "cpu_avx2", "cpu_wasm_simd128",
  "target_arch", "simde_version", "simde_commit"
) %in% names(info)))
expect_true(is.character(info$compiled_backends))
expect_true(is.character(info$cpu_supported_backends))
expect_true(is.character(info$available_backends))
expect_true(is.character(info$simde_native_backends))
expect_true(is.character(info$operations))
expect_true(is.list(info$operation_backends))
expect_true(is.character(info$operation_selected_backends))
expect_true(all(c("count_nonzero", "convolve1d") %in% info$operations))
expect_true(all(c("count_nonzero", "convolve1d") %in% names(info$operation_backends)))
expect_true(all(c("count_nonzero", "convolve1d") %in% names(info$operation_selected_backends)))
expect_true(all(info$simde_native_backends %in% info$compiled_backends))
expect_true("scalar" %in% info$compiled_backends)
expect_true("scalar" %in% info$available_backends)
expect_true("scalar" %in% info$operation_backends$count_nonzero)
expect_true("scalar" %in% info$operation_backends$convolve1d)
expect_true(info$operation_selected_backends[["count_nonzero"]] %in% info$operation_backends$count_nonzero)
expect_true(info$operation_selected_backends[["convolve1d"]] %in% info$operation_backends$convolve1d)

expect_silent(simd_set_backend("scalar"))
expect_equal(simd_backend(), "scalar")
info_scalar <- simd_info()
expect_equal(unname(info_scalar$operation_selected_backends), c("scalar", "scalar"))
expect_equal(count_nonzero(x), 4)
expect_equal(convolve1d(conv_a, conv_b), conv_ref, tolerance = 1e-12)

info <- simd_info()
available <- info$available_backends
for (backend in setdiff(available, "scalar")) {
  expect_silent(simd_set_backend(backend))
  expect_equal(simd_backend(), backend)
  info_backend <- simd_info()
  expect_equal(info_backend$operation_selected_backends[["count_nonzero"]], backend)
  expect_equal(count_nonzero(x), 4)
  if (backend %in% info$operation_backends$convolve1d) {
    expect_equal(info_backend$operation_selected_backends[["convolve1d"]], backend)
    expect_equal(convolve1d(conv_a, conv_b), conv_ref, tolerance = 1e-12)
  } else {
    expect_true(is.na(info_backend$operation_selected_backends[["convolve1d"]]))
    expect_error(convolve1d(conv_a, conv_b), "operation 'convolve1d' is not available")
  }
}

expect_silent(simd_set_backend("auto"))
info_auto <- simd_info()
expect_true(simd_backend() %in% c(available, "mixed", "unavailable"))
expect_true(info_auto$operation_selected_backends[["count_nonzero"]] %in% info_auto$operation_backends$count_nonzero)
expect_true(info_auto$operation_selected_backends[["convolve1d"]] %in% info_auto$operation_backends$convolve1d)

if (!("avx512" %in% available)) {
  expect_error(simd_set_backend("avx512"))
}
expect_error(simd_set_backend("nonsense"))

expect_true(dir.exists(simd_dispatch_template_path()))

vendor <- simde_info()
expect_true(is.list(vendor))
expect_true(all(c("component", "version", "repository", "commit", "date") %in% names(vendor)))
expect_equal(vendor$component, "SIMDe")
expect_match(vendor$version, "^[0-9]+\\.[0-9]+\\.[0-9]+$")
expect_match(vendor$commit, "^[0-9a-f]{40}$")
expect_equal(vendor$version, simd_info()$simde_version)
expect_equal(vendor$commit, simd_info()$simde_commit)
