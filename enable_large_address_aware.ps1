# Large Address Aware Patcher for Fallout New Vegas
# This script enables LAA support to increase virtual memory from 2GB to 4GB

Write-Host "Large Address Aware Patcher for Fallout New Vegas" -ForegroundColor Green
Write-Host "This will increase virtual memory limit from 2GB to 4GB" -ForegroundColor Yellow
Write-Host

# Function to check if file is already LAA enabled
function Test-LargeAddressAware {
    param([string]$FilePath)
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        
        # Find PE header
        $peOffset = [BitConverter]::ToUInt32($bytes, 60)
        if ($peOffset -ge $bytes.Length - 4) { return $false }
        
        # Check PE signature
        $peSignature = [BitConverter]::ToUInt32($bytes, $peOffset)
        if ($peSignature -ne 0x00004550) { return $false }  # "PE\0\0"
        
        # Get characteristics field (PE header + 22 bytes)
        $characteristicsOffset = $peOffset + 22
        if ($characteristicsOffset + 2 -ge $bytes.Length) { return $false }
        
        $characteristics = [BitConverter]::ToUInt16($bytes, $characteristicsOffset)
        
        # Check if IMAGE_FILE_LARGE_ADDRESS_AWARE (0x0020) is set
        return ($characteristics -band 0x0020) -ne 0
    }
    catch {
        return $false
    }
}

# Function to enable LAA
function Enable-LargeAddressAware {
    param([string]$FilePath)
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        
        # Find PE header
        $peOffset = [BitConverter]::ToUInt32($bytes, 60)
        $characteristicsOffset = $peOffset + 22
        
        # Get current characteristics
        $characteristics = [BitConverter]::ToUInt16($bytes, $characteristicsOffset)
        
        # Set LAA flag (IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020)
        $newCharacteristics = $characteristics -bor 0x0020
        
        # Write back the modified characteristics
        $characteristicsBytes = [BitConverter]::GetBytes([UInt16]$newCharacteristics)
        $bytes[$characteristicsOffset] = $characteristicsBytes[0]
        $bytes[$characteristicsOffset + 1] = $characteristicsBytes[1]
        
        # Create backup
        $backupPath = $FilePath + ".backup"
        if (-not (Test-Path $backupPath)) {
            Copy-Item $FilePath $backupPath
            Write-Host "Created backup: $backupPath" -ForegroundColor Cyan
        }
        
        # Write modified file
        [System.IO.File]::WriteAllBytes($FilePath, $bytes)
        
        return $true
    }
    catch {
        Write-Host "Error: $_" -ForegroundColor Red
        return $false
    }
}

# Find Fallout New Vegas installation
$gamePaths = @(
    "C:\Steam\steamapps\common\Fallout New Vegas",
    "C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas",
    "C:\GOG Games\Fallout New Vegas",
    "C:\Program Files (x86)\GOG Galaxy\Games\Fallout New Vegas"
)

$foundPath = $null
foreach ($path in $gamePaths) {
    if (Test-Path "$path\FalloutNV.exe") {
        $foundPath = $path
        break
    }
}

if (-not $foundPath) {
    Write-Host "Fallout New Vegas not found in common locations." -ForegroundColor Yellow
    Write-Host "Please enter the path to your Fallout New Vegas installation:"
    $customPath = Read-Host "Path"
    
    if ($customPath -and (Test-Path "$customPath\FalloutNV.exe")) {
        $foundPath = $customPath
    } else {
        Write-Host "ERROR: FalloutNV.exe not found at specified path!" -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }
}

$exePath = "$foundPath\FalloutNV.exe"
Write-Host "Found Fallout New Vegas: $exePath" -ForegroundColor Green

# Check current LAA status
if (Test-LargeAddressAware $exePath) {
    Write-Host "✓ Large Address Aware is already ENABLED" -ForegroundColor Green
    Write-Host "Virtual memory limit: 4GB" -ForegroundColor Green
    Write-Host
    Write-Host "Your game should work fine with MemoryPoolNVSE RPmalloc plugin." -ForegroundColor Green
} else {
    Write-Host "✗ Large Address Aware is currently DISABLED" -ForegroundColor Red
    Write-Host "Virtual memory limit: 2GB (may cause crashes with memory-intensive mods)" -ForegroundColor Yellow
    Write-Host
    
    $confirm = Read-Host "Enable Large Address Aware support? This is recommended for stability. (Y/n)"
    
    if ($confirm -eq '' -or $confirm -eq 'y' -or $confirm -eq 'Y') {
        Write-Host "Enabling Large Address Aware..." -ForegroundColor Yellow
        
        if (Enable-LargeAddressAware $exePath) {
            Write-Host "✓ SUCCESS: Large Address Aware enabled!" -ForegroundColor Green
            Write-Host "Virtual memory limit increased from 2GB to 4GB" -ForegroundColor Green
            Write-Host
            Write-Host "Benefits:" -ForegroundColor Cyan
            Write-Host "• Reduces crashes from memory-intensive mods" -ForegroundColor White
            Write-Host "• Allows MemoryPoolNVSE RPmalloc to work properly" -ForegroundColor White
            Write-Host "• Improves overall game stability" -ForegroundColor White
            Write-Host
            Write-Host "The game should now work much better with the MemoryPoolNVSE plugin!" -ForegroundColor Green
        } else {
            Write-Host "ERROR: Failed to enable Large Address Aware!" -ForegroundColor Red
            Write-Host "You may need to run this script as Administrator." -ForegroundColor Yellow
        }
    } else {
        Write-Host "LAA not enabled. The game may experience memory-related crashes." -ForegroundColor Yellow
        Write-Host "You can run this script again later if you change your mind." -ForegroundColor Gray
    }
}

Write-Host
Write-Host "Additional Notes:" -ForegroundColor Cyan
Write-Host "• This modification is safe and commonly used by the modding community" -ForegroundColor White
Write-Host "• It does not affect game compatibility or achievements" -ForegroundColor White
Write-Host "• You can restore from backup if needed: FalloutNV.exe.backup" -ForegroundColor White
Write-Host "• Most Fallout New Vegas mod managers also provide LAA patching" -ForegroundColor White

Write-Host
Read-Host "Press Enter to exit"