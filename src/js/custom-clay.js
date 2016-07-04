module.exports = function(minified) {
  var Clay = this;
  var _ = minified._;
  var $ = minified.$;
  var HTML = minified.HTML;

  Clay.on(Clay.EVENTS.AFTER_BUILD, function() {
    var donate_button = Clay.getItemById('donate_button');
    donate_button.on('click', function(){
      window.location.href='https://www.paypal.me/gsage';
    });
  });
};