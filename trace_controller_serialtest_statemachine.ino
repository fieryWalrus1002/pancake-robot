// mimic the protocol below (we're not doing this right now but I'm leaving it here for future reference
    // measuring_pulse_duration(20u) which is actually like 85us
    // m_number_loops(3)
    // m_pulse_set(100,125,900)
    // m_measuring_interval(.4m,.4m,.4m) which is actually 800us
// using the Cerebot MX3ck board instead of UNO, so output pins will be for that board

////////////////////////////////// input state machine ////////////////////////////////////////
// the states of our input state-machine
typedef enum { NONE, GOT_M, GOT_N, GOT_I, GOT_G, GOT_H, GOT_V, GOT_R, GOT_P} states;

// current input state-machine state
states state = NONE;

// current partial number we add to
int current_value;

// serial variables
//String sdata=""; // buffer to hold serial data coming in from user
//byte ch = ' '; // temporary storage for each bbyte of data
//int new_value; // buffer to hold user integers

/////////////////////////////////// trace variables /////////////////////////////////////////////
// trace variables that are modified by experimentControl
// format for trace values will be something like this:
// n2000 i248 g0 h0 v1 r5 p40 m
// this would define a measurement trace with 
// 2000 data points, 
// pulse_interval = 248 us
// gain_vis = 0,
// gain_ir = 0,
// meas_led_vis = 1,
// meas_led_ir = 5,
// pulse_length = 40us
// m would then execute the trace by changing measure_state from false to true, which would stop listening for the duration of the trace

int num_points = 2000; // number of times that measurement pulse will be activated, set by n
int pulse_interval = 248; // pulse interval in time units chosen, set by i
int gain_vis = 0; // gain setting, from 0-7. int value will be interpreted to a binary output to detector by switch case, set by g
int gain_ir = 0; // gain setting, from 0-7. int value will be interpreted to a binary output to detector by switch case, set by h
int meas_led_vis = 1; // holds the green led pin number, set by v
int meas_led_ir = 5; // holds the IR led pin number, set by r
int pulse_length = 50; // the length that the measurement LEDs will be on during a measurement pulse in microseconds
bool measure_state = false;

//////////////////////////// internal variables used only by this code /////////////////////////////////////////////////////////
long prev_time = 0;
int counter = 0; // counter that will be used to count the number of measurement points and trigger the end of measure_state

//////////////////////////////////// board specific pin assignments ////////////////////////////////////////////////////////////
// cerebot JA port, used for output to the head
// upper row, looking at it is 3.3V, GND, 3, 2, 1, 0
// lower row, looking at it is 3.3V, GND, 7, 6, 5, 4
// there are LEDs hooked up to this port. HIGH signal will turn on the optical isolators and turn on the LEDs
int ledArray[] = {0, 1, 2, 3, 4, 5, 6};

// cerebot JB port, used for output to detector for gain and SH
// upper row, looking at it is 3.3V, GND, 11, 10, 9, 8
// lower row, looking at it is 3.3V, GND, 15, 14, 13, 12
int gainVisArray[] = {8, 9, 10};
int sh_pin = 11; // only need one pin for this, can be wired to both detectors

// cerebot JD port, used for output to IR detector gain
// upper row, looking at it is 3.3V, GND, 27 26 25 24
// lower row, looking at it is 3.3V, GND, 31 30 29 28
int gainIRArray[] = {24, 25, 26};

// cerebot JE port, used for output to ADC
// upper row, looking at it is 3.3V, GND, 35 34 33 32
// lower row, looking at it is 3.3V, GND, 39 38 37 36
int adc_pin = 39; // the adc_tri pinout, will trigger the adc when it goes HIGH as a pseudo clock

////////////////////////////////////////////// functions //////////////////////////////////////////////////////////
void measurement_pulse(){
  // once it triggers, it executes the following sequence of actions:
  
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

int set_gain(int gain_setting, int gainArray[]){
  switch (gain_setting) {
    case 0:           
        digitalWrite(gainArray[0], LOW);
        digitalWrite(gainArray[1], LOW);
        digitalWrite(gainArray[2], LOW);
        break;
      case 1:
        digitalWrite(gainArray[0], HIGH);
        digitalWrite(gainArray[1], LOW);
        digitalWrite(gainArray[2], LOW);
        break;
      case 2:
        digitalWrite(gainArray[0], LOW);
        digitalWrite(gainArray[1], HIGH);
        digitalWrite(gainArray[2], LOW);
        break;
      case 3:
        digitalWrite(gainArray[0], HIGH);
        digitalWrite(gainArray[1], HIGH);
        digitalWrite(gainArray[2], LOW);
        break;
      case 4:
        digitalWrite(gainArray[0], LOW);
        digitalWrite(gainArray[1], LOW);
        digitalWrite(gainArray[2], HIGH);
        break;
      case 5:
        digitalWrite(gainArray[0], HIGH);
        digitalWrite(gainArray[1], LOW);
        digitalWrite(gainArray[2], HIGH);
        break;
      case 6:
        digitalWrite(gainArray[0], LOW);
        digitalWrite(gainArray[1], HIGH);
        digitalWrite(gainArray[2], HIGH);
        break;
      case 7:
        digitalWrite(gainArray[0], HIGH);
        digitalWrite(gainArray[1], HIGH);
        digitalWrite(gainArray[2], HIGH);
        break;
     }
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
  
void setVisGain(const int value){
  set_gain(value, gainVisArray);  // set the gain output bits for visible detector
  Serial.print("gain_vis = ");
  Serial.println(value);
  }
  
void setIRGain(const int value){
  set_gain(value, gainIRArray);  // set the gain output for the IR detector
  Serial.print("gain_ir = ");
  Serial.println(value);
  }

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

void executeTrace(){
  measure_state = true;
  Serial.println("Start measurement");
  }

void handlePreviousState(){
  //NONE, GOT_M, GOT_N, GOT_I, GOT_G, GOT_H, GOT_V, GOT_R, GOT_P
  switch(state){
    case GOT_N:
      setNumPoints(current_value);
      break;
    case GOT_I:
      setPulseInterval(current_value);
      break;
    case GOT_G:
      setVisGain(current_value);
      break;
    case GOT_H:
      setIRGain(current_value);
      break;
    case GOT_V:
      setVisLED(current_value);
      break;
    case GOT_R:
      setIRLED(current_value);
      break;
    case GOT_P:
      break;
    case GOT_M:
      executeTrace();
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
      case 'm':
        state = GOT_M;
        break;
      case 'n':
        state = GOT_N;
        break;
      case 'i':
        state = GOT_I;
        break;
      case 'g':
        state = GOT_G;
        break;
      case 'h':
        state = GOT_H;
        break;
      case 'v':
        state = GOT_V;
        break;
      case 'r':
        state = GOT_R;
        break;
      case 'p':
        state = GOT_P;
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
}  

///////////////////////////// BEGIN THE LOOOOOOOP ///////////////////////////////////////////////////
void loop()
{
  ////////////////// serial interpreter ///////////////////////
  // listens for the following commands:
  // nXXXX : number of data points for the traces
  // iXXXX : pulse interval in microseconds
  // gX : gain setting, from 0-7. int value will be interpreted to a binary output to detector by switch case
  // vX : sets the visible spectrum LED pin number, usually a green LED
  // rX : sets the infrared LED pin number
  // pXXXX : sets the length that the measurement pulses are HIGH during a measurement pulse
  // m : stop listening, and move to measure_state = true. Execute trace, and then it will begin listening again.
  // adapted from http://gammon.com.au/serial state machine example
  
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
    
    while (counter <= num_points){
      // check the time elapsed since the last loop, and if it has, execute a measurement pulse
      if(micros() - prev_time >= pulse_interval){
        counter += 1;
        prev_time = micros(); // reset the pulse timer at the beginning of the pulse
        measurement_pulse();
      }
    }
    // reset in order to listen for serial data
    measure_state = false;
    counter = 0;
    Serial.println("trace finished, back to listening state");
  } // end measure_state loop
} // end main loop
