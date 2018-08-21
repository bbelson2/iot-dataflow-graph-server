/*
//  IoT Dataflow Graph Server
//  Copyright (c) 2015, Adam Rehn & Bruce Belson
//  
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
*/

/*
	These operators allow the preservation of boolean state.
	The nodes detect a rising edge as a signal, i.e. an input which switches from false
	to true. 
	The Toggle node has one input, & will toggle its state whenever it receives a 
	signal on the input.
	The switch node has tow inputs. It will switch to true when it recieves a signal 
	on input 0 and switch to false whenever it receives a signal on input 1.
*/

function convertToBool(val)
{
	var numericValue = parseFloat(val);
	return (val === true || val == "true" || (!isNaN(numericValue) && numericValue != 0));
}

function Toggle()
{
	this.lastInput = false;
	this.value = false;
}

Toggle.prototype.readInput = function(newValue) {
	// Input is boolean
	newValue = convertToBool(newValue);
	// Catch a rising edge
	if (newValue && !this.lastInput) {
		this.value = !this.value;
	}
	this.lastInput = newValue;
	return this.value;
};

function Switch()
{
	this.value = false;
	this.lastInputs = [false, false];
}

Switch.prototype.readInputs = function(newValues) {
	for (var i = 0; i < 2; i++) {
		newValues[i] = convertToBool(newValues[i]);
	}
	if (newValues[0] && !this.lastInputs[0]) {
		this.value = true;
	}
	else if (newValues[1] && !this.lastInputs[1]) {
		this.value = false;
	}
	for (var i = 0; i < 2; i++) {
		this.lastInputs[i] = newValues[i];
	}
	return this.value;
};

function TimedPulse()
{
	this.period = 0; // Off
	this.lastSignal = false;
	this.value = false;
	this.endPulseTime = null;
}

TimedPulse.prototype.readInputs = function(newValues) {
	var signal = convertToBool(newValues[0]);
	var newPeriod = parseInt(newValues[1]);
	this.period = newPeriod > 0 ? newPeriod : 0;

	if (signal && !this.lastSignal && this.period) {
		// Start the timer, to end in this.period millisecs
		this.endPulseTime = Date.now() + this.period;
		this.value = true;
	}
	else if (this.endPulseTime && (Date.now() >= this.endPulseTime)) {
		// Stop the timer
		this.value = false;
		this.endPulseTime = null;
	}

	// Keep note of the signal
	this.lastSignal = signal;

	return this.value;
};

var toggleState = new Toggle();
var switchState = new Switch();
var timedPulseState = new TimedPulse();

module.exports = [
	
	{
		'id':        'toggle-operator',
		'label':     'Toggle',
		'inputs':    1,
		'validate':  function(node) {},
		'transform': function(node)
		{
			return toggleState.readInput(node.inputValues[0]);
		}
	},

	{
		'id':        'switch-operator',
		'label':     'On/Off switch',
		'inputs':    2,
		'validate':  function(node) {},
		'transform': function(node)
		{
			return switchState.readInputs(node.inputValues);
		}
	},

	{
		'id':        'timed-pulse-operator',
		'label':     'Timed pulse',
		'inputs':    2,
		'validate':  function(node) {},
		'transform': function(node)
		{
			return timedPulseState.readInputs(node.inputValues);
		}
	}
];
