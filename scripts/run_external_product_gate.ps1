[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

throw @"
This repository cannot invoke the protected product signer.

Obtain a product_gate_challenge from the guarded MCP, then deliver that exact
challenge to the separately owned service at F:\GlobalMCP2\physanim-product-oracle.
Only that protected service may run run-standing-gate.ps1 and access its signing
key or output root. Submit its exact envelope bytes and receipt back through the
MCP product_gate_receipt tool.
"@
