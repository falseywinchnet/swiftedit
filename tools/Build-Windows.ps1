param(
    [string]$Toolchain = 'C:/Users/Shadow/plan-paint/build-deps/msys64/mingw64/bin',
    [string]$GuiSdk = 'C:/Users/Shadow/file_manager/.build/sdk-checkpoints/dbe3766/windows-x64/gui-forms-sdk',
    [string]$PickerSdk = 'C:/Users/Shadow/file_manager/.build/sdk-checkpoints/dbe3766/windows-x64/picker-sdk',
    [string]$BuildDirectory = '',
    [string]$StageDirectory = '',
    [switch]$NativeTests
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$build = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else { Join-Path $repo '.build/swiftedit' }
$stage = if ($StageDirectory) { [IO.Path]::GetFullPath($StageDirectory) } else { Join-Path $repo 'dist/SwiftEdit' }
$env:PATH = "$Toolchain;$GuiSdk/bin;$PickerSdk/bin;$env:PATH"
function Assert-NotRunning([string]$directory) {
    $prefix = [IO.Path]::GetFullPath($directory).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    foreach ($process in (Get-CimInstance Win32_Process -Filter "Name = 'notepad.exe' OR Name = 'SwiftEdit.exe' OR Name = 'swiftedit-cli.exe' OR Name = 'swiftedit-session-tests.exe' OR Name = 'notepad-native-tests.exe' OR Name = 'notepad-editor-tests.exe'")) {
        if ($process.ExecutablePath -and $process.ExecutablePath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Process $($process.ProcessId) is using $directory. Choose another build/staging directory; the running copy was preserved."
        }
    }
}
function Get-SdkFingerprint {
    $roots = @("$GuiSdk/include", "$GuiSdk/lib", "$GuiSdk/bin", "$PickerSdk/include", "$PickerSdk/lib")
    $lines = foreach ($root in $roots) {
        Get-ChildItem -LiteralPath $root -Recurse -File | Sort-Object FullName | ForEach-Object {
            $_.FullName + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    }
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($lines -join "`n")))) }
    finally { $sha.Dispose() }
}
Push-Location -LiteralPath $repo
try {
    Assert-NotRunning $build
    Assert-NotRunning $stage
    $sdkFingerprint = Get-SdkFingerprint
    $stamp = Join-Path $build 'sdk-fingerprint.txt'
    $previous = if (Test-Path -LiteralPath $stamp) { (Get-Content -LiteralPath $stamp -Raw).Trim() } else { '' }
    $nativeFlag = if ($NativeTests) { 'ON' } else { 'OFF' }
    & "$Toolchain/cmake.exe" -S $repo -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release '-DNOTEPAD_BUILD_UI=ON' "-DNOTEPAD_NATIVE_TESTS=$nativeFlag" "-DCMAKE_PREFIX_PATH=$GuiSdk;$PickerSdk"
    if ($LASTEXITCODE) { throw 'Configure failed' }
    if ($previous -ne $sdkFingerprint) {
        Write-Output 'SDK contents changed or have no verified receipt: rebuilding all consumer objects.'
        & "$Toolchain/cmake.exe" --build $build --target clean
        if ($LASTEXITCODE) { throw 'Clean failed' }
    }
    & "$Toolchain/cmake.exe" --build $build --parallel 2
    if ($LASTEXITCODE) { throw 'Build failed' }
    & "$Toolchain/ctest.exe" --test-dir $build --output-on-failure
    if ($LASTEXITCODE) { throw 'Tests failed' }
    if ((Get-SdkFingerprint) -ne $sdkFingerprint) { throw 'SDK changed during build/tests; rerun against a coherent checkpoint.' }
    Assert-NotRunning $stage
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    Copy-Item -LiteralPath "$build/SwiftEdit.exe" -Destination $stage -Force
    Copy-Item -LiteralPath "$build/swiftedit-cli.exe" -Destination $stage -Force
    Copy-Item -LiteralPath "$repo/README.md" -Destination $stage -Force
    Copy-Item -LiteralPath "$repo/docs" -Destination $stage -Recurse -Force
    Copy-Item -LiteralPath "$GuiSdk/share/GUIForms/fonts" -Destination $stage -Recurse -Force
    $queue = [System.Collections.Generic.Queue[string]]::new()
    $queue.Enqueue("$stage/SwiftEdit.exe")
    $queue.Enqueue("$stage/swiftedit-cli.exe")
    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    while ($queue.Count) {
        $binary = $queue.Dequeue()
        $imports = & "$Toolchain/objdump.exe" -p $binary
        if ($LASTEXITCODE) { throw "Cannot inspect imports: $binary" }
        foreach ($line in $imports) {
            if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
            $name = $Matches[1]
            if (-not $seen.Add($name)) { continue }
            $dependency = $null
            foreach ($dir in @("$GuiSdk/bin", "$PickerSdk/bin", $Toolchain)) {
                $candidate = Join-Path $dir $name
                if (Test-Path -LiteralPath $candidate) { $dependency = $candidate; break }
            }
            if ($dependency) {
                Copy-Item -LiteralPath $dependency -Destination $stage -Force
                $queue.Enqueue((Join-Path $stage $name))
            } elseif ($name -notmatch '^(api-ms-|ext-ms-)' -and -not (Test-Path -LiteralPath (Join-Path "$env:SystemRoot/System32" $name))) {
                throw "Missing runtime dependency: $name"
            }
        }
    }
    Get-ChildItem -LiteralPath $stage -File | Get-FileHash -Algorithm SHA256 | Format-Table -AutoSize
    if ((Get-SdkFingerprint) -ne $sdkFingerprint) { throw 'SDK changed during packaging; this stage is not verified.' }
    Set-Content -LiteralPath $stamp -Value $sdkFingerprint
    Set-Content -LiteralPath (Join-Path $stage 'sdk-fingerprint.txt') -Value $sdkFingerprint
    if ($NativeTests) { Write-Output "Staged $stage/SwiftEdit.exe. Coordinated self-closing native smoke passed." }
    else { Write-Output "Staged $stage/SwiftEdit.exe. No desktop window was launched." }
} finally { Pop-Location }
