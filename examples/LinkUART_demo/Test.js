// node v14
const { SerialPort, ReadlineParser } = require("serialport"); // "^12.0.0"

var serialPort = new SerialPort({
  path: "COM9", // (*nix: /dev/ttyACMX, Windows: COMX)
  baudRate: 9600,
});
const parser = serialPort.pipe(new ReadlineParser());

parser.on("data", (it) => console.log(it));

setInterval(() => {
  serialPort.write("<< node\n");
}, 1000);
