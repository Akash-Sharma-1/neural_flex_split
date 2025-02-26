## Thoughts on how it should work : 

__Key Features & Implementation Notes__

Split Communication
	•	The keyboard is fully split, with the left half acting as an I2C slave, responsible for scanning its own key matrix and reporting key states.
	•	The right half serves as the I2C master, polling the left half for key data and then sending the combined input to the PC via USB.

State Management
	•	multiple operational states, including normal typing mode, programming mode, and macro recording.
	•	state transitions using a structured waiting mechanism to avoid glitches -> check if there are better ways ??????

Layer System
	•	Supports multiple layers - have to read more on this ????
	•	Default layer for standard typing
	•	Function layer for shortcuts and special commands.
	•	Numpad layer for efficient number input.

Keyboard Functionality
	•	Debounced key scanning ensures accurate keypress detection - there are better techniques used in qmk core software ??? 
	•	Special key handling allows for advanced features like entering programming mode or recording macros 
	•	I2C communication using packed key state data for efficient data transfer between halves -> people said its not recommended ?? why ? ???? 

Implementation & Expansion Possibilities
	•	Built on the Arduino HID-Project library for keyboard emulation - no support rn ? 
	•	Matrix scanning follows a row-output, column-input (with pull-ups) approach for precise detection.
	•	SIDE_SELECT_PIN to differentiate between left and right halves at boot.
	•	Key state data packed into bytes for minimal I2C overhead.

Potential Enhancements
	•	EEPROM storage for persistent custom keymaps and macros - not recommended from reddit comm - check again 
	•	LED indicators for layer changes and mode feedback -  take ergodox exmaple her 
	•	Power management optimizations for lower energy consumption - enhancement but not required rn 
