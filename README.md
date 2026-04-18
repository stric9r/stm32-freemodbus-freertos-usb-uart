# STM32L552 FreeModbus - FreeRTOS with USB/UART

Project still very much WIP.

## General
Project primarily used to demonstrate knowledge of
- FreeRTOS
- FreeModbus
- USB
- Potentially DMA - May have to make changes to FreeModbus to allow this.
- USART as UART
- Timers
- General firmware development

This is project on a NUCLEO-L552ZE-Q board. 
It's what I have on hand, and used to learn STM32 development.
It was chosen for no special reason except that its what I had. 

Project generated with STMCubeMX and edited in STM32CubeIDE and VSCode.
The example project was the FreeRTOS Timers example.  
This was done to get a project with a port of FreeRTOS already there.
It has been heavily modified.

Personally I'm a fan of VSCode but STMCubeIDE is useful for project setup and a 
quick build environment.

VSCode primarily used to do code development.

## Features
This will be a Modbus Slave device that supports RTU and ASCII serial connections.

### Communications
It will allow communications of USB (virtual com port) and a USART configured as a UART.  
Pins arbiraritly chosen based on dev board ease of access.

UART configuration defaults to 115200 baud, 8 data bits, 1 stop bit, no parity.
This (future) will be configuralbe in the Modbus registers.

### LEDs
There are 3 LED's on the board:
- RED   - FreeModbus debug for timer enabled
- BLUE  - TBD
- GREEN - TBD

## Complaints 
STM32CubeMX is very helpful for generating a project on the fly but the naming conventions and how some of this code is structured is frustrating.  That coupled with FreeRTOS and my own personal development rules (hammered in my by a coding standard) causes:

- Different case developement - I'm a fan of snake_case, but I see a mix of camelCase, and some other ways . I've tried to conform but this is just part of develping with 3rd party libs.

- Issues with how code is generated.  MX is very helpful but I've found myself improving their output to something more maintainable, reusable, and readable.  

- FreeModbus is old, and not really structured for modern MCU's.  But its still popular.  I may do a port of nanoModbus in the future.  All that to say, I don't know if I can make DMA work well here, or if I'd have to make substantial changes to FreeModbus to allow it.  That's all TBD.

- I'm sure there are more but those are the three biggest ones.

