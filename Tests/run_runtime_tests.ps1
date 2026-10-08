param([string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$testDir = Join-Path ([System.IO.Path]::GetTempPath()) ('bpc-runtime-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDir | Out-Null
$utf8 = [System.Text.UTF8Encoding]::new($false)
$source = [System.IO.File]::ReadAllText((Join-Path $repoRoot 'User/bsp_eeprom.c'))
$marker = 'uint32_t g_runtime_total_minutes = 0;'
$start = $source.IndexOf($marker)
if ($start -lt 0) { throw 'Production runtime section was not found.' }
[System.IO.File]::WriteAllText((Join-Path $testDir 'runtime_impl.inc'), $source.Substring($start), $utf8)
$header = Get-Content -Encoding UTF8 (Join-Path $repoRoot 'User/bsp_mo_i2c.h')
$rtos = Get-Content -Encoding UTF8 (Join-Path $repoRoot 'User/FreeRTOSConfig.h')
$defines = @($header | Where-Object { $_ -match '^#define\s+RUNTIME_' })
$defines += @($header | Where-Object { $_ -match '^#define\s+EE_(PAGE_SIZE|SIZE)\s' })
$defines += @($rtos | Where-Object { $_ -match '^#define\s+configTICK_RATE_HZ\s' })
[System.IO.File]::WriteAllLines((Join-Path $testDir 'runtime_config.h'), [string[]]$defines, $utf8)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'runtime_test.c') -Destination $testDir
$exe = Join-Path $testDir 'runtime_test.exe'
& $Compiler -std=c99 -O2 -Wall -Wextra -Werror -pedantic (Join-Path $testDir 'runtime_test.c') -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Runtime test compilation failed.' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Runtime regression tests failed.' }

$relayHeader = [System.IO.File]::ReadAllText((Join-Path $repoRoot 'User/timer/bsp_pwm.h'))
$relayType = [regex]::Match($relayHeader, '(?s)typedef struct \{.*?\} RELAY_SignalTypeDef;').Value
$relay = [System.IO.File]::ReadAllText((Join-Path $repoRoot 'User/timer/bsp_pwm.c'))
$relayStart = $relay.IndexOf('RELAY_SignalTypeDef g_fan_pwm_relay')
$relayEnd = $relay.IndexOf('// PC6/PA9')
if (-not $relayType -or $relayStart -lt 0 -or $relayEnd -le $relayStart) { throw 'Relay production code not found.' }
[System.IO.File]::WriteAllText((Join-Path $testDir 'relay_impl.inc'), $relayType + "`n" + $relay.Substring($relayStart, $relayEnd - $relayStart), $utf8)
$interrupts = [System.IO.File]::ReadAllText((Join-Path $repoRoot 'User/stm32f10x_it.c'))
$sysTick = [regex]::Match($interrupts, '(?ms)^void SysTick_Handler\(void\).*?^\}').Value
if (-not $sysTick) { throw 'Production SysTick handler not found.' }
[System.IO.File]::WriteAllText((Join-Path $testDir 'systick_impl.inc'), $sysTick, $utf8)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'relay_tick_test.c') -Destination $testDir
$relayExe = Join-Path $testDir 'relay_tick_test.exe'
& $Compiler -std=c99 -O2 -Wall -Wextra -Werror -pedantic (Join-Path $testDir 'relay_tick_test.c') -o $relayExe
if ($LASTEXITCODE -ne 0) { throw 'Relay test compilation failed.' }
& $relayExe
if ($LASTEXITCODE -ne 0) { throw 'Relay tick regression tests failed.' }

$serial = [System.IO.File]::ReadAllText((Join-Path $repoRoot 'User/usart/bsp_usart.c'))
$serialStart = $serial.IndexOf('#define RESET_CMD')
$serialEnd = $serial.IndexOf('/* DEBUG TX serialization:')
if ($serialStart -lt 0 -or $serialEnd -le $serialStart) { throw 'Serial command implementation not found.' }
[System.IO.File]::WriteAllText((Join-Path $testDir 'serial_impl.inc'), $serial.Substring($serialStart, $serialEnd - $serialStart), $utf8)
$serialDefines = @(Get-Content -Encoding UTF8 (Join-Path $repoRoot 'User/usart/bsp_usart.h') | Where-Object { $_ -match '^#define\s+SERIAL_COMMAND_' })
[System.IO.File]::WriteAllLines((Join-Path $testDir 'serial_config.h'), [string[]]$serialDefines, $utf8)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'serial_command_test.c') -Destination $testDir
$serialExe = Join-Path $testDir 'serial_command_test.exe'
& $Compiler -std=c99 -O2 -Wall -Wextra -Werror -pedantic (Join-Path $testDir 'serial_command_test.c') -o $serialExe
if ($LASTEXITCODE -ne 0) { throw 'Serial command test compilation failed.' }
& $serialExe
if ($LASTEXITCODE -ne 0) { throw 'Serial command regression tests failed.' }
$txHelpersStart = $serial.IndexOf('/* DEBUG TX serialization:')
$txHelpersEnd = $serial.IndexOf('void USART_Config(void)')
$txStart = $serial.IndexOf('// UART TX entry points:')
$txEnd = $serial.IndexOf('int fgetc(FILE *f)')
if ($txHelpersStart -lt 0 -or $txHelpersEnd -le $txHelpersStart -or $txStart -lt 0 -or $txEnd -le $txStart) { throw 'UART TX implementation not found.' }
$txSource = $serial.Substring($txHelpersStart, $txHelpersEnd - $txHelpersStart) + $serial.Substring($txStart, $txEnd - $txStart)
[System.IO.File]::WriteAllText((Join-Path $testDir 'serial_tx_impl.inc'), $txSource, $utf8)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'serial_tx_test.c') -Destination $testDir
$txExe = Join-Path $testDir 'serial_tx_test.exe'
& $Compiler -std=c99 -O2 -Wall -Wextra -Werror -pedantic (Join-Path $testDir 'serial_tx_test.c') -o $txExe
if ($LASTEXITCODE -ne 0) { throw 'UART TX test compilation failed.' }
& $txExe
if ($LASTEXITCODE -ne 0) { throw 'UART TX regression tests failed.' }
Write-Output "Test artifacts: $testDir"
