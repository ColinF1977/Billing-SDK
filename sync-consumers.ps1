param([string]$DevRoot = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
foreach ($name in @('TSG','Player','Farcaster','ARC','Grey')) {
    $destination = Join-Path $DevRoot "$name/vendor/billing-sdk"
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    foreach ($part in @('native','python','tests')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $part) -Destination $destination -Recurse -Force }
    foreach ($file in @('build.sh','VERSION')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $destination -Force }
    $manifest = Get-ChildItem -LiteralPath $destination -File -Recurse | Where-Object { $_.Extension -ne '.pyc' -and $_.Name -ne 'SHA256SUMS' } | Sort-Object FullName | ForEach-Object {
        '{0}  {1}' -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLower(), $_.FullName.Substring($destination.Length + 1).Replace('\','/')
    }
    [IO.File]::WriteAllText((Join-Path $destination 'SHA256SUMS'), ($manifest -join "`n") + "`n")
}
foreach ($project in @('Hyperion.Control','Shrike.Api')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'dotnet/BillingClient.cs') -Destination (Join-Path $DevRoot "Hyperion/src/$project/BillingClient.cs") -Force
}
