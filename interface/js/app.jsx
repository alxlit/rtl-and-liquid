/** @jsx React.DOM */

define(['jsx!client', 'jsx!connector', 'jsx!modulations', 'react'],

function(Client, Connector, Modulations, React) {
  var AudioContext = AudioContext || webkitAudioContext;
  
  return {
    // I/O objects
    ac: null,
    anode: null,
    ws: null,
    
    // view components
    connector: null,
    modulations: null,
    
    // config
    bufferSize: Math.pow(2,11),
    inputChannels: 1,
    outputChannels: 1,
    
    // storage and state
    buffers: [],
    cursor: 0,
    semaphore: 0,
    
    init: function() {
      var self = this;
      
      this.ac = new AudioContext();
      this.anode = this.ac.createScriptProcessor(this.bufferSize, this.inputChannels,
						this.outputChannels);

      // set up audio processing
      this.anode.onaudioprocess = function(e) {
	var buf;
	
	if (self.semaphore > 0) {
	  buf = self.buffers.shift();
	  self.semaphore--;
        }
	else {
	  buf = new Float32Array(self.bufferSize);
	}
	
	e.outputBuffer.getChannelData(0).set(buf);
      };
      
      // start playing (nothing)
      this.anode.connect(this.ac.destination);
 
      Client.onMessage = this.onMessage.bind(this);

      this.modulations = <Modulations />;      
      this.connector = <Connector modulations={this.modulations} />;

      // render interface
      React.renderComponent(this.connector,
			    document.getElementById('connector'));
      
      React.renderComponent(this.modulations,
			    document.getElementById('modulations'));
    },

    onMessage: function(e) {
      var view = new DataView(e.data),
          littleEndian = true,
          headerSize = view.getUint32(0, littleEndian),
          header = [];

      // the current offset
      var offset = 4;

      if (headerSize > 0) {
	for (var i = 0; i < headerSize; i++) {
	  header.push(view.getUint8(offset + i)); }

	offset += headerSize;
	
	// convert to string, parse
	header = JSON.parse(String.fromCharCode.apply(null, header));

	// do something
	// console.log(header);

	// should've made the heartbeat info global somehow, instead of passing it
	// downward like this
	this.modulations.setState({ heartbeat: header });
      }

      var dataSize = Math.floor((e.data.byteLength - offset) / 2),
          nf = 32767.0,
          buf = this.buffers[this.buffers.length - 1];
      
      for (var i = 0; i < dataSize; i++) {
	if (buf == null || this.cursor >= this.bufferSize) {
	  buf = new Float32Array(this.bufferSize);
	  this.buffers.push(buf);
	  this.cursor = 0;
	  this.semaphore++;
	}

	// read next sample into buffer
	buf[this.cursor++] = view.getInt16(offset + 2*i, littleEndian) / nf;
      }
    }
  };
}

);
