Pebble.addEventListener('ready', function() {
    console.log('PebbleKit JS ready!');
});

Pebble.addEventListener('showConfiguration', function() {
    var url='http://pebble.lastfuture.de/config/supersimple/';
    console.log('Showing configuration page: '+url);
    Pebble.openURL(url);
});