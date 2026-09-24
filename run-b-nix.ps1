[CmdletBinding()]
param([string]$ImageDirectory = $PSScriptRoot, [int]$MemoryMB = 1024, [switch]$Fullscreen)
$ErrorActionPreference = "Stop"

function Find-BNixFile([string]$Name) {
    $item = Get-ChildItem -LiteralPath $ImageDirectory -Recurse -File -Filter $Name | Select-Object -First 1
    if ($null -eq $item) { throw "Cannot find $Name below '$ImageDirectory'. Extract the complete b-nix-linux-build artifact." }
    $item.FullName
}

$qemu = (Get-Command qemu-system-x86_64.exe -ErrorAction SilentlyContinue).Source
if (-not $qemu) { $qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe" }
if (-not (Test-Path -LiteralPath $qemu -PathType Leaf)) { throw "QEMU x86_64 is not installed or available on PATH." }

$kernel = Find-BNixFile "bzImage"
$rootfs = Find-BNixFile "rootfs.ext4"
Write-Host "Starting B-nix Linux graphical image"
Write-Host "Kernel: $kernel"
Write-Host "Rootfs: $rootfs"
Write-Host "Use Ctrl+Alt+G to release the pointer."

$qemuArgs = @("-M","pc","-cpu","max","-m","$($MemoryMB)M","-kernel",$kernel,
    "-append","rootwait root=/dev/vda console=ttyS0",
    "-drive","file=$rootfs,if=virtio,format=raw",
    "-netdev","user,id=net0","-device","virtio-net-pci,netdev=net0",
    "-device","virtio-vga","-display","sdl","-serial","stdio","-no-reboot")
if ($Fullscreen) { $qemuArgs += "-full-screen" }
& $qemu @qemuArgs
if ($LASTEXITCODE -ne 0) { throw "QEMU exited with code $LASTEXITCODE." }
