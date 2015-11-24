Pebble.addEventListener('ready', function() {
    console.log('PebbleKit JS ready!');
});

Pebble.addEventListener('showConfiguration', function() {
    var watch;
    if(Pebble.getActiveWatchInfo) {
      watch = Pebble.getActiveWatchInfo();
    }
    var url='http://pebble.lastfuture.de/config/supersimple15/';
    if (watch.platform != "chalk") {
      url += "?rect=true";
    }
    console.log('Showing configuration page: '+url);
    Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
    var configData = JSON.parse(decodeURIComponent(e.response));
    console.log('Configuration page returned: '+JSON.stringify(configData));
    if (configData.colorbg) {
        Pebble.sendAppMessage({
            colorbg: parseInt(configData.colorbg, 16),
            colorm: parseInt(configData.colorm, 16),
            colorh: parseInt(configData.colorh, 16),
            colorp: parseInt(configData.colorp, 16),
            shadows: configData.shadows === 'true',
            ticks: configData.ticks,
            colort: parseInt(configData.colort, 16),
            rectticks: configData.rectticks === 'true'
        }, function() {
            console.log('Send successful!');
        }, function() {
            console.log('Send failed!');
        });
    }
});