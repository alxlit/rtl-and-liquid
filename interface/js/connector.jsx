/** @jsx React.DOM */

define(['jsx!client', 'react', 'react-bootstrap'], function(Client, React, Bs) {
  var cx = React.addons.classSet,
      Button = Bs.Button;
  
  return React.createClass({
    getDefaultProps: function() {
      return {
	modulations: null
      };
    },
    
    getInitialState: function() {
      return {
	status: 'unknown',
//	uri: 'ws://192.168.0.222:8080'
	uri: 'ws://0.0.0.0:8080'
      };
    },

    isValidWebSocketUri: function(uri) {
      return /^(wss?):\/\/(\w+|\d{1,3}(\.\d{1,3}){3})(:\d+)?$/.test(uri);
    },
  
    onChange: function(e) {
      this.setState({ uri: e.target.value, status: 'unknown' });
    },
    
    onClick: function(e) {
      var status = this.state.status,
          uri = this.state.uri,
          self = this;
      
      switch (status) {
      case 'connected':
	Client.disconnect();
	this.props.modulations.setState({ disabled: true });
	this.setState({ status: 'disconnected' });
	break;
	
      case 'error':
      case 'disconnected':
      case 'unknown':
	var ignoreClose = false;
	
	Client.connect(uri, function(reason, e) {
	  if (reason == 'close' && ignoreClose) return;
	  if (reason == 'error') ignoreClose = true;

	  Client.send('-f 90700000');
	  self.props.modulations.setState({ frequency: 90.7 });
	  

	  // transition in a little bit so the user can see
	  setTimeout(function() {
	    self.props.modulations.setState({ disabled: reason != 'open' });
	    
	    if (reason == 'error') {
	      self.setState({ status: 'error' });
	    }
	    else {
	      self.setState({ status: (reason == 'close' ? 'dis' : '') + 'connected' });
	    }
	  }, 250);
	});
	
	this.setState({ status: 'connecting' });
	break;
      }
      
      return false;
    },
  
    render: function() {
      var status = this.state.status,
          uri = this.state.uri,
          validUri = this.isValidWebSocketUri(uri),
          buttonDisabled = status == 'connecting' || ! validUri;
          buttonText = (status == 'connected'  ? 'Disconnect' :
		       (status == 'connecting' ? 'Connecting...' : 'Connect')),
          inputDisabled = status == 'connecting',
          form = cx({
	    'form-group': true,
	    'has-feedback': true,
	    'has-error': ! validUri || status == 'error',
	    'has-success': status == 'connected'
	  }),
          icon = cx({
	    'form-control-feedback': true,
	    'glyphicon': true,
	    'glyphicon-ok': status == 'connected',
	    'glyphicon-remove': ! validUri || status == 'error'
	  });
      
      return (
      <form className="navbar-form navbar-right connector" role="search">
        <div className={form}>
	  <input className="form-control" type="text" name="uri" disabled={inputDisabled}
	    onChange={this.onChange} value={uri} />
	  <span className={icon} />
        </div>
	<div className="form-group">
          <Button disabled={buttonDisabled} onClick={this.onClick}>{buttonText}</Button>
        </div>
      </form>
      );
    }
  });
});
