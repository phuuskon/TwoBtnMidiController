
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
 *  - Setup for program change parameters in mode 3
 *  - Fix displaying text in save settings
 *  
 *  DONE:
 *  - Saving and loading setup to/from EEPROM
 *  - Using 7 segment display for showing operation
 *  - Setup for pc parameters in mode 1 and 2
 *    
 *  Current version uses following arduino libraries:
 *  OneButton (https://github.com/mathertel/OneButton)
 *  Fsm (https://github.com/jonblack/arduino-fsm)
 *  MIDI (https://github.com/FortySevenEffects/arduino_midi_library)
 *  EEPROM (https://www.arduino.cc/en/Reference/EEPROM)
 *  SevSeg (https://github.com/DeanIsMe/SevSeg)
 */
const String VERSION = "0.1";
const int INIT_SHOWTIME = 3000;

#include <OneButton.h>
#include <Fsm.h>
#include <MIDI.h>
#include <EEPROM.h>
#include <SevSeg.h>

// state triggers
#define TO_RUN 1
#define TO_SETUP_MODE 2
#define TO_SETUP_MIDI 3
#define TO_SETUP_MODE1 4
#define TO_SETUP_MODE1_LOW 4
#define TO_SETUP_MODE1_HI 5
#define TO_SETUP_MODE2 6
#define TO_SETUP_MODE2_BTN1 7
#define TO_SETUP_MODE2_BTN2 8
#define TO_SETUP_MODE3 9
#define TO_SETUP_MODE3_BTN1_PC1 10 
#define TO_SETUP_MODE3_BTN1_PC2 11
#define TO_SETUP_MODE3_BTN2_PC1 12
#define TO_SETUP_MODE3_BTN2_PC2 13
#define TO_SETUP_EXIT 14
#define TO_SETUP_MODE_START 15

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


// mode 1 variables
int mode1_currentPC;

// mode 3 variables
int mode3_current_btn1PC;
int mode3_current_btn2PC;

// button states for detecting simultanious presses
int btn1_state;
int btn2_state;

// current run mode
int run_mode;

// states
State state_init(&on_init_enter, NULL, NULL);
State state_run(&on_run_enter, NULL, NULL);
State state_setup_mode(&on_setup_mode_enter, NULL, NULL);
State state_setup_midi(&on_setup_midi_enter, NULL, NULL);
State state_setup_mode1(&on_setup_mode1_enter, NULL, NULL);

// mode 1 setup states
State state_setup_mode1_low(&on_setup_mode1_low_enter, NULL, NULL);
State state_setup_mode1_hi(&on_setup_mode1_hi_enter, NULL, NULL);

// mode 2 setup states
State state_setup_mode2_btn1(&on_setup_mode2_btn1_enter, NULL, NULL);
State state_setup_mode2_btn2(&on_setup_mode2_btn2_enter, NULL, NULL);

// mode 3 setup states
State state_setup_mode3_btn1PC1(&on_setup_mode3_btn1PC1_enter, NULL, NULL);
State state_setup_mode3_btn1PC2(&on_setup_mode3_btn1PC2_enter, NULL, NULL);
State state_setup_mode3_btn2PC1(&on_setup_mode3_btn2PC1_enter, NULL, NULL);
State state_setup_mode3_btn2PC2(&on_setup_mode3_btn2PC2_enter, NULL, NULL);

State state_setup_mode2(&on_setup_mode2_enter, NULL, NULL);
State state_setup_mode3(&on_setup_mode3_enter, NULL, NULL);
State state_setup_exit(&on_setup_exit_enter, NULL, &on_setup_exit_exit);
Fsm main_fsm(&state_init);  

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

// display
SevSeg segDisp;

void setup() {
  //Serial.begin(9600);

  // load settings
  readSettings();

  // DEBUG: print settings to serial
  /*
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
  */
  
  // midi begin
  MIDI.begin(_settings.midiChannel);

  // init mode 1
  mode1_currentPC = _settings.mode1_lowlimitPC;

  // init mode 3
  mode3_current_btn1PC = _settings.mode3_btn1PC1;
  mode3_current_btn2PC = _settings.mode3_btn2PC1;

  // main_fsm transitions
  main_fsm.add_timed_transition(&state_init, &state_run, INIT_SHOWTIME, NULL);
  main_fsm.add_transition(&state_run, &state_setup_mode, TO_SETUP_MODE, NULL);
  main_fsm.add_transition(&state_setup_mode, &state_setup_midi, TO_SETUP_MIDI, NULL);
  main_fsm.add_transition(&state_setup_mode, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_midi, &state_setup_mode1, TO_SETUP_MODE1, NULL);
  main_fsm.add_transition(&state_setup_midi, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode1, &state_setup_mode2, TO_SETUP_MODE2, NULL);
  main_fsm.add_transition(&state_setup_mode1, &state_setup_mode1_low, TO_SETUP_MODE1_LOW, NULL);
  main_fsm.add_transition(&state_setup_mode1_low, &state_setup_mode1_hi, TO_SETUP_MODE1_HI, NULL);
  main_fsm.add_transition(&state_setup_mode1_low, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode1_hi, &state_setup_mode2, TO_SETUP_MODE2, NULL);
  main_fsm.add_transition(&state_setup_mode1_hi, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode1, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode2, &state_setup_mode3, TO_SETUP_MODE3, NULL);
  main_fsm.add_transition(&state_setup_mode2, &state_setup_mode2_btn1, TO_SETUP_MODE2_BTN1, NULL);
  main_fsm.add_transition(&state_setup_mode2_btn1, &state_setup_mode2_btn2, TO_SETUP_MODE2_BTN2, NULL);
  main_fsm.add_transition(&state_setup_mode2_btn1, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode2_btn2, &state_setup_mode3, TO_SETUP_MODE3, NULL);
  main_fsm.add_transition(&state_setup_mode2_btn2, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode3, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_mode3, &state_setup_mode, TO_SETUP_MODE_START, NULL);
  main_fsm.add_transition(&state_setup_mode3, &state_setup_mode3_btn1PC1, TO_SETUP_MODE3_BTN1_PC1, NULL);
  main_fsm.add_transition(&state_setup_mode3_btn1PC1, &state_setup_mode3_btn1PC2, TO_SETUP_MODE3_BTN1_PC2, NULL);
  main_fsm.add_transition(&state_setup_mode3_btn1PC2, &state_setup_mode3_btn2PC1, TO_SETUP_MODE3_BTN2_PC1, NULL);
  main_fsm.add_transition(&state_setup_mode3_btn2PC1, &state_setup_mode3_btn2PC2, TO_SETUP_MODE3_BTN2_PC2, NULL);
  main_fsm.add_transition(&state_setup_mode3_btn2PC2, &state_setup_exit, TO_SETUP_EXIT, NULL);
  main_fsm.add_transition(&state_setup_exit, &state_run, TO_RUN, NULL);
  
  // init run mode
  run_mode = RUN_MODE1;

  // set display
  setDisplay();
}

void loop() {
  btn1.tick();
  btn2.tick();
  main_fsm.run_machine();
  segDisp.refreshDisplay();
}

void on_init_enter()
{
  float showversion = VERSION.toFloat();
  int ishowversion = showversion * 10;
  segDisp.setNumber(ishowversion,1);
}

void on_run_enter()
{
  //Serial.println("on run enter");
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
  updateDisplay("op",run_mode);
      
  btn1.attachLongPressStart(longPressStart1);
  btn2.attachLongPressStart(longPressStart2);
  btn1.attachLongPressStop(longPressFromRun);
  btn2.attachLongPressStop(longPressFromRun);
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
  updateDisplay("PC",mode1_currentPC);
  //Serial.println("send midi pc: "+String(mode1_currentPC)+" ch:"+String(_settings.midiChannel));
  MIDI.sendProgramChange(mode1_currentPC, _settings.midiChannel);
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
  updateDisplay("PC",mode1_currentPC);
  //Serial.println("send midi pc: "+String(mode1_currentPC)+" ch:"+String(_settings.midiChannel));
  MIDI.sendProgramChange(mode1_currentPC, _settings.midiChannel);
}

void sendBtn1PC()
{
  updateDisplay("PC",_settings.mode2_btn1PC);
  //Serial.println("send midi pc: "+String(_settings.mode2_btn1PC)+" ch:"+String(_settings.midiChannel));
  MIDI.sendProgramChange(_settings.mode2_btn1PC, _settings.midiChannel);
}

void sendBtn2PC()
{
  updateDisplay("PC",_settings.mode2_btn2PC);
  //Serial.println("send midi pc: "+String(_settings.mode2_btn2PC)+" ch:"+String(_settings.midiChannel));
  MIDI.sendProgramChange(_settings.mode2_btn2PC, _settings.midiChannel);
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
  updateDisplay("PC",mode3_current_btn1PC);
  //Serial.println("send midi pc: "+String(mode3_current_btn1PC)+" ch:"+String(_settings.midiChannel));
  MIDI.sendProgramChange(mode3_current_btn1PC, _settings.midiChannel);
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
  updateDisplay("PC",mode3_current_btn2PC);
  //Serial.println("send midi pc: "+String(mode3_current_btn2PC)+" ch:"+String(_settings.midiChannel));
  MIDI.sendProgramChange(mode3_current_btn2PC, _settings.midiChannel);
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

void on_setup_mode_enter()
{
  //Serial.println("on_setup_mode_enter");
  btn1.attachClick(modeDown);
  btn2.attachClick(modeUp);
  btn1.attachLongPressStop(longPressFromSetupMode);
  btn2.attachLongPressStop(longPressFromSetupMode);
  updateDisplay("s op");
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
  updateDisplay("op",run_mode);
  //Serial.println("run mode set to "+String(run_mode));
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
  updateDisplay("op",run_mode);
  //Serial.println("run mode set to "+String(run_mode));
}

void longPressFromSetupMode()
{
  if(btn1_state == 1 && btn2_state == 0)
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

void on_setup_midi_enter()
{
  //Serial.println("on_setup_midi_enter");
  btn1.attachClick(midiChnDown);
  btn2.attachClick(midiChnUp);
  btn1.attachLongPressStop(longPressFromSetupMidi);
  btn2.attachLongPressStop(longPressFromSetupMidi);
  updateDisplay("s nd");
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
  updateDisplay("ch",_settings.midiChannel);
  //Serial.println("midi chn set to "+String(_settings.midiChannel));
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
  updateDisplay("ch",_settings.midiChannel);
  //Serial.println("midi chn set to "+String(_settings.midiChannel));
}

void longPressFromSetupMidi()
{
  if(btn1_state == 1 && btn2_state == 0)
  {
    main_fsm.trigger(TO_SETUP_EXIT);
  }
  else if(btn1_state == 0 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_MODE1);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void on_setup_mode1_enter()
{
  // TODO: here we will setup program change low and high limit for cycling through them
  //Serial.println("on_setup_mode1_enter");
  btn1.attachClick(nullClick);
  btn2.attachClick(nullClick);
  btn1.attachLongPressStop(longPressFromSetupMode1);
  btn2.attachLongPressStop(longPressFromSetupMode1);
  updateDisplay("s n1");
}

void longPressFromSetupMode1()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_MODE1_LOW);
  }
  else if(btn1_state == 0 && btn2_state == 1)
  {    
    main_fsm.trigger(TO_SETUP_MODE2);  
  }
  else if(btn1_state == 1 && btn2_state == 0)
  {
    main_fsm.trigger(TO_SETUP_EXIT);
  }
  btn2_state = 0;
  btn1_state = 0;
}

void on_setup_mode1_low_enter()
{
  btn1.attachClick(mode1LowLimitDecrease);
  btn2.attachClick(mode1LowLimitIncrease);
  btn1.attachLongPressStop(longPressFromSetupMode1Low);
  btn2.attachLongPressStop(longPressFromSetupMode1Low);
  updateDisplay("S LO");
}

void mode1LowLimitDecrease()
{
  if(_settings.mode1_lowlimitPC == 0)
  {
    _settings.mode1_lowlimitPC = 127;
  }
  else
  {
    _settings.mode1_lowlimitPC--;
  }
  updateDisplay("PC",_settings.mode1_lowlimitPC);
  //Serial.println("mode1 lowlimit set to "+String(_settings.mode1_lowlimitPC));
}

void mode1LowLimitIncrease()
{
  if(_settings.mode1_lowlimitPC == 127)
  {
    _settings.mode1_lowlimitPC = 0;
  }
  else
  {
    _settings.mode1_lowlimitPC++;
  }
  updateDisplay("PC",_settings.mode1_lowlimitPC);
  //Serial.println("mode1 lowlimit set to "+String(_settings.mode1_lowlimitPC));
}

void longPressFromSetupMode1Low()
{
  if(btn1_state == 1 && btn2_state == 0)
  {
    main_fsm.trigger(TO_SETUP_EXIT);
  }
  else if(btn1_state == 0 && btn2_state == 1)
  {    
    main_fsm.trigger(TO_SETUP_MODE1_HI);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void on_setup_mode1_hi_enter()
{
  btn1.attachClick(mode1HiLimitDecrease);
  btn2.attachClick(mode1HiLimitIncrease);
  btn1.attachLongPressStop(longPressFromSetupMode1Hi); 
  updateDisplay("S HI"); 
}

void mode1HiLimitDecrease()
{
  if(_settings.mode1_highlimitPC == 0)
  {
    _settings.mode1_highlimitPC = 127;
  }
  else
  {
    _settings.mode1_highlimitPC--;
  }
  updateDisplay("PC",_settings.mode1_highlimitPC);
  //Serial.println("mode1 lowlimit set to "+String(_settings.mode1_highlimitPC));
}

void mode1HiLimitIncrease()
{
  if(_settings.mode1_highlimitPC == 127)
  {
    _settings.mode1_highlimitPC = 0;
  }
  else
  {
    _settings.mode1_highlimitPC++;
  }
  updateDisplay("PC",_settings.mode1_highlimitPC);
  //Serial.println("mode1 lowlimit set to "+String(_settings.mode1_highlimitPC));
}

void longPressFromSetupMode1Hi()
{
  if(btn1_state == 1 && btn2_state == 0)
  {
    main_fsm.trigger(TO_SETUP_EXIT);
  }
  else if(btn1_state == 1 && btn2_state == 1)
  {    
    main_fsm.trigger(TO_SETUP_MODE2);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void on_setup_mode2_enter()
{
  // TODO: here we will set program change numbers for btn1 and btn2
  //Serial.println("on_setup_mode2_enter");
  btn1.attachClick(nullClick);
  btn2.attachClick(nullClick);
  btn1.attachLongPressStop(longPressFromSetupMode2);
  btn2.attachLongPressStop(longPressFromSetupMode2);
  updateDisplay("s n2");
}

void longPressFromSetupMode2()
{
  if(btn1_state == 1 && btn2_state == 0)
  {
    main_fsm.trigger(TO_SETUP_EXIT);  
  }
  else if(btn1_state == 1 && btn2_state == 1)
  {
    main_fsm.trigger(TO_SETUP_MODE2_BTN1);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void on_setup_mode2_btn1_enter()
{
  btn1.attachClick(mode2Btn1Decrease);
  btn2.attachClick(mode2Btn1Increase);
  btn1.attachLongPressStop(longPressFromSetupMode2Btn1);
  btn2.attachLongPressStop(longPressFromSetupMode2Btn1);
  updateDisplay("s b1");
}

void mode2Btn1Decrease()
{
  if(_settings.mode2_btn1PC == 0)
  {
    _settings.mode2_btn1PC = 127;
  }
  else
  {
    _settings.mode2_btn1PC--;
  }
  updateDisplay("PC",_settings.mode2_btn1PC);
  //Serial.println("mode2 btn1 set to "+String(_settings.mode2_btn1PC));
}

void mode2Btn1Increase()
{
  if(_settings.mode2_btn1PC == 127)
  {
    _settings.mode2_btn1PC = 0;
  }
  else
  {
    _settings.mode2_btn1PC++;
  }
  updateDisplay("PC",_settings.mode2_btn1PC);
  //Serial.println("mode2 btn1 set to "+String(_settings.mode2_btn1PC));
}

void longPressFromSetupMode2Btn1()
{
  if(btn1_state == 1 && btn2_state == 0)
  {
    main_fsm.trigger(TO_SETUP_EXIT);
  }
  else if(btn1_state == 1 && btn2_state == 1)
  {    
    main_fsm.trigger(TO_SETUP_MODE2_BTN2);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void on_setup_mode2_btn2_enter()
{
  btn1.attachClick(mode2Btn2Decrease);
  btn2.attachClick(mode2Btn2Increase);
  btn1.attachLongPressStop(longPressFromSetupMode2Btn2);
  btn2.attachLongPressStop(longPressFromSetupMode2Btn2);
  updateDisplay("s b2");
}

void mode2Btn2Decrease()
{
  if(_settings.mode2_btn2PC == 0)
  {
    _settings.mode2_btn2PC = 127;
  }
  else
  {
    _settings.mode2_btn2PC--;
  }
  updateDisplay("PC",_settings.mode2_btn2PC);
  //Serial.println("mode2 btn2 set to "+String(_settings.mode2_btn2PC));
}

void mode2Btn2Increase()
{
  if(_settings.mode2_btn2PC == 127)
  {
    _settings.mode2_btn2PC = 0;
  }
  else
  {
    _settings.mode2_btn2PC++;
  }
  updateDisplay("PC",_settings.mode2_btn2PC);
  //Serial.println("mode2 btn2 set to "+String(_settings.mode2_btn2PC));
}

void longPressFromSetupMode2Btn2()
{
  if(btn1_state == 1 && btn2_state == 0)
  {
    main_fsm.trigger(TO_SETUP_EXIT);
  }
  else if(btn1_state == 1 && btn2_state == 1)
  {    
    main_fsm.trigger(TO_SETUP_MODE3);  
  }
  btn2_state = 0;
  btn1_state = 0;
}


void on_setup_mode3_enter()
{
  // TODO: here we will set toggling two program change numbers for btn1 and btn2
  //Serial.println("on_setup_mode3_enter");
  btn1.attachLongPressStop(longPressFromSetupMode3);
  btn2.attachLongPressStop(longPressFromSetupMode3);
  updateDisplay("s n3");
}

void longPressFromSetupMode3()
{
  if(btn1_state == 1 && btn2_state == 0)
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

void on_setup_mode3_btn1PC1_enter()
{
  
}

void on_setup_mode3_btn1PC2_enter()
{
  
}

void on_setup_mode3_btn2PC1_enter()
{
  
}

void on_setup_mode3_btn2PC2_enter()
{
  
}

void on_setup_exit_enter()
{
  //Serial.println("on_setup_exit_enter");
  btn1.attachLongPressStop(longPressToRun);
  btn2.attachLongPressStop(longPressToRun);
  updateDisplay("set");
}

void longPressToRun()
{
  if(btn1_state == 1 && btn2_state == 1)
  {
    // save setting
    saveSettings();
    updateDisplay("save");
    main_fsm.trigger(TO_RUN);  
  }
  else if(btn1_state == 1 && btn2_state == 0)
  {
    // only button one longpressed, goto run without saving
    updateDisplay("ret");
    main_fsm.trigger(TO_RUN);  
  }
  btn2_state = 0;
  btn1_state = 0;
}

void on_setup_exit_exit()
{
  //Serial.println("on_setup_exit_exit");
  delay(2000);
}

void nullClick()
{
  
}

void loadDefaultSettings(){
  //Serial.println("loading default settings");
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
   //Serial.println("settings saved!");
}

void readSettings()
{
  EEPROM.get(CONFIG_START_ADDRESS, _settings);
  // DEBUG: print settings to serial
  /*
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
  */
  if (_settings.sversion != CONFIG_VERSION) {
    loadDefaultSettings();
  }
}

void setDisplay()
{
  byte numDigits = 4;
  byte digitPins[] = {2, 3, 4, 5};
  byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13};
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_CATHODE; // See README.md for options
  bool updateWithDelays = false; // Default. Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  
  segDisp.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
  segDisp.setBrightness(90);
}

void updateDisplay(String str, int no)
{
  char msg[5];
  String strMsg;
  if(no < 10)
  {
    strMsg = str.substring(0,2)+" "+String(no);
  }
  else if(no >=10 && no < 100)
  {
    strMsg = str.substring(0,2)+String(no);
  }
  else
  {
    strMsg = str.substring(0,1)+String(no);
  }
  strMsg.toCharArray(msg,5);
  //Serial.println("str: "+str+", no: "+String(no)+", strMsg: "+strMsg+", msg: "+msg );
  segDisp.setChars(msg);
}

void updateDisplay(String str)
{
  char msg[5];
  String strMsg;
  strMsg = str.substring(0,4);
  strMsg.toCharArray(msg,5);
  //Serial.println("str: "+str+", msg: "+msg );
  segDisp.setChars(msg);
}

