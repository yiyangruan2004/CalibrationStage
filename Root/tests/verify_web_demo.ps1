param(
    [string]$RepositoryRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
)

$indexPath = Join-Path $RepositoryRoot 'index.html'
$workerPath = Join-Path $RepositoryRoot 'coi-serviceworker.js'
$index = Get-Content -Raw -LiteralPath $indexPath

if ($index -notmatch '<script src="coi-serviceworker\.js"></script>') {
    throw 'index.html must load the cross-origin-isolation service worker.'
}

if (-not (Test-Path -LiteralPath $workerPath -PathType Leaf)) {
    throw 'The cross-origin-isolation service worker is missing.'
}

$worker = Get-Content -Raw -LiteralPath $workerPath
foreach ($header in @('Cross-Origin-Embedder-Policy', 'Cross-Origin-Opener-Policy')) {
    if ($worker -notmatch [regex]::Escape($header)) {
        throw "The service worker must set the $header response header."
    }
}

Write-Host 'Web demo isolation configuration is present.'
