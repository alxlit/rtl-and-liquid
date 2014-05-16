/** @jsx React.DOM */

define(['jsx!client', 'jsx!frequency-tuner', 'react', 'react-bootstrap'],
       
function(Client, FrequencyTuner, React, Bs) {
  var cx = React.addons.classSet,
      Button = Bs.Button,
      ButtonGroup = Bs.ButtonGroup;
  
  return React.createClass({
    getDefaultProps: function() {
      return {
	defaultFreq: 90.7,
	disabled: false,
	heartbeat: { fc: 0, fs: 0, mode: null, throughput: 0, snr: 0,
		     scanning: false, seeking: false, lastStationFound: -1  },
	stations: [],
	min: 87.9,
	max: 107.9,
	step: 0.2
      };
    },
    
    getInitialState: function() {
      return {
	frequency: 90.7,
      };
    },
    
    onChange: function(e) {
      if (this.props.disabled) return;
      
      var f = e.target.value || e.target.textContent;

      if (f != null) {
	f = '' + f;

	// don't spam 
	if ( ! e.target.dragging) { Client.setCenterFreq(f, 1e6); }
	
	this.setState({ frequency: f });
      }
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
      Client.setCenterFreq(f*1e6);
      
      this.setState({ frequency: f });
    },

    componentWillReceiveProps: function(props) {
      var heartbeat = this.props.heartbeat,
          fc = heartbeat.fc;
      
      if (heartbeat.seeking) {
	this.setState({ frequency: Math.floor(fc*100 / 1e6) / 100 });
      }
    },
    
    render: function() {
      var heartbeat = this.props.heartbeat,
          stations = this.props.stations;

      // convert to MHz
      stations = stations.map(function(s) {
	return s > 1e6 ? Math.floor(s*100 / 1e6) / 100 : s;
      });

      this.tuner = this.transferPropsTo(
	<FrequencyTuner frequency={this.state.frequency}
	  ghost={heartbeat.fc / 1e6} onChange={this.onChange}
  	  showCursor={ ! (heartbeat.scanning || heartbeat.seeking)}
	  stations={stations} />
      );

      return (
      <fieldset disabled={this.props.disabled && 'disabled'}>
        <div className="row">
	  <div className="col-sm-8">
	    <h4>Frequency modulation</h4>
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
