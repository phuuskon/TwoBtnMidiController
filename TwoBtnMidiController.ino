
/* 
 *  TwoBtnMidiController.ino
 *  Simple programmable two button midi controller
 *  Copyright (C) 2018 Pekka Huuskonen
 *  
 *  This sketch is for simple programmable two button midi controller capable of sending
 *  MIDI Program Change messages. Notic, this is alpha version with some of the functionality.
 *  See comments in code.
 *  
 *  There are three run modes where buttons operates:
 *  Mode 1: buttons cycle through determined program change numbers, button 2 increases, button 1 decreases 
 *  Mode 2: button 1 send on predefined program change and so does button 2 
 *  Mode 3: button 1 toggles between two predefined program change and so does button 2
 *  
 *  Long press on both buttons enters setup.
 *  Currently you can setup run mode, and midi channel
 *  
 *  TODO:
 *  - Setup for program change parameters in all modes
 *  - Using 7 segment display for showing operation
 *  
 *  DONE:
 *  - Saving and loading setup to/from EEPROM
 *    
 *  Current version uses following arduino libraries:
 *  OneButton (https://github.com/mathertel/OneButton)
 *  Fsm (https://github.com/jonblack/arduino-fsm)
 *  MIDI (https://github.com/FortySevenEffects/arduino_midi_library)
 *  EEPROM (https://www.arduino.cc/en/Reference/EEPROM)
 */
const char* VERSION = "0.1";

#include <OneButton.h>
#include <Fsm.h>
#include <MIDI.h>
#include <EEPROM.h>

// state triggers
#define TO_RUN 1
#define TO_SETUP_MODE 2
#define TO_SETUP_MIDI 3
#define TO_SETUP_MODE1 4
#define TO_SETUP_MODE2 5
#define TO_SETUP_MODE3 6
#define TO_SETUP_EXIT 7
#define TO_SETUP_MODE_START 8

// run modes
// mode one cycles one by one defined pc messages
#define RUN_MODE1 1
// in mode two each button send one pc message
#define RUN_MODE2 2
// in mode three each button toggles between two defined pc messages
#define RUN_MODE3 3

MIDI_CREATE_DEFAULT_INSTANCE();

OneButton btn1(A1, true);
OneButton btn2(A2, true);

// midi setup variables
//int midiChannel;

// mode 1 variables
//const int mode1_lowlimitPC = 0;
//const int mode1_highlimitPC = 16;
int mode1_currentPC;

// mode 2 variables
//int mode2_btn1PC;
//int mode2_btn2PC;

// mode 3 variables
//int mode3_btn1PC1;
//int mode3_btn1PC2;
//int mode3_btn2PC1;
//int mode3_btn2PC2;
int mode3_current_btn1PC;
int mode3_current_btn2PC;

// button states for detecting simultanious presses
int btn1_state;
int btn2_state;

// current run mode
int run_mode;

// states
State state_run(&on_run_enter, NULL, NULL);
State state_setup_mode(&on_setup_mode_enter, NULL, NULL);
State state_setup_midi(&on_setup_midi_enter, NULL, NULL);
State state_setup_mode1(&on_setup_mode1_enter, NULL, NULL);
State state_setup_mode2(&on_setup_mode2_enter, NULL, NULL);
State state_setup_mode3(&on_setup_mode3_enter, NULL, NULL);
State state_setup_exit(&on_setup_exit_enter, NULL, &on_setup_exit_exit);
Fsm main_fsm(&state_run);  

// settings
const int CONFIG_VERSION = 1; // ID of the settings block, change if struct is modified

struct settingsStruct {
  byte sversion;
  byte midiChannel;
  byte mode1_lowlimitPC, mode1_highlimitPC;
  byte mode2_btn1PC, mode2_btn2PC;
  byte mode3_btn1PC1, mode3_btn1PC2, mode3_btn2PC1, mode3_btn2PC2; 
} _settings;

const byte CONFIG_START_ADDRESS = 0;

void setup() {
  // TODO: read setup from EEPROM
  
  Serial.begin(9600);

  // load settings
  readSettings();

  // DEBUG: print settings to serial
  Serial.println("setting version: "+String(_settings.sversion));
  Serial.println("midiChannel: "+String(_settings.midiChannel));
  Serial.println("mode1_lowlimitPC: "+String(_settings.mode1_lowlimitPC));
  Serial.println("mode1_highlimitPC: "+String(_settings.mode1_highlimitPC));
  Serial.println("mode2_btn1PC: "+String(_settings.mode2_btn1PC));
  Serial.println("mode2_btn2PC: "+String(_settings.mode2_btn2PC));
  Serial.println("mode3_btn1PC1: "+String(_settings.mode3_btn1PC1));
  Serial.println("mode3_btn1PC2: "+String(_settings.mode3_btn1PC2));
  Serial.println("mode3_btn2PC1: "+String(_settings.mode3_btn2PC1));
  Serial.println("mode3_btn2PC2: "+String(_settings.mode3_btn2PC2));
  
  // init midi channel
  //midiChannel = 1;
  //MIDI.begin(midiChannel);

  // init mode 1
  mode1_currentPC = _settings.mode1_lowlimitPC;

  // init mode 2
  //mode2_btn1PC = 17;
  //mode2_btn2PC = 18;

  // init mode 3
  //mode3_btn1PC1 = 20;
  //mode3_btn1PC2 = 21;
  //mode3_btn2PC1 = 22;
  //mode3_btn2PC2 = 23;
  mode3_current_btn1PC = _settings.mode3_btn1PC1;
  mode3_current_btn2PC = _settings.mode3_btn2PC1;

  // main_fsm transitions
  main_fsm.add_transition(&state_run, &state_setup_mode, TO_SETUP_MODE, NULL);
  main_fsm.add_transition(&state_setup_mode, &state_setup_midi, TO_SETUP_MIDI, NULL);
  main_fsm.add_transition(&state_setup_mode, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_midi, &state_setup_mode1, TO_SETUP_MODE1, NULL);
  main_fsm.add_transition(&state_setup_midi, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode1, &state_setup_mode2, TO_SETUP_MODE2, NULL);
  main_fsm.add_transition(&state_setup_mode1, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode2, &state_setup_mode3, TO_SETUP_MODE3, NULL);
  main_fsm.add_transition(&state_setup_mode3, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode3, &state_setup_mode, TO_SETUP_MODE_START, NULL);
  main_fsm.add_transition(&state_setup_exit, &state_run, TO_RUN, NULL);
  
  // init run mode
  run_mode = RUN_MODE1;

}

void loop() {
  btn1.tick();
  btn2.tick();
  main_fsm.run_machine();
}

void on_run_enter()
{
  Serial.println("on run enter");
  switch(run_mode)
  {
    case RUN_MODE1:
      btn1.attachClick(cyclePcDown);
      btn2.attachClick(cyclePcUp);
      break;
    case RUN_MODE2:
      btn1.attachClick(sendBtn1PC);
      btn2.attachClick(sendBtn2PC);
      break;
    case RUN_MODE3:
      btn1.attachClick(sendBtn1TogglePC);
      btn2.attachClick(sendBtn2TogglePC);
      break;
    default:
      break;
  }
  btn1.attachLongPressStart(longPressStart1);
  btn2.attachLongPressStart(longPressStart2);
  btn1.attachLongPressStop(longPressFromRun);
  btn2.attachLongPressStop(longPressFromRun);
}

void on_setup_mode_enter()
{
  Serial.println("on_setup_mode_enter");
  btn1.attachClick(modeDown);
  btn2.attachClick(modeUp);
  btn1.attachLongPressStop(longPressFromSetupMode);
  btn2.attachLongPressStop(longPressFromSetupMode);
}

void on_setup_midi_enter()
{
  Serial.println("on_setup_midi_enter");
  btn1.attachClick(midiChnDown);
  btn2.attachClick(midiChnUp);
  btn1.attachLongPressStop(longPressFromSetupMidi);
  btn2.attachLongPressStop(longPressFromSetupMidi);
}

void on_setup_mode1_enter()
{
  // TODO: here we will setup program change low and high limit for cycling through them
  Serial.println("on_setup_mode1_enter");
  btn1.attachLongPressStop(longPressFromSetupMode1);
  btn2.attachLongPressStop(longPressFromSetupMode1);
}

void on_setup_mode2_enter()
{
  // TODO: here we will set program change numbers for btn1 and btn2
  Serial.println("on_setup_mode2_enter");
  btn1.attachLongPressStop(longPressFromSetupMode2);
  btn2.attachLongPressStop(longPressFromSetupMode2);
}

void on_setup_mode3_enter()
{
  // TODO: here we will set toggling two program change numbers for btn1 and btn2
  Serial.println("on_setup_mode3_enter");
  btn1.attachLongPressStop(longPressFromSetupMode3);
  btn2.attachLongPressStop(longPressFromSetupMode3);
}

void on_setup_exit_enter()
{
  // TODO: here we will save setup to EEPROM
  Serial.println("on_setup_exit_enter");
  btn1.attachLongPressStop(longPressToRun);
  btn2.attachLongPressStop(longPressToRun);
}

void on_setup_exit_exit()
{
  Serial.println("on_setup_exit_exit");
  
}

void longPressStart1()
{
  // set btn2 state to pressed
  btn1_state = 1;
}

void longPressStart2()
{
  // set btn2 state to pressed
  btn2_state= 1;
}

void longPressFromRun()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_MODE);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void longPressFromSetupMode()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_EXIT);  
  }
  else if(btn1_state == 0 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_MIDI);
  }
  btn2_state = 0;
  btn1_state = 0;
}

void longPressFromSetupMidi()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_EXIT);
  }
  else if(btn1_state == 0 and btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_MODE1);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void longPressFromSetupMode1()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_EXIT);
  }
  else if(btn1_state == 0 and btn2_state == 1)
  {    
    main_fsm.trigger(TO_SETUP_MODE2);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void longPressFromSetupMode2()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_EXIT);  
  }
  else if(btn1_state == 0 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_MODE3);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void longPressFromSetupMode3()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_EXIT);  
  }
  else if(btn1_state == 0 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_MODE_START);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void longPressToRun()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    // save setting
    saveSettings();
    main_fsm.trigger(TO_RUN);  
  }
  else if(btn1_state == 1 && btn2_state == 0)
  {
    // only button one longpressed, goto run without saving
    main_fsm.trigger(TO_RUN);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void cyclePcDown()
{
  if(mode1_currentPC == _settings.mode1_lowlimitPC)
  {
    mode1_currentPC = _settings.mode1_highlimitPC;
  }
  else 
  {
    mode1_currentPC--;
  }
  Serial.println("send midi pc: "+String(mode1_currentPC)+" ch:"+String(_settings.midiChannel));
  //MIDI.sendProgramChange(mode1_currentPC, midiChannel);
}

void cyclePcUp()
{
  if(mode1_currentPC == _settings.mode1_highlimitPC)
  {
    mode1_currentPC = _settings.mode1_lowlimitPC;
  }
  else 
  {
    mode1_currentPC++;
  }
  Serial.println("send midi pc: "+String(mode1_currentPC)+" ch:"+String(_settings.midiChannel));
  //MIDI.sendProgramChange(mode1_currentPC, midiChannel);
}

void sendBtn1PC()
{
  Serial.println("send midi pc: "+String(_settings.mode2_btn1PC)+" ch:"+String(_settings.midiChannel));
  //MIDI.sendProgramChange(_settings.mode2_btn1PC, midiChannel);
}

void sendBtn2PC()
{
  Serial.println("send midi pc: "+String(_settings.mode2_btn2PC)+" ch:"+String(_settings.midiChannel));
  //MIDI.sendProgramChange(_settings.mode2_btn2PC, midiChannel);
}

void sendBtn1TogglePC()
{
  if(mode3_current_btn1PC == _settings.mode3_btn1PC1)
  {
    mode3_current_btn1PC = _settings.mode3_btn1PC2;
  }
  else
  {
    mode3_current_btn1PC = _settings.mode3_btn1PC1;
  }
  Serial.println("send midi pc: "+String(mode3_current_btn1PC)+" ch:"+String(_settings.midiChannel));
  //MIDI.sendProgramChange(mode3_current_btn1PC, midiChannel);
}

void sendBtn2TogglePC()
{
  if(mode3_current_btn2PC == _settings.mode3_btn2PC1)
  {
    mode3_current_btn2PC = _settings.mode3_btn2PC2;
  }
  else
  {
    mode3_current_btn2PC = _settings.mode3_btn2PC1;
  }
  Serial.println("send midi pc: "+String(mode3_current_btn2PC)+" ch:"+String(_settings.midiChannel));
  //MIDI.sendProgramChange(mode3_current_btn2PC, midiChannel);
}

void modeDown()
{
  if(run_mode == RUN_MODE1)
  {
    run_mode = RUN_MODE3;  
  }
  else if(run_mode > RUN_MODE1)
  {
    run_mode = run_mode - 1;
  }
  Serial.println("run mode set to "+String(run_mode));
}

void modeUp()
{
  if(run_mode == RUN_MODE3)
  {
    run_mode = RUN_MODE1;  
  }
  else if(run_mode < RUN_MODE3)
  {
    run_mode = run_mode + 1;
  }
  Serial.println("run mode set to "+String(run_mode));
}

void midiChnDown()
{
  if(_settings.midiChannel == 1)
  {
    _settings.midiChannel = 16;
  }
  else
  {
    _settings.midiChannel--;
  }
  Serial.println("midi chn set to "+String(_settings.midiChannel));
}

void midiChnUp()
{
  if(_settings.midiChannel == 16)
  {
    _settings.midiChannel = 1;
  }
  else
  {
    _settings.midiChannel++;
  }
  Serial.println("midi chn set to "+String(_settings.midiChannel));
}

void loadDefaultSettings(){
  Serial.println("loading default settings");
  _settings.sversion = CONFIG_VERSION;
  _settings.midiChannel = 1;
  _settings.mode1_lowlimitPC = 0;
  _settings.mode1_highlimitPC = 16; 
  _settings.mode2_btn1PC = 17; 
  _settings.mode2_btn2PC = 18;
  _settings.mode3_btn1PC1 = 19;
  _settings.mode3_btn1PC2 = 20;
  _settings.mode3_btn2PC1 = 21;
  _settings.mode3_btn2PC2 = 22;
}

void saveSettings() 
{
   EEPROM.put(CONFIG_START_ADDRESS, _settings);
   Serial.println("settings saved!");
}

void readSettings()
{
  EEPROM.get(CONFIG_START_ADDRESS, _settings);
  // DEBUG: print settings to serial
  Serial.println("loaded settings:");
  Serial.println("version: "+String(_settings.sversion));
  Serial.println("midiChannel: "+String(_settings.midiChannel));
  Serial.println("mode1_lowlimitPC: "+String(_settings.mode1_lowlimitPC));
  Serial.println("mode1_highlimitPC: "+String(_settings.mode1_highlimitPC));
  Serial.println("mode2_btn1PC: "+String(_settings.mode2_btn1PC));
  Serial.println("mode2_btn2PC: "+String(_settings.mode2_btn2PC));
  Serial.println("mode3_btn1PC1: "+String(_settings.mode3_btn1PC1));
  Serial.println("mode3_btn1PC2: "+String(_settings.mode3_btn1PC2));
  Serial.println("mode3_btn2PC1: "+String(_settings.mode3_btn2PC1));
  Serial.println("mode3_btn2PC2: "+String(_settings.mode3_btn2PC2));
  if (_settings.sversion != CONFIG_VERSION) {
    loadDefaultSettings();
  }
}

