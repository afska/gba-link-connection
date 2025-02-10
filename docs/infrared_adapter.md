🌎 From: https://shonumi.github.io/dandocs.html#agb006 🌎

# AGB-006

- [General Hardware Information](#gbair_gen)
- [Device Detection](#gbair_dev)
- [IR Operation](#gbair_opr)
- [Comparison to GBC IR](#gbair_cmp)
- [Zoid Commands](#gbair_cmd)
- [Prototype IR Port](#gbair_pro)

## \[AGB-006\] : General Hardware Information

The AGB-006 is an accessory released in conjunction with Cyber Drive Zoids: Kiju no Senshi Hyuu on July 18, 2003. It serves as an infrared adapter, coming as a bundle with each game. Using IR signals, players can turn their GBAs into remote controls to pilot three toy model Zoids. Although the GBA removed the GBC's IR port, the AGB-006 officially restored that functionality as an add-on. Unfortunately Cyber Drive Zoids was the only game to take adavantage of the AGB-006.

*   The AGB-006 is a small attachment that fits into the GBA serial port providing IR functionalities
*   Has 2 IR diodes, one for receiving and one for transmitting
*   Very similar in size and shape to the connecting end of a DOL-011 (Gamecube-to-GBA cable)
*   Compatible with CDZ-01 Diablotiger, CDZ-02 Cyclops, and CDZ-EX Diablotiger B

## \[AGB-006\] : Device Detection

The AGB-006 can be detected in software by generating an interrupt request in General Purpose mode whenever sending an OFF/ON pulse. When no AGB-006 is plugged in, no interrupts are triggered, so checking Bit 7 of the Interrupt Request Flags is sufficient to verify whether or not the adapter is present. At a minimum, two writes are necessary. The first sets the SO line LOW (turning the IR signal off). The second sets the SO line HIGH (turning the IR signal on) and enables interrupts. The following RCNT values are sent by Cyber Drive Zoids to trigger an interrupt request:

```
0x80B2		//Turn IR light off
0x81BA		//Turn IR light on and enable Serial I/O interrupts
```

For this example, after the interrupt is generated, RCNT should hold the value 0x2004 at that time.

## \[AGB-006\] : IR Operation

The following values of RCNT can be used for various IR-related functions:

```
----------------------------------------
Value		| Usage
----------------------------------------
0x80BA		| Turns IR signal on
0x80B2		| Turns IR signal off
0x8190		| Receive IR signal
```

For turning the IR signal on, set the SO line HIGH. For turning the IR light off, set the SO line LOW.

For receiving IR signals, set SI's direction as input. An interrupt will be requested once the signal arrives. This interrupt is only generated when the IR sensor begins detecting light when it previously detected none. Essentially it comes when transitioning from a "no light" to "light" state. There doesn't appear to be a way to get an interrupt once the signal goes off. It should be noted that Cyberdrive Zoids, the only officially supported game for the AGB-006, is not programmed to receive IR signals. This feature was thus never used in commercial software.

Both sending and receiving signals can trigger an interrupt if Bit 8 of RCNT is set. For receiving IR signals, interrupts are the most effective method of detecting an incoming signal, as simply waiting for RCNT to change its read value is unreliable (unlike the device detection phase). Writing the value 0x8190 to RCNT, for example, instantly changes its read value to 0x8196, even when no AGB-006 is connected. For this reason, interrupts are all but required for properly receiving IR signals. Requesting interrupts while sending IR signals is optional but of limited use.

When receiving an IR signal, software should in theory measure the delay until the next IR signal to get the total ON/OFF pulse duration. Once RCNT is set, IR interrupts are continuously generated whenever a new IR signal is detected, as long as Bit 7 of the Interrupt Request Flags is cleared after each pulse. The so-called "signal fade" present in GBC IR hardware is not evident in the AGB-006, so the accessory will not automatically stop receiving an IR signal after a set amount of time. Software should set a cut-off threshold for pulse durations to decide when one side has stopped sending, especially as the AGB-006 does not have any apparent feedback to manually check.

## \[AGB-006\] : Comparison to GBC IR

The biggest difference between the AGB-006 and the GBC's native IR hardware are interrupts. While prototype GBAs originally were designed to have IR ports with their own associated interrupts, the AGB-006 repurposes the existing serial interrupt. Interrupts potentially simplify the task of detecting IR light. Depending on the communication protocol, however, actually servicing an interrupt may be too slow for rapid data transfer. In such cases, a tight loop that merely continuously reads the Interrupt Request Flags may be more effecient. The AGB-006's interrupts excel in convenience over the GBC when detecting the next IR signal, as they eliminate the need to check any OFF status of the IR signal. The only thing a program needs to do is maintain a maximum allowed duration for pulses to see when communications should stop (something GBC software does anyway to check for errors mid-transmission).

Physically speaking, the AGB-006 has a far wider range of reception. It is capable of receiving signals at a maximum distance roughly between 1.7 and 1.8 meters. Furthermore, it can receive signals approximately within 150 degrees horizontally, and approximately 165 degrees vertically (IR light is only undetectable from very extreme angles beneath the unit). This stands in contrast to the GBC, which mostly needs line-of-sight to communicate via IR, or even detect IR sources.

Finally, while the GBC IR hardware is receptive to incandescent light sources, the AGB-006 largely ignores them.

## \[AGB-006\] : Zoid Commands

Officially, the AGB-006 is exclusively used to control motorized minature models from the Zoids franchise. This communication is always one-way, with the GBA blasting IR signals repeatedly once a button is pressed. The protocol is fairly simple. A number of IR pulses (total ON and OFF time) are sent with only a few specific lengths. Their exact timing seems to vary and may not be entirely consistent, so relative duration must be used. The ON phase of the pulse accounts for a very miniscule amount time, while the OFF phase accounts for the majority of the delay.

```
Mini Pulse		Smallest pulse. Very short and very numerous
Short Pulse		Roughly 16x the length of a Mini Pulse
Medium Pulse		Roughly 40x the length of a Mini Pulse
Long Pulse		The longest pulse. Easily around 6000x the length of a Mini Pulse
```

Mini Pulses appear to last anywhere from 18.1us to 20us. There are dozens of them in between every other type of pulse, and they seem to act as a sort of "keep-alive" for IR transmission. Below are the specific transfers used to manipulate the Zoids. There are two separate "channels" for ID1 and ID2. The goal was to have two players compete against each other, so two distinct versions of each command exist to avoid interference.

When omitting Mini Pulses, each IR command is 12 Short or Medium Pulses followed by 1 Long Pulse. The order of Short and Medium Pulses determines the exact command, while the Long Pulse functions as a stop signal. Each command can be converted into binary by treating a Medium Pulse as "1", and a Short Pulse as a "0". Commands can then be parsed by examining every 4-bit segment. Below describes how each segment affects the command. Note that the values use the first pulse as the MSB and the last pulse as the LSB.

```
-----------------------------------------------------------
Bits 8-11 - Channel + Action Category
-----------------------------------------------------------
0x8		Perform "Main Actions" for ID1
0x9		Perform "Misc. Actions" for ID1
0xA		Perform "Main Actions" for ID2
0xB		Perform "Misc. Actions" for ID2
-----------------------------------------------------------
```

Bits 8-11 determine which channel the command belongs to, as well as the overall category of action the CDZ model should perform. Main Actions describe things such as moving forward/backward, firing the cannon, and turning. Misc. Actions describe a variety of things such as syncing/initializing the GBA and CDZ model, roaring, and retreating. It is also used for some actions during the "boost" mode. A number of these actions are not listed in the game's manual.

```
-----------------------------------------------------------
Bits 4-7 - Motion Type
-----------------------------------------------------------

For Main Actions

0x2		Boost Backward
0x3		Boost Backward
0x4		Backward Speed 2
0x6		Backward Speed 1
0x8		Backward Speed 0
0xA		Forward Speed 0
0xC		Forward Speed 1
0xE		Forward Speed 2

For Misc. Actions

0x0		Boost Forward
0x1		Boost Forward + Fire (optionally)
0x3		Sync Signal/Hidden Moves
0x5		Hidden Moves
-----------------------------------------------------------
```

The Cyber Drive Zoids game can use the save data from the single-player mode to enhance the actions performed by the CDZ model, chiefly in regards to speed, HP, and the ability to boost. Once sufficient progress is made in the single-player mode, the CDZ model can shift gears up and down to alter speed. The regular IR controller that comes packaged by default with CDZ models does not offer these advantages. The sync signal is used on startup to activate the CDZ model for a match.

```
-----------------------------------------------------------
Bits 0-3 - Action Type
-----------------------------------------------------------

For Main Actions

0x0		Walk OR Fire if Motion Type == 1 or 3
0x1		Walk OR Fire if Motion Type == 1 or 3
0x2		Jump (typically for Boost Mode)
0x3		Jump (typically for Boost Mode)
0x4		Jump
0x5		Jump
0x6		Jump
0x7		Jump
0x8		Jump
0x9		Jump
0xA		Fire
0xB		Fire
0xC		Fire
0xD		Fire

For Misc. Actions

0x0		If Motion Type == 1 -> Fire
0x1		If Motion Type == 1 -> Fire

0x2		If Motion Type == 0 -> Jump 
		If Motion Type == 3 -> Sync ID1, Boost Level 0 
		If Motion Type == 5 -> Intimidate


0x3		If Motion Type == 0 -> Jump
		If Motion Type == 3 -> Sync ID2, Boost Level 0
		If Motion Type == 5 -> Intimidate		

0x4		If Motion Type == 3 -> Sync ID1, Boost Level 1 
		If Motion Type == 5 -> Swing Hips	


0x5		If Motion Type == 3 -> Sync ID2, Boost Level 1 
		If Motion Type == 5 -> Swing Hips

0xA		War Cry
0xB		War Cry
0xE		Escape
0xF		Escape
-----------------------------------------------------------
```

For Main Actions, if the Motion Type is not one of the above specified values (e.g. a 0), and the Action Type is jumping or firing, then the Zoid will move not forward or backward. Instead, it will remain stationary while performing that action. In this same manner, it's possible to simultaneously combine jumping or firing with moving by properly setting the Motion Type. Walking should always specify one of the listed Motion Types, however.

The CDZ model has a boost mode where the toy is temporarily invulnerable to damage and shifts into the highest gear. This mode lasts for 10 seconds. The sync signals specify how many boosts are available per match for that CDZ model. The boost mode commands are spread across Main Actions and Misc. Operations such as firing and jumping are often conditional, requiring the Motion Type to be Boost Forward or Boost Backward.

## \[AGB-006\] : Prototype IR Port

Cyber Drive Zoids appears to frequently read from the memory location 0x4000136 which was supposed to have been the MMIO register for the IR port on prototype GBAs. The AGB-006 does not cause the 16-bit value of that location to change at all, however, and all reads do in fact return zero. The purpose of that bit of code is currently unclear. It may be a remnant of earlier code long made before the AGB-006 was fully realized.
