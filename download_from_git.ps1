$url = "https://github.com/v-prokofev/wtrans/archive/refs/heads/main.zip"
$out = "wtrans.zip"

Invoke-WebRequest -Uri $url -OutFile $out

Expand-Archive -Path $out -DestinationPath . -Force

Remove-Item $out

Write-Host "Done"