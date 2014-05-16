/** @jsx React.DOM */

define(['react'], function(React) {
  var cx = React.addons.classSet;

  // ugh
  var targetFreq = 0;
  
  return React.createClass({
    getDefaultProps: function() {
      return {
	disabled: false,
	min: 0.0,
	max: 1.0,
	frequency: 0.5,
	ghost: null,
	onChange: function() {},
	showCursor: true,
	step: 0.1,
	unit: '',
	stations: []
      }
    },
    
    getInitialState: function() {
      return {
	dragging: false,
	rel: { x: 0 }, // used for dragging
	height: 100,
	width: 100
      }
    },
    
    render: function() {
      var max = this.props.max,
          min = this.props.min,
          step = this.props.step,
          range = max - min,
          f = this.props.frequency || targetFreq, // TODO
          g = this.props.ghost || 0,
          self = this;

      var numTicks = Math.ceil(range / step) + 1,
          ticks = Array.apply(null, { length: numTicks }).map(function(_, i) {
	    return Math.round((min + i*step) * 100) / 100;
	  });
      
      var stations = this.props.stations;
      
      var w = this.state.width,
          h = this.state.height;

      var showCursor = this.props.showCursor;
      
      var color = this.props.disabled ? 'grey' : 'black',
          cursorColor = this.props.disabled ? 'grey' : 'red',
          ghostColor = 'red',
          classes = cx({
	    'frequency-tuner': true,
	    'col-md-12': true,
	    'disabled': this.props.disabled // doesn't work, react doesn't update it
	  });
      
      // create the cursor
      var pos = (f - min) / range,
          x = Math.round(pos * w),
          y = h/2,
          dy = 12;
      
      var cursor = <line x1={x} x2={x} y1={y-dy} y2={y+dy} stroke={cursorColor}
                      onMouseDown={this.onMouseDown} className="cursor" strokeWidth="3" />,
          cursorText = (<text x={x} y={y-1.5*dy} fill={color} textAnchor="middle">
	                  {f + (self.props.unit ? ' ' + self.props.unit : '')}
			</text>);

      // ghost cursor (reported by heartbeat)
      var pos = (g - min) / range,
          x = Math.round(pos * w),
          y = h/2,
          dy = 12;
      
      var ghost = <circle cx={x} cy={y} r="4" fill={ghostColor}
                    className="ghost-cursor" />;

      return (
      <div className="row">
	<svg height={h} width={w} viewport={"0 0 " + w + " " + h}
	     className={classes}>
	  <line x1={0} x2={w} y1={h/2} y2={h/2} stroke="black" strokeWidth="1" />
	  {
	    ticks.map(function(val) {
	      var pos = (val - min) / range,
	          x = Math.round(pos * w),
	          y = h/2,
	          elems = [],
	          station = stations.indexOf(val) != -1;
	          dy = station ? 12 : 5;
	      
	      elems.push(<line x1={x} x2={x} y1={y-dy} y2={y+dy} stroke={color}
			   strokeWidth="1" />);
	      
	      if (station) {
		elems.push(
		  <text x={x} y={y+2.5*dy} fill={color} onClick={self.props.onChange}
		    className="station" textAnchor="middle">
		    {val + (self.props.unit ? ' ' + self.props.unit : '')}
		  </text>
		);
	      }
	      
	      return elems;
	    })
	  }
	  {ghost}
	  {showCursor && cursor}
	  {showCursor && cursorText}
	</svg>
      </div>
      );
    },
    
    // draggable cursor: http://stackoverflow.com/questions/20926551
    onMouseDown: function(e) {
      if (e.button != 0) return;
      
      var box = this.getDOMNode().getBoundingClientRect(),
          left = window.pageXOffset + box.left;

      this.setState({
	dragging: true,
	rel: { x: left } // drag relative to left edge
      });
      
      e.stopPropagation();
      e.preventDefault();
    },
    
    onMouseUp: function(e) {
      this.setState({ dragging: false });
      this.props.onChange({ target: { dragging: false, value: targetFreq } });

      e.stopPropagation();
      e.preventDefault();
    },
    
    onMouseMove: function(e) {
      if ( ! this.state.dragging) return;
      
      var box = this.getDOMNode().getBoundingClientRect(),
          width = box.width - 2,
          x = e.pageX - this.state.rel.x;
      
      var max = this.props.max,
          min = this.props.min,
          range = max - min,
          step = this.props.step,
          i = Math.floor(x / width * range / step),
          freq = Math.round((min + i*step) * 100) / 100;
      
      if (freq < min) freq = min;
      if (freq > max) freq = max;

      targetFreq = freq;
      
      this.props.onChange({ target: { dragging: true, value: freq } });
      
//      this.props.onChange({target: {value: freq}});
      
      e.stopPropagation();
      e.preventDefault();
    },
    
    componentDidUpdate: function(props, state) {
      if (this.state.dragging && ! state.dragging) {
	document.addEventListener('mousemove', this.onMouseMove);
	document.addEventListener('mouseup', this.onMouseUp);
      }
      else if ( ! this.state.dragging && state.dragging) {
	document.removeEventListener('mousemove', this.onMouseMove);
	document.removeEventListener('mouseup', this.onMouseUp);
      }
    },

    // re-render on window resize: http://stackoverflow.com/questions/19014250
    updateDimensions: function() {
      var container = document.getElementById('modulations'),
          h = 100,
          w = container.offsetWidth - 2;
      
      this.setState({ width: w, height: h });
    },
    
    componentWillMount: function() {
      this.updateDimensions();
    },
    
    componentDidMount: function() {
      window.addEventListener('resize', this.updateDimensions);
    },

    componentWillUnmount: function() {
      window.removeEventListener('resize', this.updateDimensions);
    }
  });
});
