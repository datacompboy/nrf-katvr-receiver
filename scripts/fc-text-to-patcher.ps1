# Convert output of "fc.exe /L /N file1.txt file2.txt" into script that will patch file1.txt to became file2.txt.
# Usage:
#   fc.exe /L /N file1.txt file2.txt | fc-text-to-patcher.ps1 > patch-file1.ps1
# Patch file usage:
#   cat .\file1.hex | .\patch-file1.ps1 | Out-File -FilePath file2.txt -Encoding ascii
#
enum Stages {
    sHeader
    sWait
    sOrig
    sPatch
}
[Stages]$stage = [Stages]::sHeader
$Input | ForEach-Object {
    if ($stage -eq [Stages]::sHeader) {
        if ($_ -match "^Comparing files") {
            $stage = [Stages]::sWait
            Write-Output '$in_line = 0'
            Write-Output '$Input | ForEach-Object {'
            Write-Output '  $in_line++'
            Write-Output '  $skip = 0'
        }
    }
    elseif ($stage -eq [Stages]::sWait) {
        if ($_ -match "^[*][*][*][*][*] ") {
            $stage = [Stages]::sOrig
        }
    }
    elseif ($stage -eq [Stages]::sOrig) {
        if ($_ -match "^[*][*][*][*][*] ") {
            $stage = [Stages]::sPatch
        } else {
            $lineNo, $lineTxt = $_ -split ":  "
            Write-Output "  if (`$in_line -eq $lineNo) {"
            Write-Output "    if (`$_ -ne '$lineTxt') { throw 'File content mismatch'; }"
            Write-Output '    $skip = 1'
            Write-Output '  }'
        }
    }
    elseif ($stage -eq [Stages]::sPatch) {
        if ($_ -match "^[*][*][*][*][*]") {
            $stage = [Stages]::sWait
        } else {
            $patchLineNo, $lineTxt = $_ -split ":  "
            Write-Output "  if (`$in_line -eq $lineNo) {"
            Write-Output "    Write-Output '$lineTxt'"
            Write-Output '  }'
        }
    }
}
Write-Output '  if ($skip -eq 0) {'
Write-Output '    Write-Output $_'
Write-Output '  }'
Write-Output '}'
