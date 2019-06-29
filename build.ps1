param(
  [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
  [string[]]$files
)

if (-not $files.Length) {
  $files = Get-ChildItem .\*.cpp
}

foreach ($file in $files) {
  cl.exe /nologo /EHsc /std:c++17 $file
}
rm .\*.obj
