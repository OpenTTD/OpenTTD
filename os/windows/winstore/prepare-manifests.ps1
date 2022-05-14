[CmdletBinding()]
[Alias()]
Param
(
    # Output folder
    [Parameter(Mandatory=$true, Position=0)]
    $OutputFolder,

    # Publisher ("CN=xyz")
    [Parameter(Mandatory=$true, Position=1)]
    $Publisher,

    # IdentityName
    [Parameter(Mandatory=$true, Position=2)]
    $IdentityName,

    # Version
    [Parameter(Mandatory=$true, Position=3)]
    $AppVersion
)

function Prepare-Manifest {
    param (
        $Architecture
    )

    (Get-Content "$($PSScriptRoot)\manifests\Package.appxmanifest").replace('$PUBLISHER$', $Publisher).replace('$IDENTITY_NAME$', $IdentityName).replace('$VERSION$', $AppVersion).replace('$ARCHITECTURE$', $Architecture) | Set-Content "$($OutputFolder)\Package-$($Architecture).appxmanifest"
}

# Prepare the application binary manifests
Prepare-Manifest x86
Prepare-Manifest x64
Prepare-Manifest arm64

# Prepare the assets package manifest
(Get-Content "$($PSScriptRoot)\manifests\AssetsPackage.appxmanifest").replace('$PUBLISHER$', $Publisher).replace('$IDENTITY_NAME$', $IdentityName).replace('$VERSION$', $AppVersion) | Set-Content "$($OutputFolder)\AssetsPackage.appxmanifest"

# Prepare the overall package manifest
(Get-Content "$($PSScriptRoot)\manifests\Package.appxmanifest").replace('$PUBLISHER$', $Publisher).replace('$IDENTITY_NAME$', $IdentityName).replace('$VERSION$', $AppVersion).replace(' ProcessorArchitecture="$ARCHITECTURE$"', '') | Set-Content "$($OutputFolder)\Package.appxmanifest"

# Copy the PackagingLayout XML file
(Get-Content "$($PSScriptRoot)\manifests\PackagingLayout.xml") | Set-Content "$($OutputFolder)\PackagingLayout.xml"
