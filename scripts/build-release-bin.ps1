param (
    [Parameter(Position=0,mandatory=$true)]
    [string]$VERSION
)
Write-Host "Building $VERSION..."

$dir = Get-Location
$builddir = "$dir/build_release"
$bindir = "$dir/release-bin/nrf_receiver_$VERSION"

if (Test-Path("${bindir}.zip")) {
    throw "Release already exists!"
}

# Build default config
west build -b xiao_ble --build-dir "$builddir/c2" $dir
if ($LastExitCode) {
    exit $LastExitCode
}
west build -b xiao_ble --build-dir "$builddir/c2core" $dir -- -DOVERLAY_CONFIG="prj_c2core.conf"
if ($LastExitCode) {
    exit $LastExitCode
}

if (Test-Path($bindir)) {
    Remove-Item -Recurse "$bindir"
}
mkdir -Path $bindir
Copy-Item "$builddir/c2/zephyr/zephyr.uf2" "$bindir/nrf_receiver_walk_c2.uf2"
Copy-Item "$builddir/c2core/zephyr/zephyr.uf2" "$bindir/nrf_receiver_walk_c2_core.uf2"
Copy-Item "$dir/scripts/install/*" "$bindir/"
Copy-Item "$dir/README.md" "$bindir/README.txt"
Copy-Item -Recurse "$dir/scripts/sensors" "$bindir/"
Compress-Archive -Path $bindir -DestinationPath "${bindir}.zip"

Remove-Item -Recurse "$builddir"
Remove-Item -Recurse "$bindir"
