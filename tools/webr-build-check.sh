#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WEBR_IMAGE=${WEBR_IMAGE:-ghcr.io/r-wasm/webr:main}
WEBR_NODE_DIR=${WEBR_NODE_DIR:-${RUNNER_TEMP:-/tmp}/rsimddispatch-webr-node}

cd "${ROOT_DIR}"

echo "Building RsimdDispatch webR artifact with ${WEBR_IMAGE}..."
docker run --rm \
  -v "${ROOT_DIR}:/work" \
  -w /work \
  "${WEBR_IMAGE}" \
  bash -lc '
    set -euxo pipefail
    rm -rf RsimdDispatch.Rcheck ..Rcheck README.html RsimdDispatch_*.tar.gz RsimdDispatch_*.tgz \
           src/Makevars src/Makevars.win src/config.h src/rsd-kernels \
           src/*.o src/*.so src/*.dll src/*.dylib
    R CMD build --no-build-vignettes .
    Rscript -e "if (!requireNamespace(\"pak\", quietly = TRUE)) install.packages(\"pak\", repos = \"https://repo.r-wasm.org/\"); pak::pak(\"r-wasm/rwasm\", ask = FALSE)"
    Rscript -e "version <- read.dcf(\"DESCRIPTION\")[1, \"Version\"]; tarball <- sprintf(\"./RsimdDispatch_%s.tar.gz\", version); stopifnot(file.exists(tarball)); rwasm::build(tarball); stopifnot(file.exists(sprintf(\"RsimdDispatch_%s.tgz\", version)))"
  '

TGZ=$(ls -t RsimdDispatch_*.tgz | head -n 1)
if [ -z "${TGZ}" ]; then
  echo "Expected webR artifact RsimdDispatch_*.tgz was not produced" >&2
  exit 1
fi

echo "Checking ${TGZ} in Node webR..."
npm install --prefix "${WEBR_NODE_DIR}" --no-save webr@0.5.9
NODE_PATH="${WEBR_NODE_DIR}/node_modules" node tools/webr-check.cjs "${TGZ}"
