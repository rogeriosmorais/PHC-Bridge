param(
  [string]$ProjectDir = "F:\NewEngine-AgentB",
  [int]$ProxyPort = 3001,
  [int]$McpPort = 3000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "Starting MCP auth proxy setup..."
Write-Host "Project directory: $ProjectDir"
Write-Host ""

if (!(Test-Path $ProjectDir)) {
  throw "Project directory does not exist: $ProjectDir"
}

if (!(Get-Command node -ErrorAction SilentlyContinue)) {
  throw "Node.js is not installed or not available in PATH."
}

if (!(Get-Command npm -ErrorAction SilentlyContinue)) {
  throw "npm is not installed or not available in PATH."
}

Set-Location $ProjectDir

if (!(Test-Path ".\node_modules\http-proxy")) {
  Write-Host "Installing http-proxy..."
  npm install http-proxy
  Write-Host ""
}

# Generate URL-safe secret
$bytes = New-Object byte[] 32
[Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
$secret = [Convert]::ToBase64String($bytes).TrimEnd("=").Replace("+", "-").Replace("/", "_")

$env:MCP_SECRET = $secret
$env:PROXY_PORT = "$ProxyPort"
$env:MCP_TARGET = "http://127.0.0.1:$McpPort"

$mjsPath = Join-Path $ProjectDir "mcp-auth-proxy.mjs"

$mjsContent = @'
import http from "node:http";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const httpProxy = require("http-proxy");

const PROXY_HOST = "127.0.0.1";
const PROXY_PORT = Number(process.env.PROXY_PORT || 3001);
const TARGET = process.env.MCP_TARGET || "http://127.0.0.1:3000";
const SECRET = process.env.MCP_SECRET;

if (!SECRET) {
  console.error("Missing MCP_SECRET environment variable.");
  process.exit(1);
}

const proxy = httpProxy.createProxyServer({
  target: TARGET,
  changeOrigin: true,
  xfwd: true,
  ws: true,
  timeout: 0,
  proxyTimeout: 0,
});

proxy.on("error", (err, req, res) => {
  // Gracefully handle common client disconnects/cancellations
  const isExpectedError =
    err.code === "ECONNRESET" ||
    err.code === "EPIPE" ||
    err.message.includes("socket hang up") ||
    err.message.includes("context canceled");

  if (isExpectedError) {
    console.log(`[INFO] Client disconnected: ${err.message}`);
  } else {
    console.error(`[ERROR] Proxy error: ${err.message}`, err);
  }

  if (!res.headersSent && res.writeHead) {
    res.writeHead(502, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "Bad gateway", code: err.code }));
  } else {
    res.end();
  }
});

// Harden SSE responses: disable buffering and set keep-alive
proxy.on("proxyRes", (proxyRes, req, res) => {
  const contentType = proxyRes.headers["content-type"] || "";
  if (contentType.includes("text/event-stream")) {
    // Disable buffering for SSE across all hops
    res.setHeader("X-Accel-Buffering", "no");
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("Connection", "keep-alive");
  }
});

function isAuthorizedPath(url) {
  const pathOnly = url.split("?")[0];

  return (
    pathOnly === `/${SECRET}/mcp` ||
    pathOnly.startsWith(`/${SECRET}/mcp/`)
  );
}

const server = http.createServer((req, res) => {
  const originalUrl = req.url || "";

  if (originalUrl === "/health") {
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify({ ok: true }));
    return;
  }

  if (!isAuthorizedPath(originalUrl)) {
    res.writeHead(401, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "Unauthorized" }));
    return;
  }

  // Rewrite:
  // /SECRET/mcp         -> /mcp
  // /SECRET/mcp/abc     -> /mcp/abc
  // /SECRET/mcp?x=y     -> /mcp?x=y
  req.url = originalUrl.replace(`/${SECRET}`, "");

  proxy.web(req, res);
});

server.listen(PROXY_PORT, PROXY_HOST, () => {
  console.log("");
  console.log(`Auth proxy listening on http://${PROXY_HOST}:${PROXY_PORT}`);
  console.log(`Forwarding to ${TARGET}`);
  console.log("");
  console.log("Protected local MCP URL:");
  console.log(`http://${PROXY_HOST}:${PROXY_PORT}/${SECRET}/mcp`);
  console.log("");
  console.log("Expose this proxy with:");
  console.log(`cloudflared tunnel --url http://${PROXY_HOST}:${PROXY_PORT}`);
  console.log("");
  console.log("Then your public MCP URL will be:");
  console.log(`https://YOUR-CLOUDFLARE-URL.trycloudflare.com/${SECRET}/mcp`);
  console.log("");
});
'@

Set-Content -Path $mjsPath -Value $mjsContent -Encoding UTF8

Write-Host "Created:"
Write-Host $mjsPath
Write-Host ""

Write-Host "Generated MCP secret:"
Write-Host $secret
Write-Host ""

Write-Host "Protected local MCP URL:"
Write-Host "http://127.0.0.1:$ProxyPort/$secret/mcp"
Write-Host ""

Write-Host "Make sure your real MCP server is already running at:"
Write-Host "http://127.0.0.1:$McpPort/mcp"
Write-Host ""

Write-Host "After this proxy starts, open a second PowerShell window and run:"
Write-Host "cloudflared tunnel --url http://127.0.0.1:$ProxyPort"
Write-Host ""

node .\mcp-auth-proxy.mjs