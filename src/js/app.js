var appinfo = require('appinfo.json');

var Clay = require('clay');
var clayConfig = require('config.json');
var customClay = require('custom-clay');
var clay = new Clay(clayConfig, customClay, {autoHandleEvents: false});

Pebble.addEventListener('ready', function(e) {
  console.log(appinfo.longName + " " + appinfo.versionLabel + " ready !");
});

Pebble.addEventListener('showConfiguration', function(e) {
  var websocket = new WebSocket("wss://liveconfig.fletchto99.com/receive/"+Pebble.getAccountToken()+"/"+Pebble.getWatchToken());
  websocket.onmessage = function(message) {
    var attr = JSON.parse(message.data);
    var config = {};
    config[attr.id] = attr.value;
    console.log(JSON.stringify(config));
    Pebble.sendAppMessage(Clay.prepareSettingsForAppMessage(config));
  };
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && !e.response) { return; }

  // Send settings to Pebble watchapp
  Pebble.sendAppMessage(clay.getSettings(e.response), function(e) {
    console.log('Sent config data to Pebble');
  }, function() {
    console.log('Failed to send config data!');
    console.log(JSON.stringify(e));
  });
});