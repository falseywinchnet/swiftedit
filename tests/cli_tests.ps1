param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('swiftedit-cli-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixture) | Out-Null
$process = [Diagnostics.Process]::new()
try {
    $process.StartInfo.FileName = [IO.Path]::GetFullPath($Executable)
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardInput = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    if (-not $process.Start()) { throw 'CLI did not start' }
    function Read-Line {
        $pending = $process.StandardOutput.ReadLineAsync()
        if (-not $pending.Wait(5000)) { throw 'CLI response timeout' }
        if ($null -eq $pending.Result) { throw 'Unexpected CLI EOF' }
        return $pending.Result
    }
    function Send-Request([string[]]$Fields) {
        $process.StandardInput.WriteLine(($Fields -join "`t"))
        $process.StandardInput.Flush()
        $rows = [Collections.Generic.List[string]]::new()
        do { $row = Read-Line; $rows.Add($row) } while (-not ($row.StartsWith("ok`t") -or $row.StartsWith("error`t")))
        return ,$rows.ToArray()
    }
    function Assert([bool]$Value,[string]$Message) { if (-not $Value) { throw $Message } }
    Assert ((Read-Line) -eq "ready`tSwiftEdit`t1") 'Protocol greeting'
    $response = Send-Request @('preview','','','','hello\nworld')
    $preview = $response[0].Split("`t")
    Assert ($preview[0] -eq 'preview') 'Explicit preview returned'
    $response = Send-Request @('commit',$preview[1],$preview[2])
    Assert ($response[-1].StartsWith("ok`tcommit")) 'Preview commit accepted'
    $response = Send-Request @('commit',$preview[1],$preview[2])
    Assert ($response[-1].StartsWith("error`tStale")) 'Stale commit refused'
    $response = Send-Request @('page','0','4096')
    Assert ($response[0] -eq "page`t0`t11`t11`thello\x0aworld") 'Exact escaped page'
    $target = (Join-Path $fixture 'new.txt').Replace('\','\\')
    $response = Send-Request @('save-as',$target)
    Assert ($response[-1].StartsWith("ok`tsave-as")) 'Save creates new file'
    Assert ([IO.File]::ReadAllText((Join-Path $fixture 'new.txt')) -eq "hello`nworld") 'File bytes match explicit edit'
    $response = Send-Request @('undo')
    Assert ($response[-1].StartsWith("error`tAt last-save")) 'Save boundary through process'
    $response = Send-Request @('restore-opened')
    $response = Send-Request @('open',$target)
    Assert ($response[-1].StartsWith("error`tUnsaved")) 'Open cannot silently discard'
    $response = Send-Request @('discard')
    $csv = Join-Path $fixture 'table.csv'
    [IO.File]::WriteAllText($csv,"0.1,0.2`n4,text")
    $response = Send-Request @('open',$csv.Replace('\','\\'))
    $response = Send-Request @('csv-calculate','SUM(A1:B1)')
    Assert ($response[0] -eq "calculation`t0.3`tA1`tB1") 'Exact decimal result with references'
    $response = Send-Request @('csv-calculate','SUM(A1:B2)')
    Assert ($response[-1].StartsWith("error`tB2:")) 'Invalid referenced cell identified'
    $response = Send-Request @('csv-set','B2','7')
    $preview = $response[0].Split("`t")
    Assert ([IO.File]::ReadAllText($csv).EndsWith('text')) 'CSV preview does not write disk'
    $response = Send-Request @('commit',$preview[1],$preview[2])
    $response = Send-Request @('csv-calculate','SUM(A1:B2)')
    Assert ($response[0].StartsWith("calculation`t11.3`t")) 'CSV commit updates calculation source'
    $response = Send-Request @('quit')
    Assert ($process.WaitForExit(5000) -and $process.ExitCode -eq 0) 'Clean process exit'
    Assert ([IO.File]::ReadAllText($csv).EndsWith('text')) 'Quit does not autosave'
    Write-Output 'CLI integration passed: transport, preview/commit, stale guard, file publication, undo boundary, dirty-open refusal and exact CSV calculation.'
} finally {
    if ($process.Id -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
    $resolved = [IO.Path]::GetFullPath($fixture)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if (-not $resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or -not ([IO.Path]::GetFileName($resolved)).StartsWith('swiftedit-cli-')) { throw 'Fixture cleanup path validation failed' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
