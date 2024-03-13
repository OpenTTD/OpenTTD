# Signing script for Azure Code Signing
# Arguments: sign_azure.ps1 path_to_sign
#
# Environment variables must be set up before use:
#
# AZURE_TENANT_ID
# AZURE_CLIENT_ID
# AZURE_CLIENT_SECRET
# AZURE_CODESIGN_ACCOUNT_NAME
# AZURE_CODESIGN_ENDPOINT
# AZURE_CODESIGN_PROFILE_NAME

Param
(
    # Files folder
    [Parameter(Mandatory=$true, Position=0)]
    $FilesFolder
)

if (!$Env:AZURE_CODESIGN_ENDPOINT -or !$Env:AZURE_CODESIGN_ACCOUNT_NAME -or !$Env:AZURE_CODESIGN_PROFILE_NAME -or
	!$Env:AZURE_TENANT_ID -or !$Env:AZURE_CLIENT_ID -or !$Env:AZURE_CLIENT_SECRET)
{
	"Code signing variables not found; most likely running in a fork. Skipping signing."
	exit
}

Install-Module -Name AzureCodeSigning -Scope CurrentUser -RequiredVersion 0.3.0 -Force -Repository PSGallery

$params = @{}

$params["Endpoint"] = $Env:AZURE_CODESIGN_ENDPOINT
$params["CodeSigningAccountName"] = $Env:AZURE_CODESIGN_ACCOUNT_NAME
$params["CertificateProfileName"] = $Env:AZURE_CODESIGN_PROFILE_NAME
$params["FilesFolder"] = $FilesFolder
$params["FilesFolderFilter"] = "exe"
$params["FileDigest"] = "SHA256"
$params["TimestampRfc3161"] = "http://timestamp.acs.microsoft.com"
$params["TimestampDigest"] = "SHA256"

Invoke-AzureCodeSigning @params
