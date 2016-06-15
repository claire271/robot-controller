var app = require('express')();
var http = require('http').Server(app);
var io = require('socket.io')(http);

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

  console.log(left + "," + right);
}
