//Imports
var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var SerialPort = require('serialport');
var ekf_slam = require('./ekf_slam').ekf_slam;

//Serial to/from the C & ASM PRU controller
var serial = new SerialPort.SerialPort("./serial0", {
  baudRate: 115200,
  parser: SerialPort.parsers.byteDelimiter([0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00])
});
serial.on('data',function(data) {
  var left_in = (data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0]) & 0xFFFFFFFF;
  var right_in = (data[9] << 24 | data[8] << 16 | data[7] << 8 | data[6]) & 0xFFFFFFFF;

  var encoders = {};
  encoders.left = left_in;
  encoders.right = right_in;
  io.emit('encoder data',encoders);
});
var outBuf = Buffer.alloc(6);

//LIDAR constants
const VD = 633;
const H = 100;
const IW = 640;
const IH = 480;

//Initializing slam
//var slam = new ekf_slam();

/*
//Serial to/from the LIDAR
var serial2 = new SerialPort.SerialPort("/dev/ttyS2", {
  baudRate: 115200,
  parser: SerialPort.parsers.byteDelimiter([0xFF])
});
serial2.on('data',function(data) {
  //Bail if incomplete frame
  if(data.length - 1 < IW) return;

  var points = [];
  var npoints = 0;
  for(var i = 0;i < IW;i++) {
    //Bail if no point detected in this column
    if(data[i] == 254) continue;

    points[npoints] = {};
    points[npoints].y = VD * H * 1.0 / data[i];
    points[npoints].x = points[npoints].y * (i - IW/2) / VD;
    npoints++;
  }

  io.emit('lidar data',points);
});
*/

//Web routing information
app.get('/', function(req, res){
  res.sendFile(__dirname + '/index.html');
});
app.get('/client.js', function(req, res){
  res.sendFile(__dirname + '/client.js');
});

//Sockets for the driver station data
io.on('connection', function(socket){
  console.log('a user connected');

  socket.on('joystick input', processJoystick);
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});

//Forwarding joystick stuff to the PRU
function processJoystick(joystick) {
  var xin = joystick.axes[3];
  var yin = -joystick.axes[1];

  xin = xin * Math.abs(xin);
  yin = yin * Math.abs(yin);

  var length = Math.max(Math.abs(xin) + Math.abs(yin),1);
  xin /= length;
  yin /= length;

  var left = yin + xin;
  var right = yin - xin;

  //console.log(left + "," + right);
  //outBuf[0] = Math.round(left * 1000);
  //outBuf[1] = Math.round(right * 1000);
  var tmpLeft = Math.round(left * 1000 + 0x8000);
  var tmpRight = Math.round(right * 1000 + 0x8000);
  outBuf[0] = (tmpLeft) & 0xFF;
  outBuf[1] = (tmpLeft >> 8) & 0xFF;
  outBuf[2] = (tmpRight) & 0xFF;
  outBuf[3] = (tmpRight >> 8) & 0xFF;
  outBuf[4] = 0xFF;
  outBuf[5] = 0xFF;
  serial.write(outBuf, function(error) {
    serial.flush();
  });
  //console.log("bye");
}
