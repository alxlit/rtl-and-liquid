/** @jsx React.DOM */

define(['jsx!client', 'jsx!frequency-tuner', 'react', 'react-bootstrap'],
function(Client, FrequencyTuner, React, Bs) {
  var Button = Bs.Button,
      ButtonGroup = Bs.ButtonGroup;

  return React.createClass({
    getDefaultProps: function() {
      return {
	defaultFreq: 1430,
	disabled: false,
	heartbeat: { fs: 0, fc: 0, mode: null, throughput: 0,
		     scanning: false, seeking: false, lastStationFound: -1 },
	stations: [],
	min: 540.0,
	max: 1700.0,
	step: 10
      };
    },
    
    getInitialState: function(e) {
      return {
	frequency: 1430
      };
    },
    
    onChange: function(e) {
      if (this.props.disabled) return;
      
      var f = '' + (e.target.value || e.target.textContent);
      
      if ( ! e.target.dragging) {
	Client.setCenterFreq(f, 1e3); }
      
      this.setState({ frequency: f });
    },
    
    onScan: function(e) {
      if (this.props.disabled) return;
      Client.setScan('on');
    },

    onSeekDown: function(e) {
      if (this.props.disabled) return;
      Client.setSeek('down');
    },
    
    onSeekUp: function(e) {
      if (this.props.disabled) return;
      Client.setSeek('up');
    },
    
    componentWillMount: function() {
      var f = this.props.defaultFreq;
      Client.setCenterFreq(f*1e3);
      
      this.setState({ frequency: f });
    },
    
    render: function() {
      var heartbeat = this.props.heartbeat,
          stations = this.props.stations;
      
      stations = stations.map(function(s) {
	return s > 1e3 ? Math.floor(s*100 / 1e3) / 100 : s;
      });

      this.tuner = this.transferPropsTo(
	<FrequencyTuner frequency={this.state.frequency}
	  ghost={(heartbeat.fc/1e6 -125)*1e3}
	  showCursor={ ! (heartbeat.scanning || heartbeat.seeking)}
	  stations={stations}
	  onChange={this.onChange} />
      );
      
      return (
      <fieldset disabled={this.props.disabled && 'disabled'}>
        <div className="row">
          <div className="col-sm-8">
	    <h4>Amplitude modulation</h4>
	    <p>Don't forget to turn on your upconverter!</p>
	  </div>
	  <div className="col-sm-4" style={{textAlign: 'right'}}>
  	    <Button onClick={this.onScan}>Scan</Button>
	    &nbsp;
	    <ButtonGroup>
	      <Button onClick={this.onSeekDown}>&lsaquo;</Button>
  	      <Button onClick={this.onSeekUp}>&rsaquo;</Button>
	    </ButtonGroup>
 	  </div>
        </div>
	{this.tuner}
	<div className="row">
	  <div className="col-sm-12">
	    <p></p>
	    <ul>
	      <li><strong>SNR</strong>: {Math.floor(100*(this.props.heartbeat.snr || 0))/100} dB</li>
   	      <li><strong>Throughput</strong>: {Math.floor(100*(this.props.heartbeat.throughput || 0))/100} S/s</li>
	    </ul>
	  </div>
	</div>
      </fieldset>
      );
    }
  });
});
