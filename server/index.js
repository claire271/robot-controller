var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io')(http);
var SerialPort = require('serialport').SerialPort;
var serial = new SerialPort("./serial0", {
  baudRate: 115200
});
serial.on('data',function(data) {
  console.log("HI");
});

var outBuf = Buffer.alloc(6);

app.get('/', function(req, res){
  res.sendFile(__dirname + '/index.html');
});
app.get('/client.js', function(req, res){
  res.sendFile(__dirname + '/client.js');
});

io.on('connection', function(socket){
  console.log('a user connected');

  socket.on('joystick input', processJoystick);
});

http.listen(3000, function(){
  console.log('listening on *:3000');
});

function processJoystick(joystick) {
  var xin = joystick.axes[3];
  var yin = -joystick.axes[1];

  var length = Math.max(Math.abs(xin) + Math.abs(yin),1);
  xin /= length;
  yin /= length;

  var left = yin + xin;
  var right = yin - xin;

  //console.log(left + "," + right);
  //outBuf[0] = Math.round(left * 1000);
  //outBuf[1] = Math.round(right * 1000);
  var tmpLeft = Math.round(left * 1000 + 1000);
  var tmpRight = Math.round(right * 1000 + 1000);
  outBuf[0] = (tmpLeft) & 0xFF;
  outBuf[1] = (tmpLeft >> 8) & 0xFF;
  outBuf[2] = (tmpRight) & 0xFF;
  outBuf[3] = (tmpRight >> 8) & 0xFF;
  outBuf[4] = 0xFF;
  outBuf[5] = 0xFF;
  serial.write(outBuf, function(error) {
    serial.flush();
  });
}
