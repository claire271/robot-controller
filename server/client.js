var socket = io();

socket.on('disconnect', disable);

var enabled = false;

//Init document elements
var gamepad_num = document.getElementById("gamepad_num");
var enabled_status = document.getElementById("enabled_status");

gamepad_num.value = 0;
enabled_status.value = false;

//Setting up buttons
document.getElementById("enable_button").onclick = enable;
document.getElementById("disable_button").onclick = disable;

//Setting up canvas
var canvas = document.getElementById("lidar_out");
var ctx = canvas.getContext("2d");

//LIDAR visualization constants
const WIDTH = 800;
const HEIGHT = 600;
const SCALE = 2;
const GRIDW = 100;

//LIDAR recieve stuff
socket.on('lidar data', function(points) {
  console.log("HI");

  //Background
  ctx.fillStyle = "#000000";
  ctx.fillRect(0,0,WIDTH,HEIGHT);

  //Background lines
  ctx.strokeStyle = "#3F0000";
  for(var i = HEIGHT - GRIDW/SCALE;i >= 0;i -= GRIDW/SCALE) {
    ctx.moveTo(0,i);
    ctx.lineTo(WIDTH,i);
  }
  for(var i = GRIDW/SCALE;i < WIDTH/2;i += GRIDW/SCALE) {
    ctx.moveTo(WIDTH/2 + i,0);
    ctx.lineTo(WIDTH/2 + i,HEIGHT);
    ctx.moveTo(WIDTH/2 - i,0);
    ctx.lineTo(WIDTH/2 - i,HEIGHT);
  }
  ctx.stroke();

  ctx.strokeStyle = "#3F003F"
  ctx.beginPath();
  ctx.moveTo(WIDTH/2,0);
  ctx.lineTo(WIDTH/2,HEIGHT);
  ctx.moveTo(0,HEIGHT);
  ctx.lineTo(WIDTH,HEIGHT);
  ctx.stroke();


  ctx.fillStyle = "#00FF00";
  for(var i = 0;i < points.length;i++) {
    ctx.fillRect(Math.round(WIDTH/2 + points[i].x/SCALE - .5),Math.round(HEIGHT - points[i].y/SCALE - .5),1,1);
  }
});


//Initiailizing gamepad stuff
var gamepad_count;

//Running sending code
var sendId = setInterval(sendJoystick,50);
function sendJoystick() {
  //Bail if we are not enabled
  refreshGamepadCount();
  if(!enabled) return;

  //Send joystick input
  var joystick = navigator.getGamepads()[0];

  var joystick1 = {};
  joystick1.id = joystick.id;
  joystick1.index = joystick.index;
  joystick1.mapping = joystick.mapping;
  joystick1.connected = joystick.connected;
  joystick1.buttons = joystick.buttons;
  joystick1.axes = joystick.axes;
  joystick1.timestamp = joystick.timestamp;

  socket.emit('joystick input', joystick1);
}

function refreshGamepadCount() {
  gamepad_count = navigator.getGamepads().length;
  if(gamepad_count == 0) disable();
  gamepad_num.value = gamepad_count;
}

function enable() {
  if(gamepad_count > 0) {
    enabled = true;
  }
  enabled_status.value = enabled;
}

function disable() {
  enabled = false;
  enabled_status.value = enabled;
}
