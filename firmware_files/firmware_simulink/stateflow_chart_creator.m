% This file contains MATLAB code to create a Stateflow model for a split keyboard firmware
% The model represents the state machine and key processing logic from the C firmware

% Create a new Simulink model
model_name = 'SplitKeyboardStateflow';
close_system(model_name, 0);  % Close if already open
new_system(model_name);
open_system(model_name);

% Add Stateflow chart to model
chart_pos = [150, 50, 600, 500];
chart = add_block('sflib/Chart', [model_name '/KeyboardController'], 'Position', chart_pos);

% Add inputs to the model
add_block('simulink/Sources/In1', [model_name '/KeyMatrix'], 'Position', [50, 100, 80, 130]);
add_block('simulink/Sources/In1', [model_name '/OtherHalfKeys'], 'Position', [50, 150, 80, 180]);
add_block('simulink/Sources/In1', [model_name '/SpecialKeys'], 'Position', [50, 200, 80, 230]);
add_block('simulink/Sources/In1', [model_name '/Timer'], 'Position', [50, 250, 80, 280]);

% Add outputs to the model
add_block('simulink/Sinks/Out1', [model_name '/KeyReport'], 'Position', [770, 100, 800, 130]);
add_block('simulink/Sinks/Out1', [model_name '/LEDStatus'], 'Position', [770, 150, 800, 180]);
add_block('simulink/Sinks/Out1', [model_name '/CurrentLayer'], 'Position', [770, 200, 800, 230]);

% Connect inputs and outputs to chart
add_line(model_name, 'KeyMatrix/1', 'KeyboardController/1');
add_line(model_name, 'OtherHalfKeys/1', 'KeyboardController/2');
add_line(model_name, 'SpecialKeys/1', 'KeyboardController/3');
add_line(model_name, 'Timer/1', 'KeyboardController/4');
add_line(model_name, 'KeyboardController/1', 'KeyReport/1');
add_line(model_name, 'KeyboardController/2', 'LEDStatus/1');
add_line(model_name, 'KeyboardController/3', 'CurrentLayer/1');

% Get the chart object
rt = sfroot;
model = rt.find('-isa', 'Simulink.BlockDiagram', 'Name', model_name);
chart_obj = model.find('-isa', 'Stateflow.Chart');

% Define chart properties
chart_obj.ChartUpdate = 'DISCRETE';
chart_obj.DiscreteSampleTime = '0.01'; % 10ms scan rate

% Add data to chart
sf_data(chart_obj, 'keyMatrix', 'Input', 'boolean', [24, 1], ''); % 4x6 matrix flattened
sf_data(chart_obj, 'otherHalfKeys', 'Input', 'boolean', [24, 1], '');
sf_data(chart_obj, 'specialKeys', 'Input', 'uint8', [4, 1], ''); % [program, macro, layer, fn]
sf_data(chart_obj, 'timer', 'Input', 'uint32', 1, '');

sf_data(chart_obj, 'keyReport', 'Output', 'uint8', [6, 1], '');
sf_data(chart_obj, 'ledStatus', 'Output', 'uint8', 1, '');
sf_data(chart_obj, 'currentLayer', 'Output', 'uint8', 1, '');

sf_data(chart_obj, 'pressedKeyCount', 'Local', 'uint8', 1, '0');
sf_data(chart_obj, 'programSrcKey', 'Local', 'uint8', 1, '0');
sf_data(chart_obj, 'recordingMacro', 'Local', 'boolean', 1, 'false');
sf_data(chart_obj, 'nextState', 'Local', 'uint8', 1, '0');
sf_data(chart_obj, 'debounceTime', 'Constant', 'uint32', 1, '20');
sf_data(chart_obj, 'layerDefault', 'Constant', 'uint8', 1, '0');
sf_data(chart_obj, 'layerFn', 'Constant', 'uint8', 1, '1');
sf_data(chart_obj, 'layerNumpad', 'Constant', 'uint8', 1, '2');

% Create the main states
normal_state = Stateflow.State(chart_obj);
normal_state.Name = 'STATE_NORMAL';
normal_state.Position = [150, 100, 150, 100];

waiting_state = Stateflow.State(chart_obj);
waiting_state.Name = 'STATE_WAITING';
waiting_state.Position = [150, 250, 150, 100];

programming_src_state = Stateflow.State(chart_obj);
programming_src_state.Name = 'STATE_PROGRAMMING_SRC';
programming_src_state.Position = [350, 100, 150, 100];

programming_dst_state = Stateflow.State(chart_obj);
programming_dst_state.Name = 'STATE_PROGRAMMING_DST';
programming_dst_state.Position = [350, 250, 150, 100];

macro_record_trigger_state = Stateflow.State(chart_obj);
macro_record_trigger_state.Name = 'STATE_MACRO_RECORD_TRIGGER';
macro_record_trigger_state.Position = [150, 400, 150, 100];

macro_record_state = Stateflow.State(chart_obj);
macro_record_state.Name = 'STATE_MACRO_RECORD';
macro_record_state.Position = [350, 400, 150, 100];

macro_play_state = Stateflow.State(chart_obj);
macro_play_state.Name = 'STATE_MACRO_PLAY';
macro_play_state.Position = [550, 400, 150, 100];

% Add default transition to normal state
default_transition = Stateflow.Transition(chart_obj);
default_transition.Source = chart_obj;
default_transition.Destination = normal_state;
default_transition.SourceOClock = 9;
default_transition.DestinationOClock = 9;

% Add transitions between states

% Normal state transitions
transition_normal_to_waiting1 = Stateflow.Transition(chart_obj);
transition_normal_to_waiting1.Source = normal_state;
transition_normal_to_waiting1.Destination = waiting_state;
transition_normal_to_waiting1.SourceOClock = 6;
transition_normal_to_waiting1.DestinationOClock = 12;
transition_normal_to_waiting1.LabelString = 'specialKeys(1) && specialKeys(2) \n{nextState = 4; /* MACRO_RECORD_TRIGGER */}';

transition_normal_to_waiting2 = Stateflow.Transition(chart_obj);
transition_normal_to_waiting2.Source = normal_state;
transition_normal_to_waiting2.Destination = waiting_state;
transition_normal_to_waiting2.SourceOClock = 4;
transition_normal_to_waiting2.DestinationOClock = 10;
transition_normal_to_waiting2.LabelString = 'specialKeys(1) && ~specialKeys(2) \n{nextState = 2; /* PROGRAMMING_SRC */}';

% Waiting state transitions
transition_waiting_to_next = Stateflow.Transition(chart_obj);
transition_waiting_to_next.Source = waiting_state;
transition_waiting_to_next.Destination = chart_obj;
transition_waiting_to_next.SourceOClock = 0;
transition_waiting_to_next.DestinationOClock = 7;
transition_waiting_to_next.LabelString = 'pressedKeyCount == 0';
transition_waiting_to_next.ExecutionOrder = 1;

% Conditional junction for waiting state
junction1 = Stateflow.Junction(chart_obj);
junction1.Position = [550, 300, 10, 10];

connective_waiting_to_junction = Stateflow.Connective(chart_obj);
connective_waiting_to_junction.Source = transition_waiting_to_next;
connective_waiting_to_junction.Destination = junction1;

% Junction to programming src
junction_to_prog_src = Stateflow.Transition(chart_obj);
junction_to_prog_src.Source = junction1;
junction_to_prog_src.Destination = programming_src_state;
junction_to_prog_src.LabelString = 'nextState == 2';

% Junction to programming dst
junction_to_prog_dst = Stateflow.Transition(chart_obj);
junction_to_prog_dst.Source = junction1;
junction_to_prog_dst.Destination = programming_dst_state;
junction_to_prog_dst.LabelString = 'nextState == 3';

% Junction to macro record trigger
junction_to_macro_trigger = Stateflow.Transition(chart_obj);
junction_to_macro_trigger.Source = junction1;
junction_to_macro_trigger.Destination = macro_record_trigger_state;
junction_to_macro_trigger.LabelString = 'nextState == 4';

% Junction to macro record
junction_to_macro_record = Stateflow.Transition(chart_obj);
junction_to_macro_record.Source = junction1;
junction_to_macro_record.Destination = macro_record_state;
junction_to_macro_record.LabelString = 'nextState == 5';

% Junction to normal state (default)
junction_to_normal = Stateflow.Transition(chart_obj);
junction_to_normal.Source = junction1;
junction_to_normal.Destination = normal_state;
junction_to_normal.LabelString = 'nextState == 0 || nextState > 6';

% Programming src state transitions
transition_prog_src_to_waiting = Stateflow.Transition(chart_obj);
transition_prog_src_to_waiting.Source = programming_src_state;
transition_prog_src_to_waiting.Destination = waiting_state;
transition_prog_src_to_waiting.LabelString = 'pressedKeyCount == 1 \n{programSrcKey = getKeycode(); nextState = 3;}';

transition_prog_src_to_normal = Stateflow.Transition(chart_obj);
transition_prog_src_to_normal.Source = programming_src_state;
transition_prog_src_to_normal.Destination = normal_state;
transition_prog_src_to_normal.LabelString = 'specialKeys(1) && specialKeys(3) \n/* PROGRAM + FN keys */';

% Programming dst state transitions
transition_prog_dst_to_waiting = Stateflow.Transition(chart_obj);
transition_prog_dst_to_waiting.Source = programming_dst_state;
transition_prog_dst_to_waiting.Destination = waiting_state;
transition_prog_dst_to_waiting.LabelString = 'pressedKeyCount == 1 \n{/* Remap key */ nextState = 2;}';

transition_prog_dst_to_normal = Stateflow.Transition(chart_obj);
transition_prog_dst_to_normal.Source = programming_dst_state;
transition_prog_dst_to_normal.Destination = normal_state;
transition_prog_dst_to_normal.LabelString = 'specialKeys(1) && specialKeys(3) \n/* PROGRAM + FN keys */';

% Macro record trigger state transitions
transition_macro_trigger_to_waiting = Stateflow.Transition(chart_obj);
transition_macro_trigger_to_waiting.Source = macro_record_trigger_state;
transition_macro_trigger_to_waiting.Destination = waiting_state;
transition_macro_trigger_to_waiting.LabelString = 'pressedKeyCount > 0 \n{/* Save trigger */ nextState = 5;}';

transition_macro_trigger_to_normal = Stateflow.Transition(chart_obj);
transition_macro_trigger_to_normal.Source = macro_record_trigger_state;
transition_macro_trigger_to_normal.Destination = normal_state;
transition_macro_trigger_to_normal.LabelString = 'specialKeys(1) && specialKeys(2) \n/* PROGRAM + MACRO keys */';

% Macro record state transitions
entry_action_macro_record = 'if ~recordingMacro\n  recordingMacro = true;\n  /* Initialize macro recording */\nend';
macro_record_state.Entry = entry_action_macro_record;

transition_macro_record_to_waiting = Stateflow.Transition(chart_obj);
transition_macro_record_to_waiting.Source = macro_record_state;
transition_macro_record_to_waiting.Destination = waiting_state;
transition_macro_record_to_waiting.LabelString = 'specialKeys(1) && specialKeys(2) \n{recordingMacro = false; /* Save macro */ nextState = 0;}';

% Add state actions
normal_state_during = ['/* Process keys in normal mode */\n', ...
                      'processKeys();\n', ...
                      'updateLEDs();\n', ...
                      'countPressedKeys();\n', ...
                      'currentLayer = calculateActiveLayer(specialKeys);'];
normal_state.During = normal_state_during;

programming_src_state_during = ['/* Update LEDs to indicate programming source mode */\n', ...
                               'ledStatus = getLEDStatus(2);\n', ...
                               'countPressedKeys();'];
programming_src_state.During = programming_src_state_during;

programming_dst_state_during = ['/* Update LEDs to indicate programming destination mode */\n', ...
                               'ledStatus = getLEDStatus(3);\n', ...
                               'countPressedKeys();'];
programming_dst_state.During = programming_dst_state_during;

waiting_state_during = ['/* Update LEDs based on next state */\n', ...
                       'ledStatus = getLEDStatus(1);\n', ...
                       'countPressedKeys();'];
waiting_state.During = waiting_state_during;

macro_record_trigger_state_during = ['/* Update LEDs to indicate macro trigger recording */\n', ...
                                    'ledStatus = getLEDStatus(4);\n', ...
                                    'countPressedKeys();'];
macro_record_trigger_state.During = macro_record_trigger_state_during;

macro_record_state_during = ['/* Process and record keys */\n', ...
                            'processKeys();\n', ...
                            'recordMacroKeys();\n', ...
                            'ledStatus = getLEDStatus(5);\n', ...
                            'countPressedKeys();'];
macro_record_state.During = macro_record_state_during;

macro_play_state_during = ['/* Play back macro */\n', ...
                          'if (playNextMacroKey())\n', ...
                          '  /* More keys to play */\n', ...
                          'else\n', ...
                          '  nextState = 0;\n', ...
                          '  /* Transition to waiting then normal */\n', ...
                          'end'];
macro_play_state.During = macro_play_state_during;

% Add functions to chart
countKeys_func = ['function countPressedKeys()\n', ...
                 '  count = 0;\n', ...
                 '  for i = 1:24\n', ...
                 '    if keyMatrix(i)\n', ...
                 '      count = count + 1;\n', ...
                 '    end\n', ...
                 '    if otherHalfKeys(i)\n', ...
                 '      count = count + 1;\n', ...
                 '    end\n', ...
                 '  end\n', ...
                 '  pressedKeyCount = count;\n', ...
                 'end'];

processKeys_func = ['function processKeys()\n', ...
                   '  /* Clear key report */\n', ...
                   '  for i = 1:6\n', ...
                   '    keyReport(i) = 0;\n', ...
                   '  end\n', ...
                   '  \n', ...
                   '  /* Generate key report based on current layer */\n', ...
                   '  reportIndex = 1;\n', ...
                   '  \n', ...
                   '  /* First process this side''s keys */\n', ...
                   '  for i = 1:24\n', ...
                   '    if keyMatrix(i) && reportIndex <= 6\n', ...
                   '      keycode = getKeymapCode(currentLayer, i);\n', ...
                   '      if keycode > 0 && keycode < 240 /* Not a special command */\n', ...
                   '        keyReport(reportIndex) = keycode;\n', ...
                   '        reportIndex = reportIndex + 1;\n', ...
                   '      end\n', ...
                   '    end\n', ...
                   '  end\n', ...
                   '  \n', ...
                   '  /* Then process other half''s keys */\n', ...
                   '  for i = 1:24\n', ...
                   '    if otherHalfKeys(i) && reportIndex <= 6\n', ...
                   '      keycode = getKeymapCode(currentLayer, i+24); /* Offset for other half */\n', ...
                   '      if keycode > 0 && keycode < 240 /* Not a special command */\n', ...
                   '        keyReport(reportIndex) = keycode;\n', ...
                   '        reportIndex = reportIndex + 1;\n', ...
                   '      end\n', ...
                   '    end\n', ...
                   '  end\n', ...
                   'end'];

getLEDStatus_func = ['function status = getLEDStatus(state)\n', ...
                    '  /* Return LED status based on current state */\n', ...
                    '  switch state\n', ...
                    '    case 0 /* Normal */\n', ...
                    '      status = 1;\n', ...
                    '    case 1 /* Waiting */\n', ...
                    '      status = 2;\n', ...
                    '    case 2 /* Programming_src */\n', ...
                    '      /* Flash LED every 128ms */\n', ...
                    '      if mod(timer, 128) < 64\n', ...
                    '        status = 4;\n', ...
                    '      else\n', ...
                    '        status = 0;\n', ...
                    '      end\n', ...
                    '    case 3 /* Programming_dst */\n', ...
                    '      /* Flash LED every 256ms */\n', ...
                    '      if mod(timer, 256) < 128\n', ...
                    '        status = 8;\n', ...
                    '      else\n', ...
                    '        status = 0;\n', ...
                    '      end\n', ...
                    '    case 4 /* Macro_record_trigger */\n', ...
                    '      /* Flash LED every 128ms */\n', ...
                    '      if mod(timer, 128) < 64\n', ...
                    '        status = 16;\n', ...
                    '      else\n', ...
                    '        status = 0;\n', ...
                    '      end\n', ...
                    '    case 5 /* Macro_record */\n', ...
                    '      /* Flash LED every 128ms */\n', ...
                    '      if mod(timer, 128) < 64\n', ...
                    '        status = 32;\n', ...
                    '      else\n', ...
                    '        status = 0;\n', ...
                    '      end\n', ...
                    '    case 6 /* Macro_play */\n', ...
                    '      status = 64;\n', ...
                    '    otherwise\n', ...
                    '      status = 0;\n', ...
                    '  end\n', ...
                    'end'];

calculateLayer_func = ['function layer = calculateActiveLayer(special)\n', ...
                      '  if special(3) /* Layer key */\n', ...
                      '    if currentLayer == layerDefault\n', ...
                      '      layer = layerFn;\n', ...
                      '    else\n', ...
                      '      layer = layerDefault;\n', ...
                      '    end\n', ...
                      '  else\n', ...
                      '    /* No change */\n', ...
                      '    layer = currentLayer;\n', ...
                      '  end\n', ...
                      'end'];

getKeycode_func = ['function code = getKeycode()\n', ...
                  '  /* Find first pressed key and return its keycode */\n', ...
                  '  code = 0;\n', ...
                  '  for i = 1:24\n', ...
                  '    if keyMatrix(i)\n', ...
                  '      code = getKeymapCode(currentLayer, i);\n', ...
                  '      return;\n', ...
                  '    end\n', ...
                  '  end\n', ...
                  '  for i = 1:24\n', ...
                  '    if otherHalfKeys(i)\n', ...
                  '      code = getKeymapCode(currentLayer, i+24);\n', ...
                  '      return;\n', ...
                  '    end\n', ...
                  '  end\n', ...
                  'end'];

getKeymapCode_func = ['function code = getKeymapCode(layer, index)\n', ...
                     '  /* Simple lookup function - would use actual keymap in real implementation */\n', ...
                     '  /* This is a placeholder */\n', ...
                     '  if layer == 0 /* Default layer */\n', ...
                     '    codes = [97:122 48:57]; /* a-z, 0-9 */\n', ...
                     '    if index <= length(codes)\n', ...
                     '      code = codes(index);\n', ...
                     '    else\n', ...
                     '      code = 0;\n', ...
                     '    end\n', ...
                     '  elseif layer == 1 /* Function layer */\n', ...
                     '    codes = [112:123 49:57]; /* F1-F12, 1-9 */\n', ...
                     '    if index <= length(codes)\n', ...
                     '      code = codes(index);\n', ...
                     '    else\n', ...
                     '      code = 0;\n', ...
                     '    end\n', ...
                     '  else /* Numpad layer */\n', ...
                     '    codes = [96:105 110 111]; /* Numpad 0-9, decimal, divide */\n', ...
                     '    if index <= length(codes)\n', ...
                     '      code = codes(index);\n', ...
                     '    else\n', ...
                     '      code = 0;\n', ...
                     '    end\n', ...
                     '  end\n', ...
                     'end'];

recordMacroKeys_func = ['function recordMacroKeys()\n', ...
                       '  /* This would record pressed keys to macro buffer */\n', ...
                       '  /* Simplified placeholder implementation */\n', ...
                       'end'];

playNextMacroKey_func = ['function hasMore = playNextMacroKey()\n', ...
                        '  /* This would play the next key from macro buffer */\n', ...
                        '  /* Returns true if more keys to play, false if done */\n', ...
                        '  /* Simplified placeholder implementation */\n', ...
                        '  static macroIndex = 0;\n', ...
                        '  macroIndex = macroIndex + 1;\n', ...
                        '  if macroIndex < 10 /* Arbitrary demo value */\n', ...
                        '    hasMore = true;\n', ...
                        '  else\n', ...
                        '    hasMore = false;\n', ...
                        '    macroIndex = 0;\n', ...
                        '  end\n', ...
                        'end'];

% Add all functions to chart
chart_obj.Script = [countKeys_func, newline, newline, ...
                   processKeys_func, newline, newline, ...
                   getLEDStatus_func, newline, newline, ...
                   calculateLayer_func, newline, newline, ...
                   getKeycode_func, newline, newline, ...
                   getKeymapCode_func, newline, newline, ...
                   recordMacroKeys_func, newline, newline, ...
                   playNextMacroKey_func];

% Layout the chart for better readability
chart_obj.layoutChart;

% Save the model
save_system(model_name);
