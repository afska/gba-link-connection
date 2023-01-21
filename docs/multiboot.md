<html>
<div class="s-prose js-post-body" itemprop="text">

<p>From: <a href="https://retrocomputing.stackexchange.com/questions/14317/what-is-the-protocol-for-bootstrapping-a-game-boy-advance-over-a-link-cable">https://retrocomputing.stackexchange.com/questions/14317/what-is-the-protocol-for-bootstrapping-a-game-boy-advance-over-a-link-cable</a></p>

<p>According to the documentation for the no$ emulators (<a href="https://www.problemkaputt.de/gbatek.htm" rel="nofollow noreferrer">https://www.problemkaputt.de/gbatek.htm</a>), the core of the GBA's multiboot functionality revolves a BIOS syscall, <code>SWI 0x25</code>. The GBA's boot firmware will naturally look for and start communicating on any connection mode it supports (be it the normal serial mode, MultiPlay mode, or JOYBUS mode) - after the host GBA establishes a communcation session and sends handshake and header data, this syscall will send the multiboot payload to each connected GBA, verify with a CRC checksum, and begin execution.</p>
<h2>Payload Structure</h2>
<p>The multiboot payload resides at <code>0x02000000</code> on the client GBA, which is the location corresponding to 256KB of external work RAM. The first 192 bytes are the same as a cartridge payload (entry point, Nintendo logo, game title, etc.), followed by these entries specific to multiboot payloads:</p>
<div class="s-table-container">
<table class="s-table">
<thead>
<tr>
<th>Address</th>
<th>Bytes</th>
<th>Label</th>
<th>Comment</th>
</tr>
</thead>
<tbody>
<tr>
<td>0x0c0</td>
<td>4</td>
<td>RAM Entry Point</td>
<td>CPU will begin execution here if mulitboot was installed from another GBA</td>
</tr>
<tr>
<td>0x0c4</td>
<td>1</td>
<td>Boot Mode</td>
<td>Leave at 0 - the BIOS will overwrite this depending on how the payload was downloaded</td>
</tr>
<tr>
<td>0x0c5</td>
<td>1</td>
<td>Client ID number</td>
<td>Leave at 0 - the BIOS will overwrite this with the player number minus 1 (e.g. player 4 will get 0x03 here)</td>
</tr>
<tr>
<td>0x0c6</td>
<td>26</td>
<td>Not used</td>
<td></td>
</tr>
<tr>
<td>0x0e0</td>
<td>4</td>
<td>JOYBUS Entry Point</td>
<td>CPU will begin execution here if multiboot was installed via JOYBUS</td>
</tr>
</tbody>
</table>
</div>
<p>As with the cartridge header, these entry points should be 32-bit ARM branch instructions leading to the actual start of the code (e.g. <code>B ram_start</code>). Execution in general should be relative to <code>0x02000000</code> instead of <code>0x08000000</code> as it is when executing from cartridge (you will likely compile the Multiboot payload separately and include the binary in your cartridge)</p>
<p>The cartridge header portion is not particularly special - in fact, since the cartridge header and remaining payload are sent in separate steps, you can plan on just sending the actual cartridge header here. The ROM entry point, normally found at the start of the cartridge header, will be ignored during multiboot.</p>
<h2>Transfer Protocol</h2>
<p>The latter half of the multiboot protocol (sending the payload and executing it) are handled by the <code>SWI 0x25</code> syscall. The first half (initiating the session and sending the cartridge header) must be handled by the session host:</p>
<ol>
<li>Prepare a "Multiboot Parameter Structure" in RAM. It should be <code>0x4c</code> bytes long and contain the following fields at these addresses:</li>
</ol>
<div class="s-table-container">
<table class="s-table">
<thead>
<tr>
<th>Address</th>
<th>Bytes</th>
<th>Label</th>
<th>Comment</th>
</tr>
</thead>
<tbody>
<tr>
<td>0x14</td>
<td>1</td>
<td><code>handshake_data</code></td>
<td>Will be filled in later</td>
</tr>
<tr>
<td>0x19</td>
<td>3</td>
<td><code>client_data</code></td>
<td>Will be filled in later using random data send by the clients</td>
</tr>
<tr>
<td>0x1c</td>
<td>1</td>
<td><code>palette_data</code></td>
<td>Data controlling the color of the Nintendo logo on client devices - of the form 0b1CCCDSS1, where C=color, D=direction, and S=speed</td>
</tr>
<tr>
<td>0x1e</td>
<td>1</td>
<td><code>client_bit</code></td>
<td>Will be filled in with each of bits 1-3 set if client 1-3 is detected</td>
</tr>
<tr>
<td>0x20</td>
<td>4</td>
<td><code>boot_srcp</code></td>
<td>The start address of the payload after the cartridge header</td>
</tr>
<tr>
<td>0x24</td>
<td>4</td>
<td><code>boot_endp</code></td>
<td>The exclusive end address of the payload. The total transfer length should be a multiple of 0x10, with a minimum of 0x100 and a maximum of 0x3ff40</td>
</tr>
</tbody>
</table>
</div>
<ol start="2">
<li><p>Initiate a multiplayer communication session, using either Normal mode for a single client or MultiPlay mode for multiple clients. This is accomplished using the RCNT and SIOCNT registers as with a multiplayer game session.</p>
</li>
<li><p>Send the word <code>0x6200</code> repeatedly until all detected clients respond with <code>0x720X</code>, where X is their client number (1-3). If they fail to do this after 16 tries, delay 1/16s and go back to step 2.</p>
<ul>
<li>Note that throughout this process, any clients that are not connected will always respond with <code>0xFFFF</code> - be sure to ignore them.</li>
</ul>
</li>
<li><p>Fill in <code>client_bit</code> in the multiboot parameter structure (with bits 1-3 set according to which clients responded). Send the word <code>0x610Y</code>, where Y is that same set of set bits.</p>
</li>
<li><p>Send the cartridge header, 16 bits at a time, in little endian order. After each 16-bit send, the clients will respond with <code>0xNN0X</code>, where NN is the number of words remaining and X is the client number. (Note that if transferring in the single-client 32-bit mode, you still need to send only 16 bits at a time).</p>
</li>
<li><p>Send <code>0x6200</code>, followed by <code>0x620Y</code> again</p>
</li>
<li><p>Send <code>0x63PP</code> repeatedly, where PP is the palette_data you have picked earlier. Do this until the clients respond with <code>0x73CC</code>, where CC is a random byte. Store these bytes in client_data in the parameter structure.</p>
</li>
<li><p>Calculate the handshake_data byte and store it in the parameter structure. This should be calculated as <code>0x11</code> + the sum of the three client_data bytes. Send <code>0x64HH</code>, where HH is the handshake_data.</p>
</li>
<li><p>Call <code>SWI 0x25</code>, with <code>r0</code> set to the address of the multiboot parameter structure and <code>r1</code> set to the communication mode (0 for normal, 1 for MultiPlay).</p>
</li>
<li><p>Upon return, <code>r0</code> will be either 0 for success, or 1 for failure. If successful, all clients have received the multiboot program successfully and are now executing it - you can begin either further data transfer or a multiplayer game from here.</p>
</li>
</ol>
<p>Code sample on Github using the Rust GBA crate: <a href="https://github.com/TheHans255/rust-gba-multiboot-test" rel="nofollow noreferrer">https://github.com/TheHans255/rust-gba-multiboot-test</a>.</p>
<p>Reference:</p>
<ul>
<li>BIOS multiboot syscall: <a href="https://www.problemkaputt.de/gbatek.htm#biosmultibootsinglegamepak" rel="nofollow noreferrer">https://www.problemkaputt.de/gbatek.htm#biosmultibootsinglegamepak</a></li>
<li>MultiPlay mode: <a href="https://www.problemkaputt.de/gbatek.htm#siomultiplayermode" rel="nofollow noreferrer">https://www.problemkaputt.de/gbatek.htm#siomultiplayermode</a></li>
</ul>
    </div>
    </html>