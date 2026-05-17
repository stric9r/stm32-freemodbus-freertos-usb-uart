# STM32L552 FreeModbus — FreeRTOS with USB/UART

## General

Project demonstrating:
- FreeRTOS — tasks, stream buffers, queues, semaphores, software timers
- FreeModbus — RTU slave, ported to FreeRTOS with custom port layer
- USB CDC — virtual COM port as a second Modbus port alongside UART
- USART as UART — hardware serial at 115200 8N1
- LPTIM — hardware one-shot timer for Modbus t3.5 inter-frame silence detection
- IWDG — hardware watchdog with per-task check-in

Target board: **NUCLEO-L552ZE-Q** (STM32L552ZET6Q, Cortex-M33, TrustZone not used).
Board chosen for availability — no special reason beyond that.

Project generated with STM32CubeMX, developed in VSCode and STM32CubeIDE.
Base example was the FreeRTOS Timers template, heavily modified.

> Admission — Claude Code was used to assist with documentation and some implementation.
> Proper prompting is key!  I'm not a fan of AI but its a tool and its going to be here
> regardless.  So I rather use it to assist and get stuff out faster but with quality.
>
> My mentality is to use AI, but don't trust.  Instead be skeptical and verify.
> Any AI usage was used mainly to help with quick documentation or nuance changes.
> Any larger changes are done with heavy planning in markdown files then implementation.

---

## Theory of Operation

### Project Build Configuration

#### Port Headers

The Modbus port layer headers fall into two distinct groups: files that a porter **must** touch when porting FreeModbus to new hardware, and files that are application-level and have no FreeModbus porting requirement.

##### FreeModbus port layer — must implement

These three headers are the mandatory FreeModbus porting surface. A UART-only port to different STM32 hardware requires changes only to these files and their associated `.c` files (`portevent.c`, `portserial.c`, `porttimer.c`).

| Header | What it defines | What to change when porting |
|--------|----------------|----------------------------|
| [`Inc/modbus/port.h`](Inc/modbus/port.h) | FreeModbus type aliases (`BOOL`, `UCHAR`, `USHORT`, etc.); critical-section macros (`ENTER_CRITICAL_SECTION` / `EXIT_CRITICAL_SECTION`) mapped to `__disable_irq` / `__enable_irq` | Map critical sections to your RTOS or bare-metal primitive. Type aliases rarely need changing on Cortex-M. |
| [`Inc/modbus/port_internal.h`](Inc/modbus/port_internal.h) | Hardware peripheral bindings: USART instance (`MB_SERIAL`, `MB_SERIAL_INSTANCE`), LPTIM instance (`MB_TIMER`, `MB_TIMER_INSTANCE`), IRQ vector names, direct-register byte I/O macros (`MB_SERIAL_PUT_BYTE`, `MB_SERIAL_GET_BYTE`), interrupt arm/mask macros, full ISR bodies (`MB_SERIAL_IRQ_FUNC`, `MB_TIMER_IRQ_FUNC`), coil GPIO port (`GPIO_COIL_PORT`, `GPIO_COIL_NPINS`) | **The primary migration target.** Change `MB_SERIAL_INSTANCE` and `MB_TIMER_INSTANCE` to your USART and timer. Update the IRQ names, register macros, and ISR bodies to match the new peripheral's register layout. |
| [`Inc/modbus/portserial.h`](Inc/modbus/portserial.h) | Compile-time transport selector (`COMMS_MODBUS_PORT`: UART=0, USB=1, DYNAMIC=2); compile-time UART defaults (`DEFAULT_SLAVE_ADDR`, `DEFAULT_BAUDERATE`, `DEFAULT_PARITY`, `DEFAULT_STOP_BITS`, `DEFAULT_MODE`) | Set `COMMS_MODBUS_PORT` to `COMMS_MODBUS_UART` for a UART-only port. Adjust defaults for your product's line parameters. |

##### USB transport addition — skip for UART-only

This header and its associated source files (`portserial_usb.c`, `modbus_usb.c`) exist solely to support the USB CDC second transport. A UART-only port does not need them.

| Header | What it defines |
|--------|----------------|
| [`Inc/modbus/portserial_usb.h`](Inc/modbus/portserial_usb.h) | USB CDC RX stream buffer handle (`usbRxStream`); low-level byte layer (`portserial_usb_init`, `portserial_usb_put_byte`, `portserial_usb_flush_tx`); transport-mux API (`vMBPortSetUsbActive`, `vMBPortUsbInjectFrame`, `xMBPortIsUsbActive`) |

##### Application layer — no FreeModbus porting requirement

These headers are application code that happens to live in `Inc/modbus/` for proximity to the port layer. They contain no FreeModbus porting obligations and could reasonably move to `Inc/app/`.

| Header | What it defines | Notes |
|--------|----------------|-------|
| [`Inc/modbus/port_addresses.h`](Inc/modbus/port_addresses.h) | Register start addresses and counts for all four register types; default slave address; `REG_DISCRETE_NGPIO` sentinel for the GPIO-backed discrete count | Edit to define your register map. `REG_COIL_NREGS` and the `coilPins[]` array in `modbus_task.c` must be updated together. |
| [`Inc/modbus/modbus_mem.h`](Inc/modbus/modbus_mem.h) | Shared register bank API (`mb_mem_get_holding`, `mb_mem_set_holding`, `mb_mem_get_input`); flash-persistent config type (`modbus_cfg_t`) and get/set API | Replace or rewrite for your register storage strategy. The mutex and fail-fast timeout pattern is specific to this project's FreeRTOS design. |
| [`Inc/modbus/modbus_port_ownership.h`](Inc/modbus/modbus_port_ownership.h) | Bus ownership arbitration API (`modbus_port_ownership_try_claim`, `modbus_port_ownership_refresh`, `modbus_port_ownership_release`) | Only exists to support DYNAMIC dual-transport. Delete entirely for a single-transport port. |

#### Transport Selection

A compile-time define in `Inc/modbus/portserial.h` selects which physical port the Modbus slave listens on:

```c
#define COMMS_MODBUS_UART     0   // USART2 only
#define COMMS_MODBUS_USB      1   // USB CDC only
#define COMMS_MODBUS_DYNAMIC  2   // both; first frame claims the bus

#define COMMS_MODBUS_PORT     COMMS_MODBUS_DYNAMIC
```

In **DYNAMIC** mode both ports are active simultaneously.  A binary semaphore in `modbus_port_ownership.c` ensures only one transport processes frames at a time.  The first complete frame to arrive claims the bus.  A 100 ms inactivity one-shot FreeRTOS timer releases the bus automatically so the other transport can take over.

### Tasks

| Task | Priority | Function |
|------|----------|----------|
| `modbus_task` | 5 | Runs the FreeModbus polling loop (`eMBPoll()`). Handles all register callbacks for both UART and USB frames. |
| `usb_task` | 4 | Waits for USB CDC frames, pre-validates them, and feeds them into the FreeModbus RTU state machine via the port layer mux. |
| `system_task` | 3 | Watchdog coordinator — pets the IWDG only after all registered tasks have checked in within the window. |
| `Idle` | 0 | FreeRTOS idle task (static allocation). |

### Shared Register Memory

Both transports share a single register bank owned by `modbus_mem.c`.  A FreeRTOS mutex protects items stored in flash internally.  Everything else is in RAM without protection.

| Register bank | Start address | Count | Notes |
|---------------|--------------|-------|-------|
| Holding | 1 | 100 | Read/write via FC03 / FC06 / FC16 |
| Input | 1 | 100 | Read-only via FC04 |
| Coil | 1 | 8 | Read/write via FC01 / FC05 / FC15 — drives PE0-PE7 |
| Discrete | 1 | 16 | Read-only via FC02 — 4 GPIO-backed, 12 always 0 |

Addresses and counts are set in `Inc/modbus/port_addresses.h`.

#### Coil Register Map

| Coil address | GPIO pin | Notes |
|-------------|----------|-------|
| 1 | PE0 | General-purpose output |
| 2 | PE1 | General-purpose output |
| 3 | PE2 | General-purpose output |
| 4 | PE3 | General-purpose output |
| 5 | PE4 | General-purpose output |
| 6 | PE5 | General-purpose output |
| 7 | PE6 | General-purpose output |
| 8 | PE7 | General-purpose output |

All pins are push-pull, low speed, initially driven low at boot.  Port E was chosen because all 16 of its pins are unused by any other peripheral on the NUCLEO-L552ZE-Q.

#### Discrete Input Register Map

| Discrete address | Source | Notes |
|-----------------|--------|-------|
| 1 | PC7 (GREEN LED) | Reflects current LED drive state via IDR |
| 2 | PA9 (RED LED) | Reflects current LED drive state via IDR |
| 3 | PB7 (BLUE LED) | Reflects current LED drive state via IDR |
| 4 | PC13 (BUTTON) | Raw pin level — active-low button reads 0 when pressed |
| 5-16 | — | Always 0; declared to satisfy masters that poll 16-bit blocks |

Push-pull output pins on the STM32L5 have their IDR tied to their ODR, so reading the IDR on an LED pin gives the current drive state without requiring a software shadow register.

#### Input and Holding Register Map

Both the holding and input banks are initialised at boot from the active flash config (or compile-time defaults if flash is erased). They share the same register layout.

| Offset | Modbus address | Name | Content |
|--------|---------------|------|---------|
| 0 | 1 | `SLAVE_ADDR` | Slave address (1–247) |
| 1 | 2 | `MODE` | `eMBMode`: 0 = RTU, 1 = ASCII |
| 2 | 3 | `BAUD_RATE_LO` | Baud rate bits [15:0] |
| 3 | 4 | `BAUD_RATE_HI` | Baud rate bits [31:16] |
| 4 | 5 | `PARITY` | `eMBParity`: 0 = none, 1 = odd, 2 = even |
| 5 | 6 | `DATA_BITS` | Data bits (informational — fixed at 8 internally) |
| 6 | 7 | `STOP_BITS` | Stop bits |
| 7 | 8 | `CRC` | CRC-16/Modbus of the stored flash config |
| 8 | 9 | `WRITE_FLAG` | Write trigger: set any non-zero value to persist registers 1–8 to `FLASH_MB` and reboot |

Writing holding registers 1–8 followed by a non-zero write to register 9 (`WRITE_FLAG`) persists the new configuration to flash and issues `NVIC_SystemReset()`. The device reboots using the new values.

### Flash Layout

The STM32L552ZET has 512 KB of internal flash (256 KB per bank, 2 KB pages).
It is split into two named regions in `STM32L552ZETXQ_FLASH.ld`:

| Region | Start | Size | Pages | Purpose |
|--------|-------|------|-------|---------|
| `FLASH_APP` | `0x08000000` | 504 KB | 0–251 | Application code, constants, and initialised-data load image |
| `FLASH_MB` | `0x0807E000` | 8 KB | 252–255 | Modbus persistent configuration (`modbus_cfg_t`) |

The application currently occupies approximately 76 KB of `FLASH_APP`, leaving ~428 KB
of headroom before reaching the `FLASH_MB` boundary.

#### Modbus Config — `FLASH_MB`

`FLASH_MB` holds a single 16-byte `modbus_cfg_t` struct at page 252
(bank 2, bank-relative page 124).  The struct is CRC-validated on every boot via
`mb_mem_get_config()`:

| Field | Size | Notes |
|-------|------|-------|
| `slaveAddr` | 1 byte | Modbus slave address (1–247) |
| `mode` | 1 byte | `eMBMode` — MB_RTU or MB_ASCII |
| `_pad[2]` | 2 bytes | Alignment padding |
| `baudRate` | 4 bytes | Baud rate in bits/s |
| `parity` | 1 byte | `eMBParity` — none / odd / even |
| `dataBits` | 1 byte | Informational — FreeModbus RTU hardcodes 8 internally |
| `stopBits` | 1 byte | Number of stop bits |
| `_pad2` | 1 byte | Alignment padding |
| `crc` | 2 bytes | CRC-16/Modbus over bytes 0–11 |
| `_reserved[2]` | 2 bytes | Pad to 16 bytes for doubleword flash writes |

If the CRC fails (erased flash, first boot, or corruption), `mb_mem_get_config()` returns
a pointer to a compile-time default struct populated from the `DEFAULT_*` macros in
`Inc/modbus/port_addresses.h`.  The task never sees a NULL pointer.

To write a new config at runtime call `mb_mem_set_config()`, which erases page 252 and
writes the struct as two 8-byte doublewords.

To expand `FLASH_MB` for additional persistent data: add fields to `modbus_cfg_t`,
increase `FLASH_MB LENGTH`, and decrease `FLASH_APP LENGTH` by the same amount, keeping
both values multiples of 2 KB (one flash page).

### Port Layer Mux

The FreeModbus port layer (`portserial.c`) contains a single `bUsbActive` flag.
When true, the three FreeModbus I/O functions behave as follows:

| Function | UART path (`bUsbActive == false`) | USB path (`bUsbActive == true`) |
|----------|----------------------------------|--------------------------------|
| `xMBPortSerialGetByte()` | reads `USART2->RDR` | returns `usbPendingByte` (staged by injection loop) |
| `xMBPortSerialPutByte()` | writes `USART2->TDR` | calls `portserial_usb_put_byte()` (accumulate) |
| `vMBPortSerialEnable(FALSE,TRUE)` | enables USART2 TXEIE | drives TX FSM synchronously, flushes CDC buffer |
| `vMBPortSerialEnable(TRUE,FALSE)` | enables USART2 RXNEIE | clears `bUsbActive`, re-enables USART2 RXNEIE |

`eMBPoll()` and all register callbacks are completely unaware of which transport delivered the frame.

---

## Features

### Communications

- **UART** — USART2, 115200 baud 8N1.  Configurable via Modbus registers.
- **USB CDC** — virtual COM port via STM32L5 USB FS.  Baud rate setting ignored (USB ignores line coding for data routing).

Both ports speak Modbus RTU by default, configurable via holding registers.  
In DYNAMIC mode the first task to send a frame claims the bus for 5 seconds of inactivity.

### Supported Modbus Function Codes

| FC | Name | Notes |
|----|------|-------|
| 01 | Read Coils | Coils 1-8 (PE0-PE7) |
| 02 | Read Discrete Inputs | Discretes 1-4 GPIO-backed; 5-16 always 0 |
| 03 | Read Holding Registers | |
| 04 | Read Input Registers | |
| 05 | Write Single Coil | Coils 1-8 (PE0-PE7) |
| 15 (0x0F) | Write Multiple Coils | Coils 1-8 (PE0-PE7) |
| 16 (0x10) | Write Multiple Registers | |

All other function codes return exception 01 (Illegal Function).

### LEDs

| LED | GPIO | Behaviour |
|-----|------|-----------|
| RED | PA9 | FreeModbus timer debug — set when LPTIM1 t3.5 timer is enabled, cleared when disabled (`MB_TIMER_DEBUG_RED == 1`) |
| BLUE | PB7 | Toggled every watchdog pet cycle (`WD_DEBUG_BLUE == 1`) |
| GREEN | PC7 | Set while USB owns the Modbus port (frame being processed), cleared on release (`USB_MODBUS_ACTIVE_DEBUG_GREEN == 1`) |

All three LED drive states are readable as discrete inputs 1-3 via FC02.

---

## Building

### 1. Clone and initialise submodules

```sh
git clone <repo-url>
cd stm32-freemodbus-freertos-usb-uart
git submodule update --init --recursive
```

### 2. Command-line build

Requires **GNU Tools for STM32 14.3.rel1** (or compatible `arm-none-eabi-gcc`) on `PATH`.

```sh
cd Debug
make -j4 all
```

| Output file | Description |
|-------------|-------------|
| `stm32-freertos-freemodbus-usb-uart.elf` | Flashable ELF image |
| `stm32-freertos-freemodbus-usb-uart.map` | Linker map |
| `stm32-freertos-freemodbus-usb-uart.list` | Disassembly listing |

```sh
make clean
```

### 3. STM32CubeIDE

Open the project: File → Open Projects from File System → select repo root.
The `.project` and `.cproject` files are committed.  Select the **Debug** build configuration and press `Ctrl+B`.

> `Generated_Project/` is a CubeMX reference only — do not import it.

---

## Project Structure

```
├── Docs/                              Datasheets and board schematics (PDFs)
│
├── Drivers/
│   ├── CMSIS/                         ARM CMSIS core + STM32L5 device headers
│   └── STM32L5xx_HAL_Driver/          STM32L5 HAL/LL driver source and headers
│
├── Generated_Project/                 Original STM32CubeMX reference project.
│                                      Do not edit or import.
│
├── Inc/
│   ├── app/
│   │   ├── FreeRTOSConfig.h           FreeRTOS kernel configuration
│   │   ├── modbus_task.h              Modbus task entry point
│   │   ├── modbus_usb.h               USB Modbus adapter entry points
│   │   ├── system.h                   Task stack sizes and priorities
│   │   ├── system_task.h              Watchdog coordinator API (register / check-in)
│   │   └── usb_task.h                 USB task entry point
│   ├── hw/
│   │   ├── crc.h                      CRC peripheral interface
│   │   ├── dma.h                      DMA peripheral interface
│   │   ├── flash.h                    Flash erase/write interface (FLASH_MB)
│   │   ├── gpio.h                     GPIO peripheral interface
│   │   ├── icache.h                   Instruction cache interface
│   │   ├── lptim.h                    LPTIM one-shot timer interface
│   │   ├── stm32l5xx_hal_conf.h       HAL module enable/disable configuration
│   │   ├── stm32l5xx_it.h             Interrupt handler declarations
│   │   ├── usart.h                    USART/UART peripheral interface
│   │   ├── usart_common.h             Shared USART types and constants
│   │   ├── usb_device.h               USB device init interface
│   │   ├── usbd_cdc_if.h              USB CDC class interface (CDC_Transmit_FS)
│   │   ├── usbd_conf.h                USB device stack hardware configuration
│   │   ├── usbd_desc.h                USB device descriptor strings
│   │   └── watchdog.h                 IWDG watchdog interface
│   ├── modbus/
│   │   ├── modbus_mem.h               Shared register bank API
│   │   ├── modbus_port_ownership.h    Port ownership arbitration API
│   │   ├── port_addresses.h           Register start addresses, counts, slave address
│   │   ├── port.h                     FreeModbus port layer public interface
│   │   ├── port_internal.h            Hardware macros for USART2, LPTIM1, coil GPIO
│   │   ├── portserial.h               Transport selection (COMMS_MODBUS_PORT)
│   │   └── portserial_usb.h           USB serial port API + transport mux API
│   └── main.h                         Top-level includes and pin definitions
│
├── Middlewares/Third_Party/
│   ├── freemodbus/                    FreeModbus stack (git submodule, unmodified)
│   └── FreeRTOS/                      FreeRTOS kernel (unmodified)
│
├── Src/
│   ├── app/
│   │   ├── modbus_task.c              modbus_task: eMBInit/eMBPoll loop + all register callbacks
│   │   ├── modbus_usb.c               usb_task helper: frame accumulation, pre-validation, injection
│   │   ├── system.c                   Task creation (conditional on COMMS_MODBUS_PORT)
│   │   ├── system_task.c              Watchdog coordinator task
│   │   └── usb_task.c                 USB CDC frame accumulation and Modbus injection task
│   ├── hw/
│   │   ├── crc.c                      CRC peripheral driver
│   │   ├── dma.c                      DMA peripheral initialisation
│   │   ├── flash.c                    Flash erase/write driver (used by modbus_mem for FLASH_MB)
│   │   ├── gpio.c                     GPIO peripheral initialisation (LEDs, button, coil outputs PE0-PE7)
│   │   ├── icache.c                   Instruction cache initialisation
│   │   ├── lptim.c                    LPTIM one-shot timer (FreeModbus t3.5)
│   │   ├── startup/
│   │   │   └── startup_stm32l552xx.s  Reset handler and vector table
│   │   ├── stm32l5xx_hal_msp.c        HAL MSP peripheral clock/pin callbacks
│   │   ├── stm32l5xx_hal_timebase_tim.c  HAL tick timebase (TIM6)
│   │   ├── stm32l5xx_it.c             Interrupt service routines
│   │   ├── system_stm32l5xx.c         SystemInit and clock configuration
│   │   ├── usart.c                    USART/UART driver (Modbus serial port)
│   │   ├── usb_device.c               USB device stack initialisation
│   │   ├── usbd_cdc_if.c              USB CDC callbacks — routes RX bytes to usbRxStream
│   │   ├── usbd_conf.c                USB device stack hardware configuration
│   │   ├── usbd_desc.c                USB device descriptor strings
│   │   └── watchdog.c                 IWDG hardware watchdog driver
│   ├── modbus/
│   │   ├── modbus_mem.c               Shared holding/input register arrays + mutex + flash config
│   │   ├── modbus_port_ownership.c    Binary semaphore + one-shot timer for bus arbitration
│   │   ├── portevent.c                FreeModbus port — event queue (FreeRTOS queue, context-aware)
│   │   ├── portserial.c               FreeModbus port — USART2 byte I/O + USB transport mux
│   │   ├── portserial_usb.c           USB CDC byte layer — stream buffer, TX accumulation buffer
│   │   └── porttimer.c                FreeModbus port — LPTIM1 t3.5 timer
│   ├── main.c                         Entry point: peripheral init, scheduler start
│   ├── syscalls.c                     Newlib OS hook stubs
│   └── sysmem.c                       Newlib heap stub
│
└── CLAUDE.md                          Coding standards for AI-assisted development
```

## Versions

| Tool / Library | Version |
|----------------|---------|
| STM32CubeMX | 6.17.0 |
| STM32CubeIDE | 2.1.1 |
| STM32Cube FW L5 | V1.5.0 |
| GNU Tools for STM32 | 14.3.rel1 |

---

## Known Limitations / TODOs

- No DMA on USART2 — each byte goes through an ISR.  FreeModbus's byte-at-a-time model makes DMA integration non-trivial.

---

## Design Decisions

### `extern` Usage Policy

#### The Rule

No raw `extern` declarations inside `.c` files. All cross-module variable sharing must go through a header. For HAL peripheral handles, the preferred pattern is a getter function rather than a raw `extern` variable.

#### Why Getters Over Raw `extern` — and Why It Matters More on an RTOS

On bare-metal, execution order is linear and deterministic. On FreeRTOS, the scheduler decides which task runs when. A raw `extern` handle shared across modules carries no information about ownership, initialization order, or which execution contexts are allowed to touch it. If two tasks both reference `extern USBD_HandleTypeDef hUsbDeviceFS` there is nothing at the call site that signals which task holds a lock or whether the handle is initialized yet.

A getter function (`USB_GetDeviceHandle()`) does not solve thread-safety on its own, but it:
- Gives you a single intercept point if you later need to add an assertion or a mutex check.
- Makes ownership explicit: the `.c` file that defines the handle and provides the getter is unambiguously the owner.
- Removes the handle type from the public interface of any module that does not need to know about it.

In this project the thread-safety risk is low — USB handle access is dominated by ISR context, and Modbus-level access is serialized by a mutex in `modbus_port_ownership.c`. The getters are policy, not a runtime fix.

#### Why Some Handles Live Entirely in Their Own File

For `hpcd_USB_FS` (`Src/hw/usbd_conf.c`) and `htim6` (`Src/hw/stm32l5xx_hal_timebase_tim.c`), the ISR handlers were moved into the same file that defines the handle. The handle never leaves its translation unit — no `extern` of any kind is needed.

#### Intentionally Left as `extern` (in Headers)

| Symbol | File | Reason |
|--------|------|--------|
| `USBD_Interface_fops_FS`, `CDC_Desc` | `Inc/hw/usbd_cdc_if.h`, `Inc/hw/usbd_desc.h` | Initialization-time constants passed once to middleware at startup. No mutation, no concurrent access, no encapsulation benefit from a getter. |
| `usbRxStream` | `Inc/modbus/portserial_usb.h` | FreeRTOS stream buffer written directly from an ISR (`CDC_Receive_FS`). A wrapper function would add indirection in ISR context with no benefit. Declared in a header (not a `.c` file), so CLAUDE.md is satisfied. |
| `SystemCoreClock` | `Inc/app/FreeRTOSConfig.h` | Defined by CMSIS/STM32 startup code. No application source file owns it; there is nothing to wrap. |
| Linker symbols (`_end`, `_estack`, `_Min_Stack_Size`) | `Src/sysmem.c` | Defined by the linker script. Only expressible as `extern`; no source file allocates them. |
| Weak stubs (`__io_putchar`, `__io_getchar`) | `Src/syscalls.c` | Newlib weak-symbol pattern. No source to wrap. |

#### What Was Not Changed

`Drivers/` (HAL + CMSIS) and `Middlewares/` (FreeRTOS, FreeModbus, USB Device Library) are vendor/third-party code and are never modified. The STM32 USB Device Library's callback interface (`USBD_CDC_ItfTypeDef`) requires the CDC layer (`usbd_cdc_if.c`) to hold a reference to the device handle in order to re-arm the RX endpoint after each receive — fully hiding `hUsbDeviceFS` from `usbd_cdc_if.c` would require changing that middleware interface. The getter (`USB_GetDeviceHandle()`) is the practical ceiling without touching vendor code.

---

---

## Release Notes

### Release 1.1

**Branch:** `bugfix/fix_assert_issue` — based on Release 1.0

#### Improvements

**Assert diagnostics**
- Failed assertions now capture the crash location (file, line, function, expression) into a reserved RAM region that survives a watchdog reset, enabling post-mortem diagnosis without a debugger.
- If a debugger is attached at the time of the fault, execution halts for live inspection before the device resets.

**Dual-transport stability (DYNAMIC mode)**
- Fixed three race conditions that allowed USB and UART to interfere with each other under simultaneous traffic, causing state machine corruption and unexpected resets.
- The port layer now correctly tracks the full UART request-response cycle — both the receive and transmit phases — and blocks the USB port from claiming the bus during either.

**Code cleanup**
- Removed legacy error-handling scaffolding superseded by the new assert diagnostics.
- Stripped unused newlib heap and syscall code; the build now produces zero warnings.

---

### Release 1.0

**Commit:** `b6853991`

Initial release — FreeModbus RTU slave on the **STM32L552ZET6Q** (Cortex-M33, NUCLEO-L552ZE-Q).

**Platform**
- STM32L552ZET6Q — Cortex-M33, 110 MHz, 512 KB flash, 256 KB RAM
- FreeRTOS V10.6.2
- FreeModbus RTU slave — custom FreeRTOS port layer; vendor source unmodified

**Communications**
- UART — Modbus RTU, configurable baud rate, parity, stop bits, and slave address; configuration persisted to flash
- USB CDC — virtual COM port as a second independent Modbus port sharing the same register bank
- Both ports active simultaneously in DYNAMIC mode; first complete frame claims the bus

**Supported function codes:** FC01, FC02, FC03, FC04, FC05, FC15, FC16

**Register map**
- 100 holding registers (read/write; offsets 0–8 are flash-persistent configuration)
- 100 input registers (read-only; mirrors configuration)
- 8 coil outputs (GPIO-backed)
- 16 discrete inputs (4 GPIO-backed: three LEDs + user button; 12 always 0)

**Other**
- Hardware watchdog with per-task check-in — never pet unless all tasks are healthy
- Hardware one-shot timer for Modbus inter-frame silence detection
- CRC-validated flash configuration with compile-time defaults on first boot

---

## Complaints

STM32CubeMX is convenient for project scaffolding but its generated code structure conflicts with maintainability goals:

- Mixed naming conventions (camelCase, PascalCase, snake_case) from three different sources (HAL, FreeRTOS, FreeModbus, application).
- Generated code structure is harder to maintain than hand-written equivalents — most generated files have been substantially rewritten.
- FreeModbus is old and designed for single-port bare-metal use.  It cannot be instantiated twice (all state is in module-level statics), which is why the USB port uses a transport mux into the single FreeModbus instance rather than a second stack instance.
- DMA integration with FreeModbus would require meaningful changes to the library.  Deferred.
