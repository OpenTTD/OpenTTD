function ResizeImage() {
    param([String]$sourcePath, [Int]$targetWidth, [Int]$targetHeight, [String]$targetPath)

    Add-Type -AssemblyName "System.Drawing"

    $img = [System.Drawing.Image]::FromFile($sourcePath)

    $ratioX = $targetWidth / $img.Width;
    $ratioY = $targetHeight / $img.Height;

    $ratio = $ratioY

    if ($ratioX -le $ratioY) {
        $ratio = $ratioX
    }

    $newWidth = [int] ($img.Width * $ratio)
    $newHeight = [int] ($img.Height * $ratio)

    $resizedImage = New-Object System.Drawing.Bitmap($targetWidth, $targetHeight)
    $graph = [System.Drawing.Graphics]::FromImage($resizedImage)
    $graph.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic

    $graph.Clear([System.Drawing.Color]::Transparent)
    $graph.DrawImage($img, $targetWidth / 2 - $newWidth / 2, $targetHeight / 2 - $newHeight / 2, $newWidth, $newHeight)

    $resizedImage.Save($targetPath)
    $resizedImage.Dispose()
    $img.Dispose()
}

$logoPath = "..\..\..\media\openttd.2048.png"

# Create the main image assets required for the Windows Store
New-Item -Path "." -Name "assets" -ItemType "directory" -Force
ResizeImage $logoPath 1240 1240 "assets\LargeTile.png"
ResizeImage $logoPath 284 284 "assets\SmallTile.png"
ResizeImage $logoPath 2480 1200 "assets\SplashScreen.png"
ResizeImage $logoPath 176 176 "assets\Square44x44Logo.png"
Copy-Item "assets\Square44x44Logo.png" -Destination "assets\Square44x44Logo.targetsize-44_altform-unplated.png"
ResizeImage $logoPath 600 600 "assets\Square150x150Logo.png"
Copy-Item "assets\Square150x150Logo.png" -Destination "assets\Square150x150Logo.targetsize-150_altform-unplated.png"
ResizeImage $logoPath 200 200 "assets\StoreLogo.png"
ResizeImage $logoPath 1240 600 "assets\Wide310x150Logo.png"

# Copy the logo for the store for the common package
New-Item -Path "." -Name "assets-common" -ItemType "directory" -Force
Copy-Item "assets\StoreLogo.png" -Destination "assets-common\StoreLogoCommon.png"
