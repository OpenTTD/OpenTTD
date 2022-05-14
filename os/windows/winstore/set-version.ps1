[CmdletBinding()]
[Alias()]
Param
(
    # EXE path
    [Parameter(Mandatory=$true, Position=0)]
    $ExePath
)

try
{
    $versionInfo = (Get-Item "$ExePath").VersionInfo

    # Generate the app version - the build number (MS calls it revision) is always 0 because the Windows Store requires that
    $AppVersion = "$($versionInfo.FileMajorPart).$($versionInfo.FileMinorPart).$($versionInfo.FileBuildPart).0"

    Write-Output "SET OTTD_VERSION=$($AppVersion)"
}
catch
{
    Write-Output "@ECHO Error retrieving EXE version - did you provide a path?"
    exit 1
}
