$ErrorActionPreference = 'Stop'

$datasetDirectory = Join-Path $PSScriptRoot 'ML_canine_data'
$archivePath = Join-Path $env:TEMP 'pawstate-dog-posture-dataset-v1.zip'
$extractionDirectory = Join-Path $env:TEMP 'pawstate-dog-posture-dataset-v1'
$downloadUrl = 'https://data.mendeley.com/public-api/zip/mpph6bmn7g/download/1'

New-Item -ItemType Directory -Force -Path $datasetDirectory | Out-Null
Write-Host 'Downloading Mendeley Data dataset version 1...'
Invoke-WebRequest -Uri $downloadUrl -OutFile $archivePath
if (Test-Path $extractionDirectory) {
    Remove-Item $extractionDirectory -Recurse -Force
}
Expand-Archive -Path $archivePath -DestinationPath $extractionDirectory -Force

Copy-Item (Join-Path $extractionDirectory 'df_raw.csv') $datasetDirectory -Force
Copy-Item (Join-Path $extractionDirectory 'df_dogs.csv') $datasetDirectory -Force
if (Test-Path (Join-Path $extractionDirectory 'readme.md')) {
    Copy-Item (Join-Path $extractionDirectory 'readme.md') (Join-Path $datasetDirectory 'dataset-readme.md') -Force
}

$expected = @{
    'df_raw.csv' = 'AC04F51F35F5396C5C73A651974BBA56070177BFB30A393266F5A02DD41C0C93'
    'df_dogs.csv' = 'D24988E00BF8E8E670007C1D3A67CA767B926CF33CA2F3FD4256768FF112CC7D'
}

foreach ($name in $expected.Keys) {
    $path = Join-Path $datasetDirectory $name
    if (-not (Test-Path $path)) {
        throw "Expected dataset file was not found: $path"
    }
    $actual = (Get-FileHash -Path $path -Algorithm SHA256).Hash
    if ($actual -ne $expected[$name]) {
        throw "Checksum mismatch for $name. Expected $($expected[$name]); got $actual"
    }
    Write-Host "Verified $name"
}

Remove-Item $archivePath -Force
Remove-Item $extractionDirectory -Recurse -Force
Write-Host 'Dataset downloaded and verified in ML_canine_data.'
