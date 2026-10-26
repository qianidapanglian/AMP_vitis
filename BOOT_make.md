# Zynq 生成 BOOT.BIN 并自动烧录到 QSPI Flash

本文记录本工程在 Windows + Vitis 2023.2 环境下，生成 `BOOT.BIN` 并通过 JTAG 自动烧录进 Zynq QSPI Flash 的方法。

## 环境前提

- 软件：Vitis 2023.2 / Vivado 2023.2
- 板卡：HelloFpga 自定义板 / `xc7z020`
- JTAG 已连接到主机（电缆：`HelloFpga JTAG-SP2 25SE191A`）
- `hw_server` 已启动，默认地址为 `tcp:localhost:3121`
- 当前工程已生成以下文件：
  - `plfm_SGDMA4/export/plfm_SGDMA4/sw/plfm_SGDMA4/boot/fsbl.elf`
  - `SG_cyclic_dma/_ide/bitstream/SGDMA4.bit`
  - `SG_cyclic_dma/Debug/SG_cyclic_dma.elf`

## 0. 前置检查：确认关键文件存在

```powershell
$root = 'D:\project\zynq_project_2023\PL_PS\AMP_vitis'
$fsbl = "$root\plfm_SGDMA3\export\plfm_SGDMA3\sw\plfm_SGDMA3\boot\fsbl.elf"
$bit  = "$root\SG_cyclic_dma\_ide\bitstream\SGDMA3.bit"
$elf  = "$root\SG_cyclic_dma\Debug\SG_cyclic_dma.elf"
Write-Host "fsbl.elf:   $(Test-Path $fsbl)"
Write-Host "bitstream:  $(Test-Path $bit)"
Write-Host "app.elf:    $(Test-Path $elf)"
```

三个文件均应返回 `True`，否则需先在 Vitis 中编译工程。

## 1. 可选：重新编译应用 ELF

如果修改了 Vitis 工程中的应用代码，可以先重新编译：

```powershell
$env:PATH='D:\software\vitis_2023_2\Vitis\2023.2\bin;D:\software\vitis_2023_2\Vitis\2023.2\gnuwin\bin;' + $env:PATH
make -C SG_cyclic_dma\Debug all
```

本工程应用源码位于：

```text
SG_cyclic_dma/src/main.c
SG_cyclic_dma/src/user.c
SG_cyclic_dma/src/user_dma.c
```

## 2. 生成 system.bif

用 PowerShell 直接生成 `SG_cyclic_dma_system/Debug/system.bif`，路径统一使用 `/`：

```powershell
$root = 'D:/project/zynq_project_2023/PL_PS/AMP_vitis'
$bif  = "$root/SG_cyclic_dma_system/Debug/system.bif"
$bifWin = $bif.Replace('/','\')
$fsbl = "$root/plfm_SGDMA4/export/plfm_SGDMA4/sw/plfm_SGDMA4/boot/fsbl.elf"
$bit  = "$root/SG_cyclic_dma/_ide/bitstream/SGDMA4.bit"
$elf  = "$root/SG_cyclic_dma/Debug/SG_cyclic_dma.elf"
New-Item -ItemType Directory -Force -Path (Split-Path $bifWin) | Out-Null
"the_ROM_image:`n{`n  [bootloader] $fsbl`n  $bit`n  $elf`n}" | Set-Content -Encoding ASCII $bifWin
```

生成后 `system.bif` 内容应为：

```text
the_ROM_image:
{
  [bootloader] D:/project/zynq_project_2023/PL_PS/AMP_vitis/plfm_SGDMA4/export/plfm_SGDMA4/sw/plfm_SGDMA4/boot/fsbl.elf
  D:/project/zynq_project_2023/PL_PS/AMP_vitis/SG_cyclic_dma/_ide/bitstream/SGDMA4.bit
  D:/project/zynq_project_2023/PL_PS/AMP_vitis/SG_cyclic_dma/Debug/SG_cyclic_dma.elf
}
```

## 3. 生成 BOOT.BIN

在工程根目录执行 `bootgen`：

```powershell
$root = 'D:\project\zynq_project_2023\PL_PS\AMP_vitis'
$bif  = "$root\SG_cyclic_dma_system\Debug\system.bif"
$boot = "$root\SG_cyclic_dma_system\Debug\BOOT.BIN"
New-Item -ItemType Directory -Force -Path (Split-Path $boot) | Out-Null
& 'D:\software\vitis_2023_2\Vitis\2023.2\bin\bootgen.bat' -image $bif -arch zynq -w -o $boot
```

验证生成结果：

```powershell
$boot = 'D:\project\zynq_project_2023\PL_PS\AMP_vitis\SG_cyclic_dma_system\Debug\BOOT.BIN'
if (Test-Path $boot) {
  $f = Get-Item $boot
  Write-Host "BOOT.BIN 已生成! 大小: $([math]::Round($f.Length/1024, 2)) KB, 时间: $($f.LastWriteTime)"
} else {
  Write-Host "BOOT.BIN 未找到，bootgen 可能失败"
}
```

本工程实测输出：

```text
BOOT.BIN 已生成! 大小: 4084.57 KB, 时间: 10/25/2026 17:53:57
```

## 4. 查看 JTAG 目标

烧录前先确认 JTAG 目标：

```powershell
& 'D:\software\vitis_2023_2\Vitis\2023.2\bin\program_flash.bat' -jtagtargets -url tcp:localhost:3121
```

本工程使用 `HelloFpga JTAG-SP2 25SE191A` 电缆，ARM DAP 的 `target_id` 需根据实际输出确认（示例）：

```text
1  HelloFpga/25SE191A/
   2  jsn-JTAG-SP2-25SE191A-4ba00477-0  (name arm_dap  idcode 4ba00477)
   3  jsn-JTAG-SP2-25SE191A-23727093-0  (name xc7z020  idcode 23727093)
```

如果你的电脑上显示的 `target_id` 不同，后面的命令要对应修改。

## 5. 自动烧录到 QSPI Flash

将 `BOOT.BIN` 写入 QSPI Flash 起始地址 `0x0`，并写后校验：

```powershell
& 'D:\software\vitis_2023_2\Vitis\2023.2\bin\program_flash.bat' `
  -f SG_cyclic_dma_system\Debug\BOOT.BIN `
  -offset 0 `
  -fsbl plfm_SGDMA4\export\plfm_SGDMA4\sw\plfm_SGDMA4\boot\fsbl.elf `
  -flash_type qspi-x4-single `
  -verify `
  -target_id 2 `
  -url tcp:localhost:3121
```

成功后会看到：

```text
Program Operation successful.
Verify Operation successful.
Flash Operation Successful
```

## 6. 一键生成并烧录脚本

下面这段 PowerShell 可在工程根目录直接执行：

```powershell
$vitis = 'D:\software\vitis_2023_2\Vitis\2023.2'
$root  = 'D:/project/zynq_project_2023/PL_PS/AMP_vitis'

$fsbl = "$root/plfm_SGDMA4/export/plfm_SGDMA4/sw/plfm_SGDMA4/boot/fsbl.elf"
$bit  = "$root/SG_cyclic_dma/_ide/bitstream/SGDMA4.bit"
$elf  = "$root/SG_cyclic_dma/Debug/SG_cyclic_dma.elf"
$bif  = "$root/SG_cyclic_dma_system/Debug/system.bif"
$boot = "$root/SG_cyclic_dma_system/Debug/sd_card/BOOT.BIN"

$bifWin  = $bif.Replace('/','\')
$bootWin = $boot.Replace('/','\')

New-Item -ItemType Directory -Force -Path (Split-Path $bifWin)  | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $bootWin) | Out-Null

"the_ROM_image:`n{`n  [bootloader] $fsbl`n  $bit`n  $elf`n}" | Set-Content -Encoding ASCII $bifWin

& "$vitis\bin\bootgen.bat" -image $bifWin -arch zynq -w -o $bootWin

& "$vitis\bin\program_flash.bat" `
  -f $bootWin `
  -offset 0 `
  -fsbl $fsbl.Replace('/','\') `
  -flash_type qspi-x4-single `
  -verify `
  -target_id 2 `
  -url tcp:localhost:3121
```

## 7. 启动注意事项

- 烧录完成后，需要把板卡启动模式切换到 QSPI。
- 重新上电或复位后，板子才会从 QSPI 中的 `BOOT.BIN` 启动。
- 如果仍处于 JTAG 启动模式，板子不会自动从 Flash 启动。

## 8. 常见问题

- `program_flash` 提示 target 不存在：先运行 `-jtagtargets`，确认正确的 `target_id`。

- `BOOT.BIN` 生成成功但板子不启动：检查启动拨码是否切到 QSPI。
- 如果硬件平台不是 `plfm_SGDMA4`，需要同步修改 `fsbl.elf`、`.bit`、`.xpfm` 等路径。
- 如果 Flash 类型不是 `qspi-x4-single`，需要按实际硬件修改 `-flash_type`。

### 烧写卡死问题
- `program_flash` 长时间停在 `Retrieving Flash info...` / `Initialization done`：
  - 这通常不是 `BOOT.BIN` 生成问题，而是 JTAG/XSCT/hw_server 会话状态卡住。
  - 先查看是否有旧的 `rdi_xsct.exe`、`xsct.exe`、`cmd.exe` 或异常占用的 `hw_server.exe` 进程。
  - 如果确认没有正在使用的调试会话，可以结束旧 `rdi_xsct`，并重启 `hw_server` 后重试：

```powershell
Get-Process | Where-Object { $_.ProcessName -match 'rdi_xsct|xsct|hw_server|cmd' } |
  Select-Object ProcessName,Id,CPU,StartTime

# 仅在确认不是正在使用的调试会话后执行
Stop-Process -Id <旧rdi_xsct进程ID> -Force
Stop-Process -Id <hw_server进程ID> -Force
Start-Process -FilePath 'D:\software\vitis_2023_2\Vitis\2023.2\bin\hw_server.bat' -WindowStyle Hidden
```

  - 重启后先重新确认 JTAG 目标：

```powershell
& 'D:\software\vitis_2023_2\Vitis\2023.2\bin\program_flash.bat' -jtagtargets -url tcp:localhost:3121
```

  - 然后重新烧录。必要时加低速 JTAG 参数，例如 `-frequency 6000000`：

```powershell
& 'D:\software\vitis_2023_2\Vitis\2023.2\bin\program_flash.bat' `
  -f SG_cyclic_dma_system\Debug\sd_card\BOOT.BIN `
  -offset 0 `
  -fsbl plfm_SGDMA3\export\plfm_SGDMA3\sw\plfm_SGDMA3\boot\fsbl.elf `
  -flash_type qspi-x4-single `
  -verify `
  -target_id 2 `
  -url tcp:localhost:3121 `
  -frequency 6000000
```

  - 本工程实测：结束旧 `rdi_xsct`、重启 `hw_server` 后，Flash 可正常识别，随后 `Program Operation successful` / `Verify Operation successful` / `Flash Operation Successful`。