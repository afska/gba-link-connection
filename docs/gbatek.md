🌎 From: http://problemkaputt.de/gbatek.htm 🌎

# GBA Communication Ports

The GBAs Serial Port may be used in various different communication modes. Normal mode may exchange data between two GBAs (or to transfer data from master GBA to several slave GBAs in one-way direction).  
Multi-player mode may exchange data between up to four GBAs. UART mode works much like a RS232 interface. JOY Bus mode uses a standardized Nintendo protocol. And General Purpose mode allows to mis-use the 'serial' port as bi-directional 4bit parallel port.  
Note: The Nintendo DS does not include a Serial Port.

- SIO Normal Mode
- SIO Multi-Player Mode
- SIO General-Purpose Mode
- SIO Control Registers Summary
- GBA Wireless Adapter

# SIO Normal Mode

This mode is used to communicate between two units.  
Transfer rates of 256Kbit/s or 2Mbit/s can be selected, however, the fast 2Mbit/s is intended ONLY for special hardware expansions that are DIRECTLY connected to the GBA link port (ie. without a cable being located between the GBA and expansion hardware). In normal cases, always use 256Kbit/s transfer rate which provides stable results.  
Transfer lengths of 8bit or 32bit may be used, the 8bit mode is the same as for older DMG/CGB gameboys, however, the voltages for "GBA cartridges in GBAs" are different as for "DMG/CGB cartridges in DMG/CGB/GBAs", ie. it is not possible to communicate between DMG/CGB games and GBA games.

**4000134h - RCNT (R) - Mode Selection, in Normal/Multiplayer/UART modes (R/W)**

```
  Bit   Expl.
  0-3   Undocumented (current SC,SD,SI,SO state, as for General Purpose mode)
  4-8   Not used     (Should be 0, bits are read/write-able though)
  9-13  Not used     (Always 0, read only)
  14    Not used     (Should be 0, bit is read/write-able though)
  15    Must be zero (0) for Normal/Multiplayer/UART modes
```

**4000128h - SIOCNT - SIO Control, usage in NORMAL Mode (R/W)**

```
  Bit   Expl.
  0     Shift Clock (SC)        (0=External, 1=Internal)
  1     Internal Shift Clock    (0=256KHz, 1=2MHz)
  2     SI State (opponents SO) (0=Low, 1=High/None) --- (Read Only)
  3     SO during inactivity    (0=Low, 1=High) (applied ONLY when Bit7=0)
  4-6   Not used                (Read only, always 0 ?)
  7     Start Bit               (0=Inactive/Ready, 1=Start/Active)
  8-11  Not used                (R/W, should be 0)
  12    Transfer Length         (0=8bit, 1=32bit)
  13    Must be "0" for Normal Mode
  14    IRQ Enable              (0=Disable, 1=Want IRQ upon completion)
  15    Not used                (Read only, always 0)
```

The Start bit is automatically reset when the transfer completes, ie. when all 8 or 32 bits are transferred, at that time an IRQ may be generated.

**400012Ah - SIODATA8 - SIO Normal Communication 8bit Data (R/W)**

For 8bit normal mode. Contains 8bit data (only lower 8bit are used). Outgoing data should be written to this register before starting the transfer. During transfer, transmitted bits are shifted-out (MSB first), and received bits are shifted-in simultaneously. Upon transfer completion, the register contains the received 8bit value.

**4000120h - SIODATA32_L - SIO Normal Communication lower 16bit data (R/W)**  
**4000122h - SIODATA32_H - SIO Normal Communication upper 16bit data (R/W)**

Same as above SIODATA8, for 32bit normal transfer mode respectively.  
SIOCNT/RCNT must be set to 32bit normal mode <before> writing to SIODATA32.

**Initialization**

First, initialize RCNT register. Second, set mode/clock bits in SIOCNT with startbit cleared. For master: select internal clock, and (in most cases) specify 256KHz as transfer rate. For slave: select external clock, the local transfer rate selection is then ignored, as the transfer rate is supplied by the remote GBA (or other computer, which might supply custom transfer rates).  
Third, set the startbit in SIOCNT with mode/clock bits unchanged.

**Recommended Communication Procedure for SLAVE unit (external clock)**  
\- Initialize data which is to be sent to master.  
\- Set Start flag.  
\- Set SO to LOW to indicate that master may start now.  
\- Wait for IRQ (or for Start bit to become zero). (Check timeout here!)  
\- Set SO to HIGH to indicate that we are not ready.  
\- Process received data.  
\- Repeat procedure if more data is to be transferred.  
(or is so=high done automatically? would be fine - more stable - otherwise master may still need delay)

**Recommended Communication Procedure for SLAVE unit (external clock)**  
\- Initialize data which is to be sent to master.  
\- Set Start=0 and SO=0 (SO=LOW indicates that slave is (almost) ready).  
\- Set Start=1 and SO=1 (SO=HIGH indicates not ready, applied after transfer).

> (Expl. Old SO=LOW kept output until 1st clock bit received).

> (Expl. New SO=HIGH is automatically output at transfer completion).

\- Set SO to LOW to indicate that master may start now.  
\- Wait for IRQ (or for Start bit to become zero). (Check timeout here!)  
\- Process received data.  
\- Repeat procedure if more data is to be transferred.

**Recommended Communication Procedure for MASTER unit (internal clock)**  
\- Initialize data which is to be sent to slave.  
\- Wait for SI to become LOW (slave ready). (Check timeout here!)  
\- Set Start flag.  
\- Wait for IRQ (or for Start bit to become zero).  
\- Process received data.  
\- Repeat procedure if more data is to be transferred.

**Cable Protocol**  
During inactive transfer, the shift clock (SC) is high. The transmit (SO) and receive (SI) data lines may be manually controlled as described above.  
When master sends SC=LOW, each master and slave must output the next outgoing data bit to SO. When master sends SC=HIGH, each master and slave must read out the opponents data bit from SI. This is repeated for each of the 8 or 32 bits, and when completed SC will be kept high again.

**Transfer Rates**  
Either 256KHz or 2MHz rates can be selected for SC, so max 32KBytes (256Kbit) or 128KBytes (2Mbit) can be transferred per second. However, the software must process each 8bit or 32bit of transmitted data separately, so the actual transfer rate will be reduced by the time spent on handling each data unit.  
Only 256KHz provides stable results in most cases (such like when linking between two GBAs). The 2MHz rate is intended for special expansion hardware (with very short wires) only.

**Using Normal mode for One-Way Multiplayer communication**  
When using normal mode with multiplay-cables, data isn't exchanged between first and second GBA as usually. Instead, data is shifted from first to last GBA (the first GBA receives zero, because master SI is shortcut to GND).  
This behaviour may be used for fast ONE-WAY data transfer from master to all other GBAs. For example (3 GBAs linked):

```
  Step         Sender      1st Recipient   2nd Recipient
  Transfer 1:  DATA #0 --> UNDEF      -->  UNDEF     -->
  Transfer 2:  DATA #1 --> DATA #0    -->  UNDEF     -->
  Transfer 3:  DATA #2 --> DATA #1    -->  DATA #0   -->
  Transfer 4:  DATA #3 --> DATA #2    -->  DATA #1   -->
```

The recipients should not output any own data, instead they should forward the previously received data to the next recipient during next transfer (just keep the incoming data unmodified in the data register).  
Due to the delayed forwarding, 2nd recipient should ignore the first incoming data. After the last transfer, the sender must send one (or more) dummy data unit(s), so that the last data is forwarded to the 2nd (or further) recipient(s).

# SIO Multi-Player Mode

Multi-Player mode can be used to communicate between up to 4 units.

**4000134h - RCNT (R) - Mode Selection, in Normal/Multiplayer/UART modes (R/W)**

```
  Bit   Expl.
  0-3   Undocumented (current SC,SD,SI,SO state, as for General Purpose mode)
  4-8   Not used     (Should be 0, bits are read/write-able though)
  9-13  Not used     (Always 0, read only)
  14    Not used     (Should be 0, bit is read/write-able though)
  15    Must be zero (0) for Normal/Multiplayer/UART modes
```

Note: Even though undocumented, many Nintendo games are using Bit 0 to test current SC state in multiplay mode.

**4000128h - SIOCNT - SIO Control, usage in MULTI-PLAYER Mode (R/W)**

```
  Bit   Expl.
  0-1   Baud Rate     (0-3: 9600,38400,57600,115200 bps)
  2     SI-Terminal   (0=Parent, 1=Child)                  (Read Only)
  3     SD-Terminal   (0=Bad connection, 1=All GBAs Ready) (Read Only)
  4-5   Multi-Player ID     (0=Parent, 1-3=1st-3rd child)  (Read Only)
  6     Multi-Player Error  (0=Normal, 1=Error)            (Read Only)
  7     Start/Busy Bit      (0=Inactive, 1=Start/Busy) (Read Only for Slaves)
  8-11  Not used            (R/W, should be 0)
  12    Must be "0" for Multi-Player mode
  13    Must be "1" for Multi-Player mode
  14    IRQ Enable          (0=Disable, 1=Want IRQ upon completion)
  15    Not used            (Read only, always 0)
```

The ID Bits are undefined until the first transfer has completed.

**400012Ah - SIOMLT_SEND - Data Send Register (R/W)**  
Outgoing data (16 bit) which is to be sent to the other GBAs.

**4000120h - SIOMULTI0 - SIO Multi-Player Data 0 (Parent) (R/W)**  
**4000122h - SIOMULTI1 - SIO Multi-Player Data 1 (1st child) (R/W)**  
**4000124h - SIOMULTI2 - SIO Multi-Player Data 2 (2nd child) (R/W)**  
**4000126h - SIOMULTI3 - SIO Multi-Player Data 3 (3rd child) (R/W)**  
These registers are automatically reset to FFFFh upon transfer start.  
After transfer, these registers contain incoming data (16bit each) from all remote GBAs (if any / otherwise still FFFFh), as well as the local outgoing SIOMLT_SEND data.  
Ie. after the transfer, all connected GBAs will contain the same values in their SIOMULTI0-3 registers.

**Initialization**  
\- Initialize RCNT Bit 14-15 and SIOCNT Bit 12-13 to select Multi-Player mode.  
\- Read SIOCNT Bit 3 to verify that all GBAs are in Multi-Player mode.  
\- Read SIOCNT Bit 2 to detect whether this is the Parent/Master unit.

**Recommended Transmission Procedure**  
\- Write outgoing data to SIODATA_SEND.  
\- Master must set Start bit.  
\- All units must process received data in SIOMULTI0-3 when transfer completed.  
\- After the first successful transfer, ID Bits in SIOCNT are valid.  
\- If more data is to be transferred, repeat procedure.

The parent unit blindly sends data regardless of whether childs have already processed old data/supplied new data. So, parent unit might be required to insert delays between each transfer, and/or perform error checking.  
Also, slave units may signalize that they are not ready by temporarily switching into another communication mode (which does not output SD High, as Multi-Player mode does during inactivity).

**Transfer Protocol**  
Beginning  
\- The masters SI pin is always LOW.  
\- When all GBAs are in Multiplayer mode (ready) SD is HIGH.  
\- When master starts the transfer, it sets SC=LOW, slaves receive Busy bit.  
Step A  
\- ID Bits in master unit are set to 0.  
\- Master outputs Startbit (LOW), 16bit Data, Stopbit (HIGH) through SD.  
\- This data is written to SIOMULTI0 of all GBAs (including master).  
\- Master forwards LOW from its SO to 1st childs SI.  
\- Transfer ends if next child does not output data after certain time.  
Step B  
\- ID Bits in 1st child unit are set to 1.  
\- 1st Child outputs Startbit (LOW), 16bit Data, Stopbit (HIGH) through SD.  
\- This data is written to SIOMULTI1 of all GBAs (including 1st child).  
\- 1st child forwards LOW from its SO to 2nd childs SI.  
\- Transfer ends if next child does not output data after certain time.  
Step C  
\- ID Bits in 2nd child unit are set to 2.  
\- 2nd Child outputs Startbit (LOW), 16bit Data, Stopbit (HIGH) through SD.  
\- This data is written to SIOMULTI2 of all GBAs (including 2nd child).  
\- 2nd child forwards LOW from its SO to 3rd childs SI.  
\- Transfer ends if next child does not output data after certain time.  
Step D  
\- ID Bits in 3rd child unit are set to 3.  
\- 3rd Child outputs Startbit (LOW), 16bit Data, Stopbit (HIGH) through SD.  
\- This data is written to SIOMULTI3 of all GBAs (including 3rd child).  
\- Transfer ends (this was the last child).  
Transfer end  
\- Master sets SC=HIGH, all GBAs set SO=HIGH.  
\- The Start/Busy bits of all GBAs are automatically cleared.  
\- Interrupts are requested in all GBAs (as far as enabled).

**Error Bit**  
This bit is set when a slave did not receive SI=LOW even though SC=LOW signalized a transfer (this might happen when connecting more than 4 GBAs, or when the previous child is not connected). Also, the bit is set when a Stopbit wasn't HIGH.  
The error bit may be undefined during active transfer - read only after transfer completion (the transfer continues and completes as normal even if errors have occurred for some or all GBAs).  
Don't know: The bit is automatically reset/initialized with each transfer, or must be manually reset?

**Transmission Time**  
The transmission time depends on the selected Baud rate. And on the amount of Bits (16 data bits plus start/stop bits for each GBA), delays between data for each GBA, plus final timeout (if less than 4 GBAs). That is, depending on the number of connected GBAs:

```
  GBAs    Bits    Delays   Timeout
  1       18      None     Yes
  2       36      1        Yes
  3       54      2        Yes
  4       72      3        None
```

(The average Delay and Timeout periods are unknown?)  
Above is not counting the additional CPU time that must be spent on initiating and processing each transfer.

**Fast One-Way Transmission**  
Beside for the actual SIO Multiplayer mode, you can also use SIO Normal mode for fast one-way data transfer from Master unit to all Child unit(s). See chapter about SIO Normal mode for details.

# SIO General-Purpose Mode

In this mode, the SIO is 'misused' as a 4bit bi-directional parallel port, each of the SI,SO,SC,SD pins may be directly controlled, each can be separately declared as input (with internal pull-up) or as output signal.

**4000134h - RCNT (R) - SIO Mode, usage in GENERAL-PURPOSE Mode (R/W)**  
Interrupts can be requested when SI changes from HIGH to LOW, as General Purpose mode does not require a serial shift clock, this interrupt may be produced even when the GBA is in Stop (low power standby) state.

```
  Bit   Expl.
  0     SC Data Bit         (0=Low, 1=High)
  1     SD Data Bit         (0=Low, 1=High)
  2     SI Data Bit         (0=Low, 1=High)
  3     SO Data Bit         (0=Low, 1=High)
  4     SC Direction        (0=Input, 1=Output)
  5     SD Direction        (0=Input, 1=Output)
  6     SI Direction        (0=Input, 1=Output, but see below)
  7     SO Direction        (0=Input, 1=Output)
  8     SI Interrupt Enable (0=Disable, 1=Enable)
  9-13  Not used
  14    Must be "0" for General-Purpose Mode
  15    Must be "1" for General-Purpose or JOYBUS Mode
```

SI should be always used as Input to avoid problems with other hardware which does not expect data to be output there.

**4000128h - SIOCNT - SIO Control, not used in GENERAL-PURPOSE Mode**  
This register is not used in general purpose mode. That is, the separate bits of SIOCNT still exist and are read- and/or write-able in the same manner as for Normal, Multiplay, or UART mode (depending on SIOCNT Bit 12,13), but are having no effect on data being output to the link port.

# SIO Control Registers Summary

**Mode Selection (by RCNT.15-14 and SIOCNT.13-12)**

```
  R.15 R.14 S.13 S.12 Mode
  0    x    0    0    Normal 8bit
  0    x    0    1    Normal 32bit
  0    x    1    0    Multiplay 16bit
  0    x    1    1    UART (RS232)
  1    0    x    x    General Purpose
  1    1    x    x    JOY BUS
```

**SIOCNT**

```
  Bit    0      1    2     3      4 5 6   7     8    9      10   11
  Normal Master Rate SI/In SO/Out - - -   Start -    -      -    -
  Multi  Baud   Baud SI/In SD/In  ID# Err Start -    -      -    -
  UART   Baud   Baud CTS   Parity S R Err Bits  FIFO Parity Send Recv
```

# GBA Wireless Adapter

**GBA Wireless Adapter (AGB-015 or OXY-004)**

- GBA Wireless Adapter Games
- GBA Wireless Adapter Login
- GBA Wireless Adapter Commands
- GBA Wireless Adapter Component Lists

## GBA Wireless Adapter Games

**GBA Wireless Adapter compatible Games**

```
  bit Generations series (Japan only)
  Boktai 2: Solar Boy Django (Konami)
  Boktai 3: Sabata's Counterattack
  Classic NES Series: Donkey Kong
  Classic NES Series: Dr. Mario
  Classic NES Series: Ice Climber
  Classic NES Series: Pac-Man
  Classic NES Series: Super Mario Bros.
  Classic NES Series: Xevious
  Digimon Racing (Bandai) (No Wireless Adapter support in European release)
  Dragon Ball Z: Buu's Fury (Atari)
  Famicom Mini Series: #13 Balloon Fight
  Famicom Mini Series: #12 Clu Clu Land
  Famicom Mini Series: #16 Dig Dug
  Famicom Mini Series: #02 Donkey Kong
  Famicom Mini Series: #15 Dr. Mario
  Famicom Mini Series: #03 Ice Climber
  Famicom Mini Series: #18 Makaimura
  Famicom Mini Series: #08 Mappy
  Famicom Mini Series: #11 Mario Bros.
  Famicom Mini Series: #06 Pac-Man
  Famicom Mini Series: #30 SD Gundam World Scramble Wars
  Famicom Mini Series: #01 Super Mario Bros.
  Famicom Mini Series: #21 Super Mario Bros.
  Famicom Mini Series: #19 Twin Bee
  Famicom Mini Series: #14 Wrecking Crew
  Famicom Mini Series: #07 Xevious
  Hamtaro: Ham-Ham Games (Nintendo)
  Lord of the Rings: The Third Age, The (EA Games)
  Mario Golf: Advance Tour (Nintendo)
  Mario Tennis: Power Tour (Nintendo)
  Mega Man Battle Network 5: Team Protoman (Capcom)
  Mega Man Battle Network 5: Team Colonel (Capcom)
  Mega Man Battle Network 6: Cybeast Falzar
  Mega Man Battle Network 6: Cybeast Gregar
  Momotaro Dentetsu G: Make a Gold Deck! (Japan only)
  Pokemon Emerald (Nintendo)
  Pokemon FireRed (Nintendo)
  Pokemon LeafGreen (Nintendo)
  Sennen Kazoku (Japan only)
  Shrek SuperSlam
  Sonic Advance 3
```

## GBA Wireless Adapter Login

**GBA Wireless Adapter Login**

```
  rcnt=8000h    ;\\
  rcnt=80A0h    ;
  rcnt=80A2h    ; reset adapter or so
  wait          ;
  rcnt=80A0h    ;/
  siocnt=5003h  ;\\set 32bit normal mode, 2MHz internal clock
  rcnt=0000h    ;/
  passes=0, index=0
 @@lop:
  passes=passes+1, if passes>32 then ERROR  ;give up (usually only 10 passses)
  recv.lo=siodata AND FFFFh    ;response from adapter
  recv.hi=siodata/10000h       ;adapter's own "NI" data
  if send.hi<>recv.lo then index=0, goto @@stuck  ;<-- fallback to index=0
  if (send.lo XOR FFFFh)<>recv.lo then goto @@stuck
  if (send.hi XOR FFFFh)<>recv.hi then goto @@stuck
  index=index+1
 @@stuck:
  send.lo=halfword\[@@key\_string+index\*2\]
  send.hi=recv.hi XOR FFFFh
  siodata=send.lo+(send.hi\*10000h)
  siocnt.bit7=1                        ;<-- start transmission
  if index<4 then goto @@lop
  ret
 @@key\_string db 'NINTENDO',01h,80h    ;10 bytes (5 halfwords; index=0..4)
```

**Data exchanged during Login**

```
               GBA                         ADAPTER
               xxxx494E ;\\     <-->        xxxxxxxx
               xxxx494E ; "NI" <--> "NI"/; 494EB6B1 ;\\
  NOT("NI") /; B6B1494E ;/     <-->     \\; 494EB6B1 ; NOT("NI")
            \\; B6B1544E ;\\"NT" <--> "NT"/; 544EB6B1 ;/
  NOT("NT") /; ABB1544E ;/     <-->     \\; 544EABB1 ;\\NOT("NT")
            \\; ABB14E45 ;\\"EN" <--> "EN"/; 4E45ABB1 ;/
  NOT("EN") /; B1BA4E45 ;/     <-->     \\; 4E45B1BA ;\\NOT("EN")
            \\; B1BA4F44 ;\\"DO" <--> "DO"/; 4F44B1BA ;/
  NOT("DO") /; B0BB4F44 ;/     <-->     \\; 4F44B0BB ;\\NOT("DO")
            \\; B0BB8001 ;-fin  <-->  fin-; 8001B0BB ;/
                 \\   \\                      \\   \\
                  \\   LSBs=Own               \\   LSBs=Inverse of
                   \\   Data.From.Gba          \\   Prev.Data.From.Gba
                    \\                          \\
                     MSBs=Inverse of            MSBs=Own
                      Prev.Data.From.Adapter     Data.From.Adapter
```

# GBA Wireless Adapter Commands

**Wireless Command/Parameter Transmission**

```
  GBA       Adapter
  9966ppcch 80000000h   ;-send command (cc), and num param\_words (pp)
  <param01> 80000000h   ;\\
  <param02> 80000000h   ; send "pp" parameter word(s), if any
  ...       ...         ;/
  80000000h 9966rraah   ;-recv ack (aa=cc+80h), and num response\_words (rr)
  80000000? <reply01>   ;\\
  80000000? <reply02>   ; recv "rr" response word(s), if any
  ...       ...         ;/
```

Wireless 32bit Transfers

```
  wait until \[4000128h\].Bit2=0  ;want SI=0
  set \[4000128h\].Bit3=1         ;set SO=1
  wait until \[4000128h\].Bit2=1  ;want SI=1
  set \[4000128h\].Bit3=0,Bit7=1  ;set SO=0 and start 32bit transfer
```

All command/param/reply transfers should be done at Internal Clock (except, Response Words for command 25h,27h,35h,37h should use External Clock).

**Wireless Commands**

```
  Cmd Para Reply Name
  10h -    -     Hello (send immediately after login)
  11h -    1     Good/Bad response to cmd 16h ?
  12h
  13h -    1
  14h
  15h
  16h 6    -     Introduce (send game/user name)
  17h 1    -     Config (send after Hello) (eg. param=003C0420h or 003C043Ch)
  18h
  19h
  1Ah
  1Bh
  1Ch -    -
  1Dh -    NN    Get Directory? (receive list of game/user names?)
  1Eh -    NN    Get Directory? (receive list of game/user names?)
  1Fh 1    -     Select Game for Download (send 16bit Game\_ID)



  20h -    1
  21h -    1     Good/Bad response to cmd 1Fh ?
  22h
  23h
  24h -    -
  25h                                       ;use EXT clock!
  26h -    -
  27h -    -     Begin Download ?           ;use EXT clock!
  28h
  29h
  2Ah
  2Bh
  2Ch
  2Dh
  2Eh
  2Fh



  30h 1    -
  31h
  32h
  33h
  34h
  35h                                       ;use EXT clock!
  36h
  37h                                       ;use EXT clock!
  38h
  39h
  3Ah
  3Bh
  3Ch
  3Dh -    -     Bye (return to language select)
  3Eh
  3Fh
```

Special Response 996601EEh for error or so? (only at software side?)

## GBA Wireless Adapter Component Lists

Main Chipset

```
  U1 32pin Freescale MC13190 (2.4 GHz ISM band transceiver)
  U2 48pin Freescale CT3000 or CT3001 (depending on adapter version)
  X3  2pin 9.5MHz crystal
```

The MC13190 is a Short-Range, Low-Power 2.4 GHz ISM band transceiver.  
The processor is Motorola's 32-bit M-Core RISC engine. (?) MCT3000 (?)  
See also: [http://www.eetimes.com/document.asp?doc_id=1271943](http://www.eetimes.com/document.asp?doc_id=1271943)

Version with GERMAN Postal Code on sticker:

```
  Sticker on Case:
    "GAME BOY advance, WIRELESS ADAPTER"
    "Pat.Pend.Made in Philipines, CE0125(!)B"
    "MODEL NO./MODELE NO.AGB-015 D-63760 Grossosteim P/AGB-A-WA-EUR-2 E3"
  PCB: "19-C046-04, A-7" (top side) and "B-7" and Microchip ",\\\\" (bottom side)
  PCB: white stamp "3104, 94V-0, RU, TW-15"
  PCB: black stamp "22FDE"
  U1 32pin "Freescale 13190, 4WFQ" (MC13190) (2.4 GHz ISM band transceiver)
  U2 48pin "Freescale CT3001, XAC0445"  (bottom side)
  X3  2pin "D959L4I" (9.5MHz)           (top side) (ca. 19 clks per 2us)
```

Further components... top side (A-7)

```
  D1   5pin "D6F, 44"   (top side, below X3)
  U71  6pin ".., () 2"  (top side, right of X3, tiny black chip)
  B71  6pin "\[\]"        (top side, right of X3, small white chip)
  ANT  2pin on-board copper wings
  Q?   3pin             (top side, above CN1)
  Q?   3pin             (top side, above CN1)
  D?   2pin "72"        (top side, above CN1)
  D3   2pin "F2"        (top side, above CN1)
  U200 4pin "MSV"       (top side, above CN1)
  U202 5pin "LXKA"      (top side, right of CN1)
  U203 4pin "M6H"       (top side, right of CN1)
  CN1  6pin connector to GBA link port (top side)
```

Further components... bottom side (B-7)

```
  U201 5pin "LXVB"      (bottom side, near CN1)
  U72  4pin "BMs"       (bottom side, near ANT, tiny black chip)
  FL70 ?pin "\[\] o26"    (bottom side, near ANT, bigger white chip)
  B70  6pin "\[\]"        (bottom side, near ANT, small white chip)
```

Plus, resistors and capacitors (without any markings).

Version WITHOUT sticker:

```
  Sticker on Case: N/A
  PCB: "19-C046-03, A-1" (top side) and "B-1" and Microchip ",\\\\" (bottom side)
  PCB: white stamp "3204, TW-15, RU, 94V-0"
  PCB: black stamp "23MN" or "23NH" or so (smeared)
  U1 32pin "Freescale 13190, 4FGD"      (top side)
  U2 48pin "Freescale CT3000, XAB0425"  (bottom side) ;CT3000 (not CT3001)
  X3  2pin "9.5SKSS4GT"                 (top side)
```

Further components... top side (A-1)

```
  D1   5pin "D6F, 31"   (top side, below X3)
  U71  6pin "P3, () 2"  (top side, right of X3, tiny black chip)
  B71  6pin "\[\]"        (top side, right of X3, small white chip)
  ANT  2pin on-board copper wings
  Q70  3pin             (top side, above CN1)
  D?   2pin "72"        (top side, above CN1)
  D3   2pin "F2"        (top side, above CN1)
  U200 4pin "MSV"       (top side, above CN1)
  U202 5pin "LXKH"      (top side, right of CN1)
  U203 4pin "M6H"       (top side, right of CN1)
  CN1  6pin connector to GBA link port (top side)
```

Further components... bottom side (B-1)

```
  U201 5pin "LXV2"      (bottom side, near CN1)
  U70  6pin "AAG"       (bottom side, near ANT, tiny black chip)
  FL70 ?pin "\[\] o26"    (bottom side, near ANT, bigger white chip)
  B70  6pin "\[\]"        (bottom side, near ANT, small white chip)
```

Plus, resistors and capacitors (without any markings).

Major Differences

```
  Sticker      "N/A"                     vs "Grossosteim P/AGB-A-WA-EUR-2 E3"
  PCB-markings "19-C046-03, A-1, 3204"   vs "19-C046-04, A-7, 3104"
  U1           "CT3000, XAB0425"         vs "CT3001, XAC0445"
  Transistors  One transistor (Q70)      vs Two transistors (both nameless)
  U70/U72      U70 "AAG" (6pin)          vs U72 "BMs" (4pin)
```

Purpose of the changes is unknown (either older/newer revisions, or different regions with different FCC regulations).
