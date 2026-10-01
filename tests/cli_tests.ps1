param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('swiftedit-cli-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixture) | Out-Null
$process = [Diagnostics.Process]::new()
$started = $false
try {
    $process.StartInfo.FileName = [IO.Path]::GetFullPath($Executable)
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardInput = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $started = $process.Start()
    if (-not $started) { throw 'CLI did not start' }
    function Read-Line([Diagnostics.Process]$Process) {
        $pending = $process.StandardOutput.ReadLineAsync()
        [bool]$completed = $pending.Wait(5000)
        if (-not $completed) { throw 'CLI response timeout' }
        if ($null -eq $pending.Result) { throw 'Unexpected CLI EOF' }
        return $pending.Result
    }
    function Send-Request([Diagnostics.Process]$Process, [string[]]$Fields) {
        $process.StandardInput.WriteLine(($Fields -join "`t"))
        $process.StandardInput.Flush()
        $rows = [Collections.Generic.List[string]]::new()
        do { $row = Read-Line -Process $Process; $rows.Add($row) } while (-not ($row.StartsWith("ok`t") -or $row.StartsWith("error`t")))
        $result = $rows.ToArray()
        return ,$result
    }
    function Assert([bool]$Value,[string]$Message) { if (-not $Value) { throw $Message } }
    [string]$greeting = Read-Line -Process $process
    Assert ($greeting -eq "ready`tSwiftEdit`t1") 'Protocol greeting'
    $response = Send-Request -Process $process -Fields @('preview','','','','','extra')
    Assert ($response[-1].StartsWith("error`tToo many")) 'Request field storage is bounded'
    $response = Send-Request -Process $process -Fields @('preview','','','','hello\nworld')
    $preview = $response[0].Split("`t")
    Assert ($preview[0] -eq 'preview') 'Explicit preview returned'
    $response = Send-Request -Process $process -Fields @('commit',$preview[1],$preview[2])
    Assert ($response[-1].StartsWith("ok`tcommit")) 'Preview commit accepted'
    $response = Send-Request -Process $process -Fields @('commit',$preview[1],$preview[2])
    Assert ($response[-1].StartsWith("error`tStale")) 'Stale commit refused'
    $response = Send-Request -Process $process -Fields @('page','0','4096')
    Assert ($response[0] -eq "page`t0`t11`t11`thello\x0aworld") 'Exact escaped page'
    $response = Send-Request -Process $process -Fields @('word-count')
    Assert ($response[0] -eq "word-count`t2") 'Shared word count observes working text'
    $target = (Join-Path $fixture 'new.txt').Replace('\','\\')
    $response = Send-Request -Process $process -Fields @('save-as',$target)
    Assert ($response[-1].StartsWith("ok`tsave-as")) 'Save creates new file'
    [string]$savedPath = Join-Path $fixture 'new.txt'
    [string]$savedText = [IO.File]::ReadAllText($savedPath)
    Assert ($savedText -eq "hello`nworld") 'File bytes match explicit edit'
    $response = Send-Request -Process $process -Fields @('undo')
    Assert ($response[-1].StartsWith("error`tAt last-save")) 'Save boundary through process'
    $response = Send-Request -Process $process -Fields @('restore-opened')
    $response = Send-Request -Process $process -Fields @('open',$target)
    Assert ($response[-1].StartsWith("error`tUnsaved")) 'Open cannot silently discard'
    $response = Send-Request -Process $process -Fields @('discard')
    $blankFile = Join-Path $fixture 'blank.txt'
    [IO.File]::WriteAllText($blankFile,"a`r`n`r`n `n`n")
    $response = Send-Request -Process $process -Fields @('open',$blankFile.Replace('\','\\'))
    Assert ($response[0].StartsWith("context`t")) 'Open emits stamped context'
    Assert ($response -contains "blank`t3`t2`t\x0d\x0dL2\x0d\x0d") 'True blank has escaped marker metadata'
    Assert ($response -contains "blank`t7`t4`t\x0d\x0dL4\x0d\x0d") 'Whitespace-only line does not get marker'
    $response = Send-Request -Process $process -Fields @('context','2')
    Assert ($response[1] -eq "page`t0`t2`t8`ta\x0d") 'Bounded context restart'
    $response = Send-Request -Process $process -Fields @('context-next','2')
    Assert ($response -contains "blank`t3`t2`t\x0d\x0dL2\x0d\x0d") 'Split CRLF preserves line metadata'
    $response = Send-Request -Process $process -Fields @('preview','','a','','\r\rL2\r\r')
    Assert ($response[-1].StartsWith("error`t")) 'Metadata echoed into replacement is refused'
    $response = Send-Request -Process $process -Fields @('preview','','a','','b')
    $blankPreview = $response[0].Split("`t")
    $response = Send-Request -Process $process -Fields @('commit',$blankPreview[1],$blankPreview[2])
    $response = Send-Request -Process $process -Fields @('context-next','2')
    Assert ($response[-1].StartsWith("error`tContext changed")) 'Context traversal rejects stale revision'
    $response = Send-Request -Process $process -Fields @('discard')
    $csv = Join-Path $fixture 'table.csv'
    [IO.File]::WriteAllText($csv,"0.1,0.2`n4,text")
    $response = Send-Request -Process $process -Fields @('open',$csv.Replace('\','\\'))
    $response = Send-Request -Process $process -Fields @('csv-calculate','SUM(A1:B1)')
    Assert ($response[0] -eq "calculation`t0.3`tA1`tB1") 'Exact decimal result with references'
    $response = Send-Request -Process $process -Fields @('csv-calculate','SUM(A1:B2)')
    Assert ($response[-1].StartsWith("error`tB2:")) 'Invalid referenced cell identified'
    $response = Send-Request -Process $process -Fields @('csv-set','B2','7')
    $preview = $response[0].Split("`t")
    [string]$previewDiskText = [IO.File]::ReadAllText($csv)
    Assert ($previewDiskText.EndsWith('text')) 'CSV preview does not write disk'
    $response = Send-Request -Process $process -Fields @('commit',$preview[1],$preview[2])
    $response = Send-Request -Process $process -Fields @('csv-calculate','SUM(A1:B2)')
    Assert ($response[0].StartsWith("calculation`t11.3`t")) 'CSV commit updates calculation source'
    $response = Send-Request -Process $process -Fields @('csv-set','B2','=A2*2')
    $preview = $response[0].Split("`t")
    $response = Send-Request -Process $process -Fields @('commit',$preview[1],$preview[2])
    $response = Send-Request -Process $process -Fields @('csv-value','B2')
    Assert ($response[0] -eq "value`t8`tA2") 'Formula is evaluated with reference metadata'
    $response = Send-Request -Process $process -Fields @('csv-convert-to-value','B2')
    $preview = $response[0].Split("`t")
    $response = Send-Request -Process $process -Fields @('commit',$preview[1],$preview[2])
    $response = Send-Request -Process $process -Fields @('csv-get','B2')
    Assert ($response[0] -eq "cell`tB2`t8") 'Conversion stores a literal value'
    $response = Send-Request -Process $process -Fields @('undo')
    $response = Send-Request -Process $process -Fields @('csv-get','B2')
    Assert ($response[0] -eq "cell`tB2`t=A2*2") 'Conversion undo restores the formula'
    $response = Send-Request -Process $process -Fields @('quit')
    [bool]$exited = $process.WaitForExit(5000)
    Assert ($exited -and $process.ExitCode -eq 0) 'Clean process exit'
    [string]$quitDiskText = [IO.File]::ReadAllText($csv)
    Assert ($quitDiskText.EndsWith('text')) 'Quit does not autosave'
    Write-Output 'CLI integration passed: transport, preview/commit, stale guard, file publication, undo boundary, dirty-open refusal and exact CSV calculation.'
} finally {
    if ($started -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    $process.Dispose()
    $resolved = [IO.Path]::GetFullPath($fixture)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if (-not $resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or -not ([IO.Path]::GetFileName($resolved)).StartsWith('swiftedit-cli-')) { throw 'Fixture cleanup path validation failed' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
