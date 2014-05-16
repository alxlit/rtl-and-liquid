/** @jsx React.DOM */

define(['jsx!client', 'react'], function(Client, React) {
  return React.createClass({
    getDefaultProps: function() {
      return {
	defaultArity: 'bpsk',
	defaultFreq: 910,
	disabled: false,
	heartbeat: { fc: 0, fs: 0, mode: null, throughput: 0 },	
      };
    },

    getInitialState: function() {
      return {
	arity: 'bpsk',
	frequency: 910
      };
    },
    
    componentWillMount: function() {
      var arity = this.props.defaultArity,
          f1 = this.props.defaultFreq,
          f2 = f1 * 1e6;

      Client.send('-m ' + arity);
      Client.send('-f ' + f2);
      
      this.setState({ arity: arity, frequency: f1 });
    },
    
    onChangeArity: function(e) {
      if (this.props.disabled) return;
      
      var val = e.target.value;

      Client.send('-m ' + val);
      this.setState({ arity: val });
    },
  
    render: function() {
      return (
      <fieldset disabled={this.props.disabled && 'disabled'}>
	<div className="row">
	  <div className="col-sm-6">
	    <h4>Phase-shift keying</h4>
	  </div>
	</div>
	<div className="row">
	  <div className="col-sm-6">
	    <select className="form-control" value={this.state.arity}
	      onChange={this.onChangeArity}>
	      <option value="bpsk">DPSK</option>
	      <option value="qpsk">QPSK</option>
	      <option value="8psk">8PSK</option>
	    </select>
	  </div>
	</div>
      </fieldset>
      );
    }
  });
});
