param([string]$SdkPath=$env:PICO_SDK_PATH, [string]$ToolchainPath=$env:PICO_TOOLCHAIN_PATH)
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
if(-not $SdkPath){$SdkPath=Join-Path $here '../external/pico-sdk'}
$buildArgs=@('-S',$here,'-B',(Join-Path $here 'build-rp2350'),'-G','Ninja',"-DPICO_SDK_PATH=$SdkPath",'-DPICO_BOARD=waveshare_core2350b')
if($ToolchainPath){$buildArgs+="-DPICO_TOOLCHAIN_PATH=$ToolchainPath"}
python -m cmake @buildArgs
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
python -m cmake --build (Join-Path $here 'build-rp2350') -j 6
if($LASTEXITCODE -ne 0){throw 'Build failed'}
python (Join-Path $here 'pack_uf2.py')
if($LASTEXITCODE -ne 0){throw 'UF2 packaging failed'}
