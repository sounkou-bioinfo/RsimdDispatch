#!/usr/bin/env Rscript

# Vendor the full SIMDe header tree from a pinned upstream commit.
# Run from the RsimdDispatch package root:
#   Rscript tools/vendor-simde.R

root <- normalizePath(".", mustWork = TRUE)
repo <- "https://github.com/simd-everywhere/simde.git"
commit <- "f3e8262173b7089db9a9d57a9ecef8dd07ad9c97"

run <- function(cmd, args, wd = root) {
  message("$ ", cmd, " ", paste(args, collapse = " "))
  old <- setwd(wd)
  on.exit(setwd(old), add = TRUE)
  status <- system2(cmd, args, stdout = "", stderr = "")
  if (!identical(status, 0L)) {
    stop("Command failed: ", cmd, call. = FALSE)
  }
}

have_git <- nzchar(Sys.which("git"))
if (!have_git) {
  stop("git is required to vendor SIMDe", call. = FALSE)
}

tmp <- tempfile("rsd-simde-")
dir.create(tmp)
on.exit(unlink(tmp, recursive = TRUE, force = TRUE), add = TRUE)

checkout <- file.path(tmp, "simde")
run("git", c("clone", "--no-checkout", repo, checkout), wd = tmp)
run("git", c("fetch", "--depth", "1", "origin", commit), wd = checkout)
run("git", c("checkout", "--detach", commit), wd = checkout)

old <- setwd(checkout)
on.exit(setwd(old), add = TRUE)
actual <- system2("git", c("rev-parse", "HEAD"), stdout = TRUE, stderr = TRUE)
commit_date <- system2("git", c("log", "-1", "--format=%cs"), stdout = TRUE)
setwd(old)
if (!identical(actual[[1]], commit)) {
  stop("unexpected SIMDe commit: ", actual[[1]], call. = FALSE)
}

include_dest <- file.path(root, "inst", "include", "simde")
vendor_dest <- file.path(root, "inst", "vendor", "simde")
unlink(include_dest, recursive = TRUE, force = TRUE)
unlink(vendor_dest, recursive = TRUE, force = TRUE)
dir.create(dirname(include_dest), recursive = TRUE, showWarnings = FALSE)
dir.create(vendor_dest, recursive = TRUE, showWarnings = FALSE)

ok <- file.copy(file.path(checkout, "simde"), dirname(include_dest), recursive = TRUE, copy.date = TRUE)
if (!ok) {
  stop("failed to copy SIMDe include tree", call. = FALSE)
}

for (file in c("COPYING", "README.md", ".all-contributorsrc")) {
  from <- file.path(checkout, file)
  if (file.exists(from)) {
    to <- file.path(vendor_dest, if (file == ".all-contributorsrc") "all-contributors.json" else file)
    file.copy(from, to, overwrite = TRUE, copy.date = TRUE)
  }
}

writeLines(
  c(
    "Component: SIMDe",
    paste0("Repository: ", sub("\\.git$", "", repo)),
    paste0("Commit: ", commit),
    paste0("Date: ", commit_date),
    "Vendored-include-tree: inst/include/simde",
    "License-file: inst/vendor/simde/COPYING"
  ),
  file.path(vendor_dest, "VERSION")
)

system2(file.path(R.home("bin"), "Rscript"), "tools/update-authors.R", stdout = "", stderr = "")

message("Vendored SIMDe commit ", commit)
message("Wrote ", include_dest)
