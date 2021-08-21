param(
  [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
  [string[]]$files
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
  cl.exe /nologo /EHsc /std:c++latest /O1 /MD /DNDEBUG /GF /GR- /GL $file
}
Remove-Item .\*.obj
