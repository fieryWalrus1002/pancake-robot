////////////////////////////////// input state machine ////////////////////////////////////////
// adapted from http://gammon.com.au/serial state machine example
// the states of our input state-machine
typedef enum { NONE, GOT_A, GOT_D, GOT_G, GOT_H, GOT_I, GOT_L, GOT_M, GOT_N, GOT_P, GOT_R, GOT_S, GOT_T, GOT_V, GOT_W, GOT_X, GOT_Y, GOT_Z} states;
states state = NONE;
int current_value;

/////////////////////////////////// trace variables /////////////////////////////////////////////
// trace variables that are modified by experimentControl
// format for trace values will be something like this:
// testing d n6000 i248 g0 h0 v1 r5 p40 a128 s500 t900 w1 x2 y4 z1 m
// this would define a measurement trace with 
// n6000 = 6000 data points, 
// i248 = pulse_interval of 248 us,
// g0 = gain_vis = 0,
// h0 = gain_ir = 0,
// v1 = meas_led_vis = 1,
// r5 = meas_led_ir = 5,
// p40 = pulse_length = 40us
// a128 = actininc light intensity = 128, this turns on the actinic light during measure_state = false, not during a trace
// s500 = sat_pulse_length = 500 in milliseconds
// t900 = sat_trigger_point = 900
// w1 = act_int_phase[0] = 1/255
// x2 = act_int_phase[1] = 2/255
// y4 = act_int_phase[2] = 4/255
// z1 = pulse_mode = 1; will trigger a saturation pulse. 0 is dirk. 2 is single-turnover flash.
// m would then execute the trace by changing measure_state from false to true, which would stop listening for the duration of the trace

int num_points = 4000; // number of times that measurement pulse will be activated, set by n
int pulse_interval = 248; // pulse interval in time units chosen, set by i
int gain_vis = 0; // gain setting, from 0-7. int value will be interpreted to a binary output to detector by switch case, set by g
int gain_ir = 0; // gain setting, from 0-7. int value will be interpreted to a binary output to detector by switch case, set by h
int meas_led_vis = 1; // holds the green led pin number, set by v
int meas_led_ir = 5; // holds the IR led pin number, set by r
int pulse_length = 50; // the length that the measurement LEDs will be on during a measurement pulse in microseconds
int sat_pulse_length = 200; // length in milliseconds for the saturation pulse
int sat_trigger_point = 1501; // the data point at which the sat pulse will trigger. compares to counter to trigger.
int act_int_phase[] = {1, 64, 128}; // holds the actinic intenisty values for phases 0-2
int pulse_mode = 1; // 1 is a sat pulse, 0 is a dirk, 2 is a single turnover flash

//////////////////////////// internal variables not modified externally /////////////////////////////////////////////////////////
long prev_time = 0;
long sat_trigger_time = 0;
int counter = 0; // counter that will be used to count the number of measurement points and trigger the end of measure_state
bool measure_state = false;
int trace_phase = 0; // trace phase will be 0 for before sat pulse, 1 for sat pulse, 2 for post sat pulse. 
int trace_length = 0;
int sat_pulse_trigger_time = 0;
long trace_begin_time = 0;
int test_value = 0; // just an int for doing test feedback stuff

//////////////////////////////////// board specific pin assignments ////////////////////////////////////////////////////////////
// using the Cerebot MX3ck board instead of UNO, so output pins will be for that board

// cerebot JA port, used for output to the head
// upper row, looking at it is 3.3V, GND, 3, 2, 1, 0
// lower row, looking at it is 3.3V, GND, 7, 6, 5, 4
// there are LEDs hooked up to this port. HIGH signal will turn on the optical isolators and turn on the LEDs
// the transmit and receive pins for serial are usually pins 0 and 1, so we won't use those for now
// if we need more LED pins we can use some of JE later
int ledArray[] = {2, 3, 4, 5, 6, 7};

// cerebot JB port, used for output to detector for SH, output to IR detector gain and vis detector gain
// upper row, looking at it is 3.3V, GND, 11, 10, 9, 8
// lower row, looking at it is 3.3V, GND, 15, 14, 13, 12
int gainVisArray[] = {8, 9, 10};
int gainIRArray[] = {12, 13, 14};
int sh_pin = 11; // only need one pin for this, can be wired to both detectors

// cerebot JD port, used for actinic board inputs to set intensity of the actinic light
// upper row, looking at it is 3.3V, GND, 27 26 25 24
// lower row, looking at it is 3.3V, GND, 31 30 29 28
int act_int_pins[] = {24, 25, 26, 27, 28, 29, 30, 31};

// cerebot JE port, used for output to ADC and to the actinic board to trigger saturation pulse and the single turnover flash
// upper row, looking at it is 3.3V, GND, 35 34 33 32
// lower row, looking at it is 3.3V, GND, 39 38 37 36
int satPulse_pin = 32;
int stoFlash_pin = 33;
int adc_pin = 39; // the adc_tri pinout, will trigger the adc when it goes HIGH as a pseudo clock

////////////////////////////////////////////// functions //////////////////////////////////////////////////////////
void measurement_pulse(){
  // once it triggers, it executes the following sequence of actions:
  // if this is a singleturnover flash trigger point, then we trigger the single turnover flash
  if(counter == sat_trigger_point && pulse_mode == 2){
    digitalWrite(stoFlash_pin, HIGH);
    delayMicroseconds(1);
    digitalWrite(stoFlash_pin, LOW);
    Serial.println("stf triggered");  
    delayMicroseconds(8);
  }
  
  // LEDs to HIGH, set SH to track voltage
  digitalWrite(sh_pin, HIGH); // ensure SH pin is tracking voltage
  digitalWrite(meas_led_vis, HIGH); // set green LED to HIGH
  digitalWrite(meas_led_ir, HIGH); // set IR LED to HIGH
  delayMicroseconds(pulse_length); // LED pulse is set by the variable pulse_length

  // set SH to HOLD, trigger ADC to acquire voltage
  digitalWrite(sh_pin, LOW); // set the sample and hold chip to hold voltage. takes 600ns or so
  digitalWrite(adc_pin, HIGH); // set the ADC trigger to HIGH so the data will be logged

  // turn off LEDs
  digitalWrite(meas_led_vis, LOW); // set green LED to LOW
  digitalWrite(meas_led_ir, LOW); // set IR LED to LOW

  // keep SH HOLD until ADC is done acquiring data
  delayMicroseconds(20); // at 50kS/s/ch, it takes at least 20us to acquire a single 16-bit datapoint. SH is keeping voltage steady during this time.
  digitalWrite(sh_pin, 1); // release the SH chip to track voltage again
  digitalWrite(adc_pin, 0); // set the ADC trigger back to LOW in prep for next data point
}

//  1 <<  0  ==    1
//  1 <<  1  ==    2
//  1 <<  2  ==    4
//  ...
//  1 <<  8  ==  256
// length of actinic light period is set in the python experimentControl, not here. We just turn it on and off.
void setActIntensity(byte value, int intensity_array[]){
  for(int i = 0, mask = 1; i < 8; i++, mask = mask << 1){
    if (value & mask) 
    {
        // bit is on
        digitalWrite(intensity_array[i], HIGH);
    }
    else
    {
        // bit is off
        digitalWrite(intensity_array[i], LOW);
    }
  }
  Serial.print("actinic intensity is now: ");
  Serial.println(value & 255);
}

int set_gain(byte value, int gainArray[]){
  //int gainVisArray[] = {8, 9, 10};
  //int gainIRArray[] = {12, 13, 14};
  for(int i = 0, mask = 1; i < 3; i++, mask = mask << 1){
    if (value & mask) 
    {
        // bit is on
        digitalWrite(gainArray[i], HIGH);
    }
    else
    {
        // bit is off
        digitalWrite(gainArray[i], LOW);
    }
  }
  Serial.print("gain = ");
  test_value = digitalRead(gainArray[0]) * 1 + digitalRead(gainArray[1]) * 2 + digitalRead(gainArray[2]) * 4;
  Serial.println(test_value);
}

void setupPins(){
  for(int pinNum=0; pinNum<40; pinNum++){
    pinMode(pinNum, OUTPUT);
    digitalWrite(pinNum, LOW);
  }
}

void setNumPoints(const int value){
  num_points = value;
  Serial.print("num_points = ");
  Serial.println(num_points);
  }
  
void setPulseInterval(const int value){
  pulse_interval = value;
  Serial.print("pulse_interval = ");
  Serial.println(pulse_interval);
  }
  
//void setVisGain(const int value){
//  set_gain(value, gainVisArray);  // set the gain output bits for visible detector
//  Serial.print("gain_vis = ");
//  Serial.println(value);
//  }
//  
//void setIRGain(const int value){
//  set_gain(value, gainIRArray);  // set the gain output for the IR detector
//  Serial.print("gain_ir = ");
//  Serial.println(value);
//  }

void setVisLED(const int value){
  meas_led_vis = value;
  Serial.print("meas_led_vis = ");
  Serial.println(meas_led_vis);
  }

void setIRLED(const int value){
  meas_led_ir = value;
  Serial.print("meas_led_ir = ");
  Serial.println(meas_led_ir);
  }

void setPulseLength(const int value){
  pulse_length = value;
  Serial.print("pulse_length = ");
  Serial.println(pulse_length);
}

void setSatPulseLength(const int value){
  sat_pulse_length = value;
  Serial.print("sat_pulse_length (ms) = ");
  Serial.println(sat_pulse_length);
}

void setSatTriggerPoint(const int value){
  sat_trigger_point = value;
  Serial.print("sat_trigger_point = ");
  Serial.println(sat_trigger_point);
}

void executeTrace(){
  measure_state = true;
  Serial.println("Start measurement");
  }
 
void sendStatus(){
//  Serial.println("Arduino traceController is listening.");
  Serial.print("num_points = ");
  Serial.println(num_points);

  Serial.print("sat_trigger_point = ");
  Serial.println(sat_trigger_point);

  Serial.print("sat_pulse_length = ");
  Serial.println(sat_pulse_length);
}

void setPhaseActValue(const int value, int phase_num){
  // act_int_phase[]act_int_phase
  act_int_phase[phase_num] = value;
  Serial.print("phase[");
  Serial.print(phase_num);
  Serial.print("] actinic intensity = ");
  Serial.println(act_int_phase[phase_num]);
} // end setPhaseActValue

void respondState(){
  
  if(measure_state == false){
    Serial.println("listening");
    
    for(int i=0; i<6; i++){
      digitalWrite(LED_BUILTIN, LOW);
      delay(150);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(150);
    }
  }
}

      
void handlePreviousState(){
  //NONE, GOT_M, GOT_N, GOT_I, GOT_G, GOT_H, GOT_V, GOT_R, GOT_P
  switch(state){
    case GOT_A:
      setActIntensity(current_value, act_int_pins);
      break;
    case GOT_D:
      // return diagnostic info
      sendStatus();
      break;
    case GOT_G:
      set_gain(current_value, gainVisArray);
      break;
    case GOT_H:
      set_gain(current_value, gainIRArray);
      break;
    case GOT_I:
      setPulseInterval(current_value);
      break;
    case GOT_L:
      respondState();
      break;
    case GOT_M:
      executeTrace();
      break;
    case GOT_N:
      setNumPoints(current_value);
      break;
    case GOT_P:
      setPulseLength(current_value);
      break;
    case GOT_R:
      setIRLED(current_value);
      break;
    case GOT_S:
      setSatPulseLength(current_value);
      break;
    case GOT_T:
      setSatTriggerPoint(current_value);  
    case GOT_V:
      setVisLED(current_value);
      break;
    case GOT_W:
      setPhaseActValue(current_value, 0);
      break;
    case GOT_X:
      setPhaseActValue(current_value, 1);
      break;
    case GOT_Y:
      setPhaseActValue(current_value, 2);
      break;
    case GOT_Z:
      pulse_mode = current_value;
      break;
    default:
      break;
  } // end of switch

  current_value = 0; // since we utilized the current_value above, now we reset it to zero for the next variable
}

void processIncomingByte (const byte c){
  if (isdigit(c)){
    current_value *= 10;
    current_value += c - '0';
  } // end of digit
  else{
    //if the number ends, its time for a state change
    handlePreviousState(); // take the current value and then apply it to whatever variable is appropriate
    
    // set the new state if we recognize it
    switch(c){
      //GOT_M, GOT_N, GOT_I, GOT_G, GOT_H, GOT_V, GOT_R, GOT_P
      case 'a':
        state = GOT_A;
        break;
      case 'd':
        state = GOT_D;
        break;
      case 'g':
        state = GOT_G;
        break;
      case 'h':
        state = GOT_H;
        break;
      case 'i':
        state = GOT_I;
        break;
      case 'l':
        state = GOT_L;
        break;
      case 'm':
        state = GOT_M;
        break;
      case 'n':
        state = GOT_N;
        break;
      case 'p':
        state = GOT_P;
        break;
      case 'r':
        state = GOT_R;
        break;
      case 's':
        state = GOT_S;
        break;
      case 't':
        state = GOT_T;
        break;
      case 'v':
        state = GOT_V;
        break;
      case 'w':
        state = GOT_W;
        break;
      case 'x':
        state = GOT_X;
        break;
      case 'y':
        state = GOT_Y;
        break;
      case 'z':
        state = GOT_Z;
        break;
      default:
        state = NONE;
        break;
    } //end switch
  } // end of not digit
}

////////////////////////////////////// void setup ////////////////////////////////////////////////////////// 
void setup()
{ 
  Serial.begin(115200);
  state = NONE; // set initial state
  
  //set_gain(gain_ir, gainIRArray);  // set the gain output bits for IR detector
  prev_time = micros(); // set our previous time to zero
  setupPins(); // set all pins to output and LOW
  pinMode(LED_BUILTIN, OUTPUT); //  set the builtin LED mode
  Serial.println("arduino is listening for input on serial");
  //sendStatus();
}  

///////////////////////////// BEGIN THE LOOOOOOOP ///////////////////////////////////////////////////
void loop()
{
  ////////////////// serial interpreter ///////////////////////
  // listens for the following commands:
  // nXXXX : number of data points for the traces
  // iXXXX : pulse interval in microsecondsx
  
  // gX : gain setting, from 0-7. int value will be interpreted to a binary output to detector by switch case
  // vX : sets the visible spectrum LED pin number, usually a green LED
  // rX : sets the infrared LED pin number
  // pXXXX : sets the length that the measurement pulses are HIGH during a measurement pulse
  // m : stop listening, and move to measure_state = true. Execute trace, and then it will begin listening again.
  
  
  if (measure_state == false){
    digitalWrite(LED_BUILTIN, HIGH);

    while (Serial.available()){
      processIncomingByte(Serial.read());
    } // 
  } // end of listening state

  /////////// trace logic below /////////////////////////////
  while (measure_state == true ){
    Serial.println("begin trace");
    digitalWrite(LED_BUILTIN, LOW); // just visual feedback that a trace is running

    // tell us how long the trace will be, and when the saturation pulse will trigger. 
    // calculate the length of the trace
    trace_length = (pulse_interval * num_points) / 1000;
    sat_pulse_trigger_time = (sat_trigger_point * pulse_interval) / 1000;

    trace_begin_time = millis();
    Serial.print("trace will be approximately ");
    Serial.print(trace_length);
    Serial.println(" milliseconds.");
    
    // ready the saturation pulse state
    //sat_triggered = false;
    ////////////////////////////////// BEGIN PHASE 0: PRE PULSE MEAUSREMENT //////////////////////////////////
    trace_phase = 0;
    Serial.print("trace_phase=");
    Serial.println(trace_phase);

    // set appropriate actinic intensity for trace_phase 0
    setActIntensity(act_int_phase[trace_phase], act_int_pins);  

    //////////////////////////////// the while loop for trace execution //////////////////////////////////////
    while (counter <= num_points){
      
      /////////////// BEGIN PHASE 1: TRIGGER DIRK / SAT PULSE / SINGLE TURNOVER FLASH ////////////////////////
      if(counter == sat_trigger_point && trace_phase == 0)
      {
        // note the time
        sat_trigger_time = millis();

        // advance the trace_phase    
        trace_phase += 1;
        Serial.print("trace_phase=");
        Serial.println(trace_phase);

        // check the pulse_mode to either trigger a dirk, a sat pulse or a single turnover flash
        if (pulse_mode == 1){
          // sat pulse
            digitalWrite(stoFlash_pin, HIGH);
            Serial.println("satpulse triggered");  
        } else if (pulse_mode = 0){
          setActIntensity(0, act_int_pins);
          Serial.println("dirk triggered");  
        }

        // set the actinic to the mid phase intensity value
        setActIntensity(act_int_phase[trace_phase], act_int_pins);
                           
       } // end sat pulse trigger
       ///////////////////////////////END SAT PULSE TRIGGER //////////////////////////////////////////////////


      
      ///////////////////////////////// MEASUREMENT PULSE ////////////////////////////////////////////////////
      // check the time elapsed since the last loop, and if it has, execute a measurement pulse
      if(micros() - prev_time >= pulse_interval){
        counter += 1;
        prev_time = micros(); // reset the pulse timer at the beginning of the pulse
        measurement_pulse();
      }
      ////////////////////////////////END MEASUREMENT PULSE //////////////////////////////////////////////////


      /////////////////////// BEGIN PHASE 2 : POST DIRK / SAT PULSE / SINGLE TURNVOER FLASH /////////////////////////
      // check to see if it is time to turn off the saturation pulse
      if(millis() - sat_trigger_time >= sat_pulse_length && trace_phase == 1)
      {
        if (pulse_mode == 0){
          // dirk
          Serial.println("dirk ended");
        } else if (pulse_mode == 1){
          // sat pulse
          digitalWrite(satPulse_pin, LOW);
          Serial.println("satpulse ended");
        } else if (pulse_mode = 2){
          // ending a single turnover flash point
          Serial.println("ST flash ended");  
        }       
        // advance the trace phase to post pulse
        trace_phase += 1;
        Serial.print("trace_phase=");
        Serial.println(trace_phase);

        // set the actinic intensity for the third phase
        setActIntensity(act_int_phase[trace_phase], act_int_pins);  
      } //////////// end the saturation pulse off logic //////////////////////////////////////////
    } // end of while counter <= num_points loop

    //////////////////////////////// cleanup at the end of the trace ///////////////////////////////////
    // make sure that saturation pulse is done
    digitalWrite(satPulse_pin, LOW);
    digitalWrite(stoFlash_pin, LOW);
    setActIntensity(0, act_int_pins);
    
    // reset in order to listen for serial data
    measure_state = false;
    counter = 0;  
    Serial.println("trace finished, back to listening state");
    Serial.print("trace time elapsed: ");
    Serial.print(millis() - trace_begin_time);
    Serial.println(" ms");
  } ////////////// end measure_state loop, trace is complete, back to listening /////////////////////////
} // end main loop
