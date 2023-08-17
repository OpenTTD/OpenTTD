[CmdletBinding()]
[Alias()]
Param
(
    # Publisher ("CN=xyz")
    [Parameter(Mandatory=$true, Position=0)]
    $Publisher,

    # Password
    [Parameter(Mandatory=$true, Position=1)]
    $PasswordParam,

    # Filename
    [Parameter(Mandatory=$true, Position=2)]
    $OutputFilename
)

$cert = New-SelfSignedCertificate -Type Custom -Subject $Publisher -KeyUsage DigitalSignature -FriendlyName "OpenTTD signing certificate" -CertStoreLocation "Cert:\CurrentUser\My" -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")

$password = ConvertTo-SecureString -String $PasswordParam -Force -AsPlainText
Export-PfxCertificate -cert "Cert:\CurrentUser\My\$($cert.Thumbprint)" -FilePath $OutputFilename -Password $password
