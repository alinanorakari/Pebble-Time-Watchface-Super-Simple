(function() {
  loadOptions();
  submitHandler();
})();

function submitHandler() {
  var $submitButton = $('#send');

  $submitButton.on('click', function() {
    console.log('Submit');

    var return_to = getQueryParam('return_to', 'pebblejs://close#');
    document.location = return_to + encodeURIComponent(JSON.stringify(getAndStoreConfigData()));
  });
}

function loadOptions() {
  var $colorbg = $('#color-bg');
  var $colorm = $('#color-m');
  var $colorh = $('#color-h');
  var $colorp = $('#color-p');

  if (localStorage.colorbg) {
    $colorbg[0].value = localStorage.colorbg;
    $colorm[0].value = localStorage.colorm;
    $colorh[0].value = localStorage.colorh;
    $colorp[0].value = localStorage.colorp;
  }
}

function getAndStoreConfigData() {
  var $colorbg = $('#color-bg');
  var $colorm = $('#color-m');
  var $colorh = $('#color-h');
  var $colorp = $('#color-p');

  var options = {
    colorbg: $colorbg.val(),
    colorm: $colorm.val(),
    colorh: $colorh.val(),
    colorp: $colorp.val(),
  };

  localStorage.colorbg = options.colorbg;
  localStorage.colorm = options.colorm;
  localStorage.colorh = options.colorh;
  localStorage.colorp = options.colorp;

  console.log('Got options: ' + JSON.stringify(options));
  return options;
}

function getQueryParam(variable, defaultValue) {
  var query = location.search.substring(1);
  var vars = query.split('&');
  for (var i = 0; i < vars.length; i++) {
    var pair = vars[i].split('=');
    if (pair[0] === variable) {
      return decodeURIComponent(pair[1]);
    }
  }
  return defaultValue || false;
}