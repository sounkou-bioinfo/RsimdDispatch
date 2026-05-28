x <- as.raw(c(0, 1, 2, 0, 255, 0, 7))
expect_equal(count_nonzero(x), 4)
expect_equal(count_nonzero(raw()), 0)
expect_error(count_nonzero(1:3), "raw vector")

info <- simd_info()
expect_true(is.list(info))
expect_true(all(c(
  "dispatch_mode", "requested_backend", "selected_backend",
  "compiled_backends", "cpu_supported_backends", "available_backends",
  "cpu_avx2", "target_arch", "simde_version", "simde_commit"
) %in% names(info)))
expect_true(is.character(info$compiled_backends))
expect_true(is.character(info$cpu_supported_backends))
expect_true(is.character(info$available_backends))
expect_true("scalar" %in% info$compiled_backends)
expect_true("scalar" %in% info$available_backends)

expect_silent(simd_set_backend("scalar"))
expect_equal(simd_backend(), "scalar")
expect_equal(count_nonzero(x), 4)

available <- simd_info()$available_backends
for (backend in setdiff(available, "scalar")) {
  expect_silent(simd_set_backend(backend))
  expect_equal(simd_backend(), backend)
  expect_equal(count_nonzero(x), 4)
}

expect_silent(simd_set_backend("auto"))
expect_true(simd_backend() %in% available)

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
