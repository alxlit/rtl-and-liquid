/** @jsx React.DOM */

define(['jsx!client', 'react'], function(Client, React) {
  return React.createClass({
    getDefaultProps: function() {
      return {
	disabled: false,
	heartbeat: { fs: 0, fc: 0, mode: null, throughput: 0 }
      };
    },
    
    getInitialState: function() {
      return {
	sampleRate: this.props.heartbeat.fs
      };
    },
      
    onChangeSampleRate: function(e) {
      if (this.props.disabled) return;
      
      var rate = parseInt(e.target.value);
      Client.setSampleRate(rate);
      
      var self = this;

      // TODO
      setTimeout(function() { Client.setCenterFreq(self.props.heartbeat.fc); }, 1000);

      this.setState({ sampleRate: rate });
    },
    
    render: function() {
      var self = this,
          selected = function(fs) { return fs == self.sampleRate; };
	
      return (
      <fieldset disabled={this.props.disabled && 'disabled'}>
	<div className="row">
	  <div className="col-sm-6">
	    <h4>Settings</h4>
	  </div>
	</div>
	<div className="row">
	  <form className="form-horizontal">
	    <div className="form-group">
	      <label className="col-sm-2 control-label">Sample Rate</label>
	      <div className="col-sm-10">
	      <select className="form-control" value={this.state.sampleRate}
	        onChange={this.onChangeSampleRate}>
	        <option value="250000">250 kHz</option>
  	        <option value="1000000">1 MHz</option>
	        <option value="1920000">1.92 MHz</option>
	        <option value="2000000">2 MHz</option>
	        <option value="2048000">2.048 MHz</option>
	      </select>
	      </div>
	    </div>
          </form>
        </div>
      </fieldset>
      );
    }
  });  
});
