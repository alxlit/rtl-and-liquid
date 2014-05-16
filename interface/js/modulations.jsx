/** @jsx React.DOM */

define(['jsx!am', 'jsx!client', 'jsx!fm', 'jsx!settings',
	'react', 'react-bootstrap'],
       
function(Am, Client, Fm, Settings, React, Bs) {
  var cx = React.addons.classSet,
      NavItem = Bs.NavItem;

  return React.createClass({
    getInitialState: function() {
      return {
	heartbeat: { fs: 0, fc: 0, mode: null, throughput: 0 },
	mode: 'fm',
	disabled: true
      };
    },

    getDefaultProps: function() {
      return {
	stations: { fm: [], am: [] }
      };
    },
    
    onClick: function(e) {
      if (this.state.disabled) return;

      var mode = e.target.textContent.toLowerCase();

      // the others aren't really demod modes
      if (mode == 'am' || mode == 'fm') { Client.setMode(mode); }

      this.setState({ mode: mode });
    },
    
    render: function() {
      var self = this,
          mode = this.state.mode,
          heartbeat = this.state.heartbeat,
          recordStations = heartbeat.scanning || heartbeat.seeking,
          lastStation = heartbeat.lastStationFound,
          stations = this.props.stations;

      if (recordStations && lastStation > 0 && stations[mode].indexOf(lastStation) == -1) {
	stations[mode].push(lastStation);
      }
      
      var views = {
	    'am': Am,
	    'fm': Fm,
	    'settings': Settings
	  },
          view = views[mode]({
	    disabled: this.state.disabled,
	    heartbeat: this.state.heartbeat,
	    stations: stations[mode]
	  }),
          classes = cx({
	    'nav': true,
	    'nav-disabled': this.state.disabled,
	    'nav-tabs': true
	  });
      
      return (
      <div>
        <div className="row">
          <div className="col-md-12">
            <ul className={classes}>
              <NavItem onClick={this.onClick} active={mode == 'am'}>AM</NavItem>
              <NavItem onClick={this.onClick} active={mode == 'fm'}>FM</NavItem>
	      <NavItem onClick={this.onClick} active={mode == 'settings'}>Settings</NavItem>
	    </ul>
	  </div>
        </div>
        <div className="row">
          <div className="col-md-12">{view}</div>
        </div>
      </div>
      );
    }
  });
});
