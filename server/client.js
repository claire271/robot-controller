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
