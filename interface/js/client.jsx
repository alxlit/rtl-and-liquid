/** @jsx React.DOM */

define(function() {
  return {
    connect: function(uri, callback) {
      this.ws = new WebSocket(uri, 'sdr');
      this.ws.binaryType = 'arraybuffer';
      
      this.ws.onopen  = callback.bind(null, 'open');
      this.ws.onclose = callback.bind(null, 'close');
      this.ws.onerror = callback.bind(null, 'error');
      
      this.ws.onmessage = this.onMessage;
    },
    
    disconnect: function() {
      this.ws && this.ws.close();
    },
    
    send: function(data) {
      this.ws && this.ws.send(data);
    },

    setCenterFreq: function(fc, ord) {
      fc = typeof fc == 'string' ? parseFloat(fc) * (ord || 1e6) : fc;
      fc = Math.floor(fc);

      // TODO
      fc && this.send('-f ' + fc);
    },

    setMode: function(mode) { this.send('-m ' + mode); },

    // 'on' or 'off'
    setScan: function(state) { this.send('-s ' + state); },

    // 'up' or 'down'
    setSeek: function(state) { this.send('-s ' + state); },
    
    setSampleRate: function(fs, ord) {
      fs = typeof fs == 'string' ? parseFloat(fs) * (ord || 1e6) : fs;
      fs = Math.floor(fs);
      
      this.send('-r ' + fs);
    },

    onMessage: function() {}
  };
});
