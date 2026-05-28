# h/t to @jimhester and @yihui for this parse block:
# https://github.com/yihui/knitr/blob/dc5ead7bcfc0ebd2789fe99c527c7d91afb3de4a/Makefile#L1-L4
# Note the portability change as suggested in the manual:
# https://cran.r-project.org/doc/manuals/r-release/R-exts.html#Writing-portable-packages
PKGNAME := $(shell sed -n 's/Package: *\([^ ]*\)/\1/p' DESCRIPTION)
PKGVERS := $(shell sed -n 's/Version: *\([^ ]*\)/\1/p' DESCRIPTION)

all: check

rd:
	R -e 'roxygen2::roxygenize(load_code = "source")'

readme:
	R -e 'rmarkdown::render("README.Rmd", output_format = rmarkdown::github_document(), output_file = "README.md")'

vig:
	R -e "tools::buildVignettes(dir = '.')"

vig-md:
	R -e "for (f in Sys.glob('vignettes/*.Rmd')) { out <- sub('\\\\.Rmd$$', '.md', f); rmarkdown::render(f, output_format = rmarkdown::md_document(variant = 'gfm'), output_file = basename(out), output_dir = dirname(out), quiet = FALSE, envir = new.env(parent = globalenv())) }"

pkgdown:
	R -e 'pkgdown::build_site()'

vendor:
	Rscript tools/vendor-simde.R

update-authors:
	Rscript tools/update-authors.R

authors: update-authors

check-template-sync:
	@diff -rq src inst/templates/dispatch-c/src \
		--exclude='*.o' --exclude='*.so' --exclude='*.dll' --exclude='*.dylib' \
		--exclude='Makevars' --exclude='Makevars.win' --exclude='config.h' \
		|| (echo 'ERROR: template/src drift detected' && exit 1)
	@for f in cleanup configure configure.win src/Makevars.in src/Makevars.win.in tools/configure-simd-dispatch.sh; do \
		diff -q "$$f" "inst/templates/dispatch-c/$$f" >/dev/null || \
			(echo "ERROR: template file drift detected: $$f" && exit 1); \
	done

build:
	R CMD build .

check: build
	R CMD check --as-cran --no-manual $(PKGNAME)_$(PKGVERS).tar.gz

install_deps:
	R \
	-e 'if (!requireNamespace("remotes", quietly = TRUE)) install.packages("remotes")' \
	-e 'remotes::install_deps(dependencies = TRUE)'

install: build
	R CMD INSTALL $(PKGNAME)_$(PKGVERS).tar.gz

install2:
	R CMD INSTALL --no-configure .

install3:
	R CMD INSTALL .

clean:
	@rm -rf $(PKGNAME)_$(PKGVERS).tar.gz $(PKGNAME).Rcheck src/Makevars src/Makevars.win src/config.h config.log src/*.o src/*.so src/*.dll src/*.dylib

# Development targets
dev-install:
	R CMD INSTALL --preclean .

test1:
	R -e "tinytest::test_package('$(PKGNAME)', testdir = 'inst/tinytest', ncpu = 1L)"

test2:
	R -e "tinytest::test_package('$(PKGNAME)', testdir = 'inst/tinytest', ncpu = 2L)"

test0:
	R -e "tinytest::test_package('$(PKGNAME)', testdir = 'inst/tinytest')"

test: install
	R -e "tinytest::test_package('$(PKGNAME)', testdir = 'inst/tinytest')"

.PHONY: all rd readme vig vig-md pkgdown vendor update-authors authors check-template-sync build check install_deps install install2 install3 clean dev-install test1 test2 test0 test
