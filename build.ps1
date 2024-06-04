param(
  [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
  [string[]]$files,
  [switch]$force
)

if (-not $files.Length) {
  $files = Get-ChildItem .\*.cpp
} else {
  for ($i = 0; $i -lt $files.Length; $i += 1) {
    if (!$files[$i].EndsWith('.cpp')) {
      $files[$i] += '.cpp'
    }
  }
}

foreach ($file in $files) {
  $cpptime = (Get-Item $file).LastWriteTimeUtc
  $exename = $file -replace '\.cpp$','.exe'
  if (Test-Path $exename) {
    $exetime = (Get-Item $exename).LastWriteTimeUtc
    if ($exetime -gt $cpptime) {
      if ($force) {
        Write-Output "Building $file even though $exename is newer."
      } else {
        Write-Output "Skipping $file because $exename is newer."
        continue
      }
    }
  }
  cl.exe /nologo /EHsc /std:c++20 /O1 /MD /DNDEBUG /GF /GR- /GL $file
  Remove-Item ($file -replace '\.cpp$','.obj')
}
