#include<arduino.h>

#include "PulseSensor.h"
#include "TimerOne.h"


volatile static int BPM=0;                   // int that holds raw Analog in 0. updated every 2mS
volatile static int Signal=0;                // holds the incoming raw data
volatile static int IBI = 600;             // int that holds the time interval between beats! Must be seeded!
volatile static boolean Pulse = false;     // "True" when User's live heartbeat is detected. "False" when not a "live beat".
volatile static boolean gotBeat = false;        // becomes true when Arduoino finds a beat until you have polled with gotBeat.

volatile static int rate[10];                    // array to hold last ten IBI values
volatile static unsigned long sampleCounter = 0;          // used to determine pulse timing
volatile static unsigned long lastBeatTime = 0;           // used to find IBI
volatile static int P =512;                      // used to find peak in pulse wave, seeded
volatile static int T = 512;                     // used to find trough in pulse wave, seeded
volatile static int thresh = 530;                // used to find instant moment of heart beat, seeded
volatile static int amp = 0;                   // used to hold amplitude of pulse waveform, seeded
volatile static boolean firstBeat = true;        // used to seed rate array so we startup with reasonable BPM
volatile static boolean secondBeat = false;      // used to seed rate array so we startup with reasonable BPM

volatile int pulsePin=0;
volatile boolean enabled=false;

void pulseTimerISR();

void PulseSensor::enable(int pin)
{
    
    pulsePin=pin;
    if(!enabled)
    {
      cli();
      Timer1.initialize(2000); // set a timer of length 2 milliseconds
      Timer1.attachInterrupt(pulseTimerISR); // attach the service routine here
      sei();
      enabled=true;
    }
}

void PulseSensor::disable(int pin)
{
  if(enabled)
  {
    Timer1.detachInterrupt();
  }
}

int PulseSensor::getHeartRate(int pin)
{
  if(!enabled)
  {
    enable(pin);
  }
  return BPM;
}

boolean PulseSensor::hasBeatHappened(int pin)
{
    boolean temp=gotBeat;
    gotBeat=false;
    return temp;
}

void pulseTimerISR()
{
  cli();                                      // disable interrupts while we do this
  Signal = analogRead(pulsePin);              // read the Pulse Sensor
  sampleCounter += 2;                         // keep track of the time in mS with this variable
  int N = sampleCounter - lastBeatTime;       // monitor the time since the last beat to avoid noise

    //  find the peak and trough of the pulse wave
  if(Signal < thresh && N > (IBI/5)*3){       // avoid dichrotic noise by waiting 3/5 of last IBI
    if (Signal < T){                        // T is the trough
      T = Signal;                         // keep track of lowest point in pulse wave
    }
  }

  if(Signal > thresh && Signal > P){          // thresh condition helps avoid noise
    P = Signal;                             // P is the peak
  }                                        // keep track of highest point in pulse wave

  //  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
  // signal surges up in value every time there is a pulse
  if (N > 250){                                   // avoid high frequency noise
    if ( (Signal > thresh) && (Pulse == false) && (N > (IBI/5)*3) ){
      Pulse = true;                               // set the Pulse flag when we think there is a pulse
//      digitalWrite(blinkPin,HIGH);                // turn on pin 13 LED
      IBI = sampleCounter - lastBeatTime;         // measure time between beats in mS
      lastBeatTime = sampleCounter;               // keep track of time for next pulse

      if(secondBeat){                        // if this is the second beat, if secondBeat == TRUE
        secondBeat = false;                  // clear secondBeat flag
        for(int i=0; i<=9; i++){             // seed the running total to get a realisitic BPM at startup
          rate[i] = IBI;
        }
      }

      if(firstBeat){                         // if it's the first time we found a beat, if firstBeat == TRUE
        firstBeat = false;                   // clear firstBeat flag
        secondBeat = true;                   // set the second beat flag
        gotBeat=true;
        sei();                               // enable interrupts again
        return;                              // IBI value is unreliable so discard it
      }


      // keep a running total of the last 10 IBI values
      word runningTotal = 0;                  // clear the runningTotal variable

      for(int i=0; i<=8; i++){                // shift data in the rate array
        rate[i] = rate[i+1];                  // and drop the oldest IBI value
        runningTotal += rate[i];              // add up the 9 oldest IBI values
      }

      rate[9] = IBI;                          // add the latest IBI to the rate array
      runningTotal += rate[9];                // add the latest IBI to runningTotal
      runningTotal /= 10;                     // average the last 10 IBI values
      BPM = 60000/runningTotal;               // how many beats can fit into a minute? that's BPM!
      gotBeat = true;                              // set Quantified Self flag
      // gotBeat FLAG IS NOT CLEARED INSIDE THIS ISR
    }
  }

  if (Signal < thresh && Pulse == true){   // when the values are going down, the beat is over
//    digitalWrite(blinkPin,LOW);            // turn off pin 13 LED
    Pulse = false;                         // reset the Pulse flag so we can do it again
    amp = P - T;                           // get amplitude of the pulse wave
    thresh = amp/2 + T;                    // set thresh at 50% of the amplitude
    P = thresh;                            // reset these for next time
    T = thresh;
  }

  if (N > 2500){                           // if 2.5 seconds go by without a beat
    thresh = 530;                          // set thresh default
    P = 512;                               // set P default
    T = 512;                               // set T default
    lastBeatTime = sampleCounter;          // bring the lastBeatTime up to date
    firstBeat = true;                      // set these to avoid noise
    secondBeat = false;                    // when we get the heartbeat back
    BPM =0;                                // 0 BPM = no heart detected
    gotBeat=false;
  }
  sei();                                   // enable interrupts when youre done!
}

