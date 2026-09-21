param (
    [string] $OutputPath = (Join-Path $PSScriptRoot 'MIDI\Teratoamor_Width_Listening_Test.mid')
)

$bytes = [System.Collections.Generic.List[byte]]::new()

function Add-Byte([int] $Value) {
    $script:bytes.Add([byte] $Value)
}

function Add-Bytes([byte[]] $Values) {
    $script:bytes.AddRange($Values)
}

function Add-Ascii([string] $Text) {
    Add-Bytes ([System.Text.Encoding]::ASCII.GetBytes($Text))
}

function Add-BigEndian16([int] $Value) {
    Add-Byte (($Value -shr 8) -band 0xff)
    Add-Byte ($Value -band 0xff)
}

function Add-BigEndian32([int] $Value) {
    Add-Byte (($Value -shr 24) -band 0xff)
    Add-Byte (($Value -shr 16) -band 0xff)
    Add-Byte (($Value -shr 8) -band 0xff)
    Add-Byte ($Value -band 0xff)
}

function Add-VariableLength([int] $Value) {
    $encoded = [System.Collections.Generic.List[byte]]::new()
    $encoded.Add([byte] ($Value -band 0x7f))
    while (($Value = $Value -shr 7) -gt 0) {
        $encoded.Insert(0, [byte] (($Value -band 0x7f) -bor 0x80))
    }
    Add-Bytes $encoded.ToArray()
}

function Add-MetaText([int] $Type, [string] $Text, [int] $Delta = 0) {
    Add-VariableLength $Delta
    Add-Bytes ([byte[]] @(0xff, $Type))
    $textBytes = [System.Text.Encoding]::ASCII.GetBytes($Text)
    Add-VariableLength $textBytes.Length
    Add-Bytes $textBytes
}

function Add-TestNote([int] $Note, [string] $Name, [int] $LeadInTicks) {
    Add-MetaText 0x06 $Name $LeadInTicks
    Add-Bytes ([byte[]] @(0x00, 0x90, $Note, 100))
    Add-VariableLength 1920 # Four beats at 60 BPM = four seconds.
    Add-Bytes ([byte[]] @(0x80, $Note, 0))
}

# Header: format 0, one track, 480 ticks per quarter note.
Add-Ascii 'MThd'
Add-BigEndian32 6
Add-BigEndian16 0
Add-BigEndian16 1
Add-BigEndian16 480

$trackLengthOffset = $bytes.Count + 4
Add-Ascii 'MTrk'
Add-BigEndian32 0

Add-MetaText 0x03 'Teratoamor Width Listening Test'
Add-Bytes ([byte[]] @(0x00, 0xff, 0x51, 0x03, 0x0f, 0x42, 0x40)) # 60 BPM
Add-TestNote 36 'C2 (MIDI 36)' 480
Add-TestNote 60 'C4 (MIDI 60)' 480
Add-TestNote 84 'C6 (MIDI 84)' 480
Add-Bytes ([byte[]] @(0x83, 0x60, 0xff, 0x2f, 0x00)) # One-second tail, then end.

$trackLength = $bytes.Count - ($trackLengthOffset + 4)
for ($index = 0; $index -lt 4; ++$index) {
    $shift = 8 * (3 - $index)
    $bytes[$trackLengthOffset + $index] = [byte] (($trackLength -shr $shift) -band 0xff)
}

$outputDirectory = Split-Path -Parent $OutputPath
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
[System.IO.File]::WriteAllBytes($OutputPath, $bytes.ToArray())
Write-Output $OutputPath
