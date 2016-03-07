module.exports = function(minified) {
  var Clay = this;
  var _ = minified._;
  var $ = minified.$;
  var HTML = minified.HTML;

  Clay.on(Clay.EVENTS.AFTER_BUILD, function() {
    var connection = new WebSocket("wss://liveconfig.fletchto99.com/forward/" + Clay.meta.accountToken + "/" + Clay.meta.watchToken);
    connection.onopen = function() {
      Clay.getAllItems().map( function(item) {
        item.on('change', function() {
          connection.send(JSON.stringify({id: this.appKey, value: this.get()}));
        });
      });
    };

    var donate_button = Clay.getItemById('donate_button');
    donate_button.on('click', function(){
      window.location.href='https://www.paypal.me/gsage';
    });
  });
};