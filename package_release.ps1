function CreateReleaseZip 
{
    param (
        $FilesToZip,
        $OutputName
    )
    # remove old zip file
    if (Test-Path $OutputName) 
    { 
        Remove-Item $OutputName -ErrorAction Stop 
    }
    Push-Location ".."
    $FullFilenames = $FilesToZip | ForEach-Object -Process {Write-Output -InputObject $_.FullName}

    # #create zip file
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem

    $zip = [System.IO.Compression.ZipFile]::Open(($OutputName), [System.IO.Compression.ZipArchiveMode]::Create)

    # #write entries with relative paths as names
    foreach ($fname in $FullFilenames) 
    {
        $rname = $(Resolve-Path -Path $fname -Relative) -replace '\.\\',''
        Write-Output $rname
        $zentry = $zip.CreateEntry($rname)
        $zentryWriter = New-Object -TypeName System.IO.BinaryWriter $zentry.Open()
        $zentryWriter.Write([System.IO.File]::ReadAllBytes($fname))
        $zentryWriter.Flush()
        $zentryWriter.Close()
    }

    # #release zip file
    $zip.Dispose()
    Pop-Location
}

# Set-Location ".."

# collecting list of files that we want to archive excluding those that we don't want to preserve
Push-Location ".."
$Files  = @(Get-ChildItem "plugins" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { ($_.Extension -in '.dll')})
$Files  += @(Get-ChildItem "data" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { ($_.Extension -in '.png')})
Pop-Location
CreateReleaseZip $Files ("..\" +$args[0] + "_RELEASE.zip")

Push-Location ".."
$Files  += @(Get-ChildItem "plugins" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { ($_.Extension -in '.pdb')})
Pop-Location
CreateReleaseZip $Files ("..\" + $args[0] + "_PDB.zip")
