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

The Modbus port layer is split across several headers, each with a distinct configuration scope:

| Header | Configures | Porting role |
|--------|-----------|--------------|
| [`Inc/modbus/portserial.h`](Inc/modbus/portserial.h) | Transport mode (`COMMS_MODBUS_PORT`); UART defaults — baud rate, parity, stop bits, RTU/ASCII mode | Standard porting file; `COMMS_MODBUS_PORT` is a USB-specific addition |
| [`Inc/modbus/port_internal.h`](Inc/modbus/port_internal.h) | Hardware peripheral bindings: which LPTIM instance drives t3.5 timing (`MB_TIMER`), which USART instance is the Modbus serial port (`MB_SERIAL`), associated IRQ names and register macros | Standard porting — change only this file to migrate to a different USART or timer |
| [`Inc/modbus/port_addresses.h`](Inc/modbus/port_addresses.h) | Modbus slave address (`DEFAULT_SLAVE_ADDR`); holding and input register start addresses and counts; coil and discrete register placeholders | Application configuration — not hardware-specific |
| [`Inc/modbus/port.h`](Inc/modbus/port.h) | FreeModbus type aliases (`BOOL`, `UCHAR`, `USHORT`, etc.); critical section macros mapped to `__disable_irq` / `__enable_irq` | Standard FreeModbus porting layer — minimal changes from the reference port |
| [`Inc/modbus/portserial_usb.h`](Inc/modbus/portserial_usb.h) | USB CDC byte layer API (`portserial_usb_init`, `portserial_usb_flush_tx`); transport-mux API (`vMBPortSetUsbActive`, `vMBPortUsbInjectFrame`); RX stream buffer handle (`usbRxStream`) | Added for USB — no equivalent in a UART-only port |

#### Transport Selection

A compile-time define in `Inc/modbus/portserial.h` selects which physical port the Modbus slave listens on:

```c
#define COMMS_MODBUS_UART     0   // USART2 only
#define COMMS_MODBUS_USB      1   // USB CDC only
#define COMMS_MODBUS_DYNAMIC  2   // both; first frame claims the bus

#define COMMS_MODBUS_PORT     COMMS_MODBUS_DYNAMIC
```

In **DYNAMIC** mode both ports are active simultaneously.  A binary semaphore in `modbus_port_ownership.c` ensures only one transport processes frames at a time.  The first complete frame to arrive claims the bus.  A 5-second inactivity one-shot FreeRTOS timer releases the bus automatically so the other transport can take over.

### Tasks

| Task | Priority | Function |
|------|----------|----------|
| `modbus_task` | 5 | Runs the FreeModbus polling loop (`eMBPoll()`). Handles all register callbacks for both UART and USB frames. |
| `usb_task` | 4 | Waits for USB CDC frames, pre-validates them, and feeds them into the FreeModbus RTU state machine via the port layer mux. |
| Idle | 0 | FreeRTOS idle task (static allocation). |

### UART Modbus Path

```
USART2 RX interrupt
  → MB_SERIAL_IRQ_FUNC()  (port_internal.h)
  → pxMBFrameCBByteReceived()  (= xMBRTUReceiveFSM, mbrtu.c)
      reads byte via xMBPortSerialGetByte()  →  USART2->RDR
      restarts LPTIM1 t3.5 countdown via vMBPortTimersEnable()

LPTIM1 interrupt fires after t3.5 silence
  → pxMBPortCBTimerExpired()  (= xMBRTUTimerT35Expired)
  → posts EV_FRAME_RECEIVED to FreeModbus event queue

modbus_task / eMBPoll() wakes
  → eMBRTUReceive() validates CRC, extracts address + PDU
  → function-code handler (eMBRegHoldingCB / eMBRegInputCB in modbus_task.c)
      accesses shared registers via modbus_mem (mutex-protected)
  → eMBRTUSend() calls vMBPortSerialEnable(FALSE, TRUE)
      → enables USART2 TXE interrupt
      → byte-by-byte: USART2 TXE ISR → xMBRTUTransmitFSM → xMBPortSerialPutByte → USART2->TDR
  → vMBPortSerialEnable(TRUE, FALSE) restores RX mode
```

### USB Modbus Path

```
CDC_Receive_FS (USB ISR)
  → xStreamBufferSendFromISR(usbRxStream, ...)
  → re-arms USB OUT endpoint, yields to higher-priority task if woken

usb_task / modbus_usb_run() unblocks from xStreamBufferReceive
  → drains stream buffer with 2 ms inter-byte timeout (t3.5 equivalent)
  → pre-validates: length ≥ 8 bytes, address match, CRC == 0
  → claims port ownership (DYNAMIC mode) — discards if UART owns the bus
  → vMBPortSetUsbActive(true) — redirects FreeModbus byte I/O to USB
  → MB_SERIAL_DISABLE_RX_IRQ() — masks UART RX during injection
  → vMBPortUsbInjectFrame()
      for each byte: stage in usbPendingByte, call pxMBFrameCBByteReceived()
                     xMBRTUReceiveFSM reads usbPendingByte instead of USART2->RDR
                     vMBPortTimersEnable() suppressed (xMBPortIsUsbActive() guard)
      after last byte: pxMBPortCBTimerExpired() posts EV_FRAME_RECEIVED

modbus_task / eMBPoll() wakes (same path as UART)
  → function-code handler runs, accesses modbus_mem
  → eMBRTUSend() calls vMBPortSerialEnable(FALSE, TRUE)
      → USB TX path: drives xMBRTUTransmitFSM synchronously (no TXE interrupt)
                     xMBPortSerialPutByte accumulates bytes in USB TX buffer
                     portserial_usb_flush_tx() fires CDC_Transmit_FS in one shot
                     pxMBPortCBTimerExpired() completes FreeModbus TX cycle
  → vMBPortSerialEnable(TRUE, FALSE) clears bUsbActive, re-enables UART RX IRQ
```

### Shared Register Memory

Both transports share a single register bank owned by `modbus_mem.c`.  A FreeRTOS mutex protects the bank.  All access goes through `mb_mem_get_mutex()` / `mb_mem_release_mutex()` — the mutex is never exported directly.

| Register bank | Start address | Count | Notes |
|---------------|--------------|-------|-------|
| Holding | 1 | 100 | Read/write via FC03 / FC16 |
| Input | 1 | 100 | Read-only via FC04 |
| Coil | — | — | Not implemented |
| Discrete | — | — | Not implemented |

Addresses and counts are set in `Inc/modbus/port_addresses.h`.

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

- **UART** — USART2, 115200 baud 8N1.  Configurable via Modbus registers (future).
- **USB CDC** — virtual COM port via STM32L5 USB FS.  Baud rate setting ignored (USB ignores line coding for data routing).  Connect with any terminal: `screen /dev/ttyACM0 115200` on Linux, any COMxx port on Windows.

Both ports speak Modbus RTU.  In DYNAMIC mode the first master to send a frame claims the bus for 5 seconds of inactivity.

### Supported Modbus Function Codes

| FC | Name | Notes |
|----|------|-------|
| 03 | Read Holding Registers | |
| 04 | Read Input Registers | |
| 16 (0x10) | Write Multiple Registers | |

All other function codes return exception 01 (Illegal Function).

### LEDs

| LED | Behaviour |
|-----|-----------|
| RED | FreeModbus timer debug — set when LPTIM1 t3.5 timer is enabled, cleared when disabled (`MB_TIMER_DEBUG_RED == 1`) |
| BLUE | Toggled every watchdog pet cycle (`WD_DEBUG_BLUE == 1`) |
| GREEN | Set while USB owns the Modbus port (frame being processed), cleared on release (`USB_MODBUS_ACTIVE_DEBUG_GREEN == 1`) |

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
│   │   └── system.h                   Task stack sizes and priorities
│   ├── hw/
│   │   ├── dma.h                      DMA peripheral interface
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
│   │   ├── modbus_task.h         UART Modbus task entry point
│   │   ├── modbus_usb.h               USB Modbus adapter entry points
│   │   ├── port_addresses.h           Register start addresses, counts, slave address
│   │   ├── port.h                     FreeModbus port layer public interface
│   │   ├── port_internal.h            Hardware macros for USART2 and LPTIM1
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
│   │   └── system.c                   Task creation (conditional on COMMS_MODBUS_PORT)
│   ├── hw/
│   │   ├── dma.c                      DMA peripheral initialisation
│   │   ├── gpio.c                     GPIO peripheral initialisation
│   │   ├── icache.c                   Instruction cache initialisation
│   │   ├── lptim.c                    LPTIM one-shot timer (FreeModbus t3.5)
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
│   │   ├── modbus_mem.c               Shared holding/input register arrays + mutex
│   │   ├── modbus_port_ownership.c    Binary semaphore + one-shot timer for bus arbitration
│   │   ├── modbus_task.c         modbus_task: eMBInit/eMBPoll loop + register callbacks
│   │   ├── modbus_usb.c               usb_task helper: frame accumulation, pre-validation, injection
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

- Coil and discrete input registers not implemented (return exception 01).
- UART configuration (baud rate, parity) not yet Modbus-configurable — hardcoded in `modbus_task.c`.
- Holding register values not persisted to flash across power cycles.
- No DMA on USART2 — each byte goes through an ISR.  FreeModbus's byte-at-a-time model makes DMA integration non-trivial.
- CRC computation for the response in the USB path requires a second read of the register bank to calculate the CRC separately from the TX data accumulation path (this is handled by FreeModbus itself and is not a concern for normal operation).

---

## Complaints

STM32CubeMX is convenient for project scaffolding but its generated code structure conflicts with maintainability goals:

- Mixed naming conventions (camelCase, PascalCase, snake_case) from three different sources (HAL, FreeRTOS, application).
- Generated code structure is harder to maintain than hand-written equivalents — most generated files have been substantially rewritten.
- FreeModbus is old and designed for single-port bare-metal use.  It cannot be instantiated twice (all state is in module-level statics), which is why the USB port uses a transport mux into the single FreeModbus instance rather than a second stack instance.
- DMA integration with FreeModbus would require meaningful changes to the library.  Deferred.
