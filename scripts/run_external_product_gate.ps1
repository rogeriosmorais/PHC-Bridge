param(
    [Parameter(Mandatory = $true)]
    [string]$Evidence,

    [Parameter(Mandatory = $true)]
    [string]$Receipt,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedNonce,

    [Parameter(Mandatory = $true)]
    [string]$NotBefore
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Node = "C:\Users\roger\AppData\Local\Volta\tools\image\node\22.22.1\node.exe"
$Oracle = "F:\GlobalMCP2\physanim-product-oracle\src\cli.js"
$ProjectId = "proj_3526075d04ea"
$ProjectName = "NewEngine-AgentB"
$ObjectiveId = "node_dc8092eb7fce"

if (-not (Test-Path -LiteralPath $Oracle)) {
    throw "Protected product oracle is unavailable: $Oracle"
}
if (-not (Test-Path -LiteralPath $Evidence)) {
    throw "Runtime evidence does not exist: $Evidence"
}
if (-not $env:PHYSANIM_PRODUCT_ORACLE_PRIVATE_KEY_PEM) {
    throw "Protected CI signing key is not available in PHYSANIM_PRODUCT_ORACLE_PRIVATE_KEY_PEM"
}

$Status = & git -C $RepoRoot status --porcelain --untracked-files=normal
if ($LASTEXITCODE -ne 0) {
    throw "Unable to inspect the source worktree"
}
if ($Status) {
    throw "Product completion requires a clean source worktree"
}

$Commit = (& git -C $RepoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Unable to resolve the source commit"
}
$Tree = (& git -C $RepoRoot rev-parse "HEAD^{tree}").Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Unable to resolve the source tree"
}

& $Node $Oracle `
    --evidence ([IO.Path]::GetFullPath($Evidence)) `
    --receipt ([IO.Path]::GetFullPath($Receipt)) `
    --expected-nonce $ExpectedNonce `
    --expected-commit $Commit `
    --git-tree $Tree `
    --project-id $ProjectId `
    --project-name $ProjectName `
    --node-id $ObjectiveId `
    --objective-id $ObjectiveId `
    --not-before $NotBefore

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Output "External product receipt created: $Receipt"
