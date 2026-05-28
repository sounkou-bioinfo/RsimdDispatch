#!/usr/bin/env node

const fs = require("fs");
const http = require("http");
const os = require("os");
const path = require("path");
const zlib = require("zlib");
const { execFileSync } = require("child_process");
const { WebR, ChannelType } = require("webr");

const tgzPath = path.resolve(process.argv[2] || process.env.RSIMDDISPATCH_WEBR_TGZ || "");
if (!tgzPath || !fs.existsSync(tgzPath)) {
  console.error("Usage: node tools/webr-check.cjs path/to/RsimdDispatch_<version>.tgz");
  process.exit(2);
}

function parseDcf(text) {
  const fields = {};
  let current = null;
  for (const line of text.split(/\r?\n/)) {
    if (!line) continue;
    if (/^\s/.test(line) && current) {
      fields[current] += `\n${line}`;
      continue;
    }
    const idx = line.indexOf(":");
    if (idx < 0) continue;
    current = line.slice(0, idx);
    fields[current] = line.slice(idx + 1).replace(/^\s*/, "");
  }
  return fields;
}

function writePackagesIndex(repoDir, fields, fileName) {
  fs.mkdirSync(repoDir, { recursive: true });
  fs.copyFileSync(tgzPath, path.join(repoDir, fileName));
  const keys = Object.keys(fields).filter((key) => fields[key] !== undefined && fields[key] !== "");
  if (!keys.includes("File")) keys.push("File");
  fields.File = fileName;
  const packages = keys.map((key) => `${key}: ${fields[key]}`).join("\n") + "\n";
  fs.writeFileSync(path.join(repoDir, "PACKAGES"), packages);
  fs.writeFileSync(path.join(repoDir, "PACKAGES.gz"), zlib.gzipSync(packages));
}

function createLocalRepo(tmpRoot, rSeries) {
  const extractDir = path.join(tmpRoot, "extract");
  fs.mkdirSync(extractDir, { recursive: true });
  execFileSync("tar", ["-xzf", tgzPath, "-C", extractDir], { stdio: "inherit" });
  const descPath = path.join(extractDir, "RsimdDispatch", "DESCRIPTION");
  const fields = parseDcf(fs.readFileSync(descPath, "utf8"));
  if (fields.Package !== "RsimdDispatch" || !fields.Version) {
    throw new Error(`Unexpected DESCRIPTION in ${tgzPath}`);
  }
  const fileName = path.basename(tgzPath);
  writePackagesIndex(path.join(tmpRoot, "repo", "src", "contrib"), { ...fields }, fileName);
  writePackagesIndex(path.join(tmpRoot, "repo", "bin", "emscripten", "contrib", rSeries), { ...fields }, fileName);
  return path.join(tmpRoot, "repo");
}

function contentType(filePath) {
  if (filePath.endsWith(".gz") || filePath.endsWith(".tgz")) return "application/gzip";
  if (filePath.endsWith(".rds")) return "application/octet-stream";
  return "text/plain; charset=utf-8";
}

function serveDirectory(root) {
  const server = http.createServer((req, res) => {
    const url = new URL(req.url, "http://127.0.0.1");
    const decoded = decodeURIComponent(url.pathname).replace(/^\/+/, "");
    const filePath = path.normalize(path.join(root, decoded));
    if (!filePath.startsWith(root)) {
      res.writeHead(403);
      res.end("forbidden");
      return;
    }
    fs.readFile(filePath, (err, data) => {
      if (err) {
        res.writeHead(404);
        res.end("not found");
        return;
      }
      res.writeHead(200, { "content-type": contentType(filePath) });
      res.end(data);
    });
  });
  return new Promise((resolve) => {
    server.listen(0, "127.0.0.1", () => resolve(server));
  });
}

function outputText(capture) {
  return (capture.output || [])
    .map((entry) => (typeof entry.data === "string" ? entry.data : ""))
    .filter(Boolean)
    .join("\n");
}

(async () => {
  const tmpRoot = fs.mkdtempSync(path.join(os.tmpdir(), "rsimddispatch-webr-"));
  let server;
  let webR;
  try {
    webR = new WebR({ channelType: ChannelType.PostMessage, interactive: false });
    await webR.init();
    const rVersion = webR.versionR || "4.5.0";
    const [major, minor] = rVersion.split(".");
    const rSeries = `${major}.${minor}`;
    const repoRoot = createLocalRepo(tmpRoot, rSeries);
    server = await serveDirectory(repoRoot);
    const port = server.address().port;
    const repoUrl = `http://127.0.0.1:${port}`;

    console.log(`webR ${webR.version}; R ${webR.versionR}`);
    console.log(`Installing ${path.basename(tgzPath)} from ${repoUrl}`);
    await webR.installPackages(["RsimdDispatch"], { repos: [repoUrl], mount: false });

    const shelter = await new webR.Shelter();
    const capture = await shelter.captureR(`
library(RsimdDispatch)
info <- simd_info()
print(info[c("compiled_backends", "available_backends", "simde_native_backends", "target_arch", "target_os")])
stopifnot(identical(info$target_arch, "wasm32"))
stopifnot(identical(info$target_os, "emscripten"))
stopifnot("wasm_simd128" %in% info$compiled_backends)
stopifnot("wasm_simd128" %in% info$available_backends)
stopifnot("wasm_simd128" %in% info$simde_native_backends)
stopifnot(isTRUE(info$cpu_wasm_simd128))
simd_set_backend("wasm_simd128")
stopifnot(identical(simd_backend(), "wasm_simd128"))
x <- as.raw(c(0, 1, 2, 0, 255, 0, 7))
stopifnot(identical(count_nonzero(x), 4))
simd_set_backend("auto")
stopifnot(identical(simd_backend(), "wasm_simd128"))
`, { withAutoprint: false });
    const text = outputText(capture);
    if (text) console.log(text);
    console.log("PASS RsimdDispatch webR check");
    await webR.close();
  } finally {
    if (server) server.close();
    if (webR) webR.close();
    fs.rmSync(tmpRoot, { recursive: true, force: true });
  }
})().catch((err) => {
  console.error(err);
  process.exit(1);
});
