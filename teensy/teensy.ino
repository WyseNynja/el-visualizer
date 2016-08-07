/* control for the Teensy 3.2 in Bryan's Sound-reactive EL Jacket
 *
 * when it is quiet, lights flash out morse code from an SD card
 * when it is loud, lights flash with the music
 *
 * TODO: analog button press to set minInputSensitivity
 *
 */

/*
 * you can easily customize these
 *
 * todo: define or const?
 */
#define NUMB_PERCENT 0.75  // how much of the average level to subtract from the input
#define LOUDEST_EMA_ALPHA 0.20  // alpha for calculating the loudest sound
#define NUMB_EMA_ALPHA 0.02    // alpha for calculating background sound to ignore
#define BUTTON_HOLD_MS 200
#define BUTTON_INTERVAL 20
#define DEBUG true    // todo: write a debug_print that uses this to print to serial
#define FFT_IGNORED_BINS 1  // skip the first FFT_IGNORED_BINS * FFT_HZ_PER_BIN Hz
#define INPUT_NUM_OUTPUTS_KNOB 15
#define MAX_MORSE_ARRAY_LENGTH 256  // todo: tune this
#define MAX_OFF_MS 6000
#define MINIMUM_INPUT_RANGE 0.80  // activate outputs on sounds that are at least this % as loud as the loudest sound

unsigned int minimumOnMs = 118;  // this can be overriden by the SD card
bool randomizeOutputs = false; // todo: move this to the teensy so we can read this setting off the sd card

float minInputSensitivity = 0.060;  // this can be overriden by the SD card

const int morseDitMs = 250;
const int morseDahMs = morseDitMs * 3;
const int morseElementSpaceMs = morseDitMs;
const int morseWordSpaceMs = morseDitMs * 7;
/*
 * END you can easily customize these
 */

#define BETWEEN(value, min, max) (value < max && value > min)
#define FFT_HZ_PER_BIN 43
#define MAX_FFT_BINS 512
#define MAX_OUTPUTS 8  // EL Sequencer only has 8 wires, but you could easily make this larger and drive other things
#define OUTPUT_A 0
#define OUTPUT_B 1
#define OUTPUT_C 2
#define OUTPUT_D 3
#define OUTPUT_E 4
#define OUTPUT_F 5
#define OUTPUT_G 8
#define OUTPUT_H 20
const int outputPins[MAX_OUTPUTS] = {OUTPUT_A, OUTPUT_B, OUTPUT_C, OUTPUT_D, OUTPUT_E, OUTPUT_F, OUTPUT_G, OUTPUT_H};
int outputBins[MAX_OUTPUTS];

#include <Audio.h>
#include <elapsedMillis.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioInputI2S audioInput;
AudioOutputI2S audioOutput; // even though we don't output to this, it still needs to be defined for the shield to work
AudioAnalyzeFFT1024 myFFT;
AudioConnection patchCord1(audioInput, audioOutput);
AudioConnection patchCord2(audioInput, myFFT);
AudioControlSGTL5000 audioShield;

// save on/off/off+next states. if current time < runningSumOfCheckMs, than do action. else check next time
struct TimedAction {
  unsigned int checkMs;
  unsigned int outputAction;
};
int morseArrayLength = 0;
TimedAction morseArray[MAX_MORSE_ARRAY_LENGTH];


int morseDit(TimedAction morseArray[], int i) {
  Serial.print('.');
  // todo: protect overflowing morseArray
  morseArray[i].checkMs = morseDitMs;
  morseArray[i].outputAction = HIGH;
  i++;
  return morse_element_space(morseArray, i);
}

int morseDah(TimedAction morseArray[], int i) {
  Serial.print('-');
  // todo: protect overflowing morseArray
  morseArray[i].checkMs = morseDahMs;
  morseArray[i].outputAction = HIGH;
  i++;
  return morse_element_space(morseArray, i);
}

int morse_element_space(TimedAction morseArray[], int i) {
  // todo: protect overflowing morseArray
  morseArray[i].checkMs = morseElementSpaceMs;
  morseArray[i].outputAction = LOW;
  i++;
  return i;
}

int morse_word_space(TimedAction morseArray[], int i) {
  Serial.print(" / ");
  // todo: protect overflowing morseArray
  morseArray[i].checkMs = morseWordSpaceMs;
  morseArray[i].outputAction = LOW;
  i++;
  return i;
}

void string2morseArray(String morseString) {
  morseArrayLength = 0;
  for (unsigned int i = 0; i < morseString.length(); i++) {
    // todo: make sure morseArrayLength never exceeds max
    // https://upload.wikimedia.org/wikipedia/en/5/5a/Morse_comparison.svg
    // todo: make everything uppercase?
    // todo: put this in its own library?
    switch (morseString[i]) {
      case 'E':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'T':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'I':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'A':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'N':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'M':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'S':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'U':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'R':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'W':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'D':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'K':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'G':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'O':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'H':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'V':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'F':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case ' ':
        morseArrayLength = morse_word_space(morseArray, morseArrayLength);
        break;
      case 'L':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'P':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'J':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'B':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'X':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'C':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'Y':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case 'Z':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case 'Q':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '5':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '4':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '3':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '2':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '1':
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '6':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '7':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '8':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '9':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        break;
      case '0':
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        break;
      case '.':
        // todo: do this on a different wire?
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
        morseArrayLength = morseDit(morseArray, morseArrayLength);
        morseArrayLength = morseDah(morseArray, morseArrayLength);
      default:
        break;
    }

    Serial.print(' ');
    morseArrayLength = morse_element_space(morseArray, morseArrayLength);
  }
  Serial.println();
}

// from http://forum.arduino.cc/index.php?topic=43424.0
// generate a value between 0 <= x < n, thus there are n possible outputs
int rand_range(int n) {
  int r, ul;
  ul = RAND_MAX - RAND_MAX % n;
  while ((r = rand()) >= ul) {
    ;  // get a random number that is fair
  }
  return r % n;
}

// from http://forum.arduino.cc/index.php?topic=43424.0
void bubbleUnsort(int *list, int elem) {
  for (int a = elem - 1; a > 0; a--) {
    // todo: not sure what is better. apparently the built in random has some sort of bias?
    //int r = random(a+1);
    int r = rand_range(a + 1);
    if (r != a) {
      int temp = list[a];
      list[a] = list[r];
      list[r] = temp;
      // https://betterexplained.com/articles/swap-two-variables-using-xor/
      //list[a] = list[a] xor list[r];
      //list[r] = list[a] xor list[r];
      //list[a] = list[a] xor list[r];
    }
  }
}

/*
   END HELPER FUNCTIONS
*/

void setup() {
  Serial.begin(9600);  // TODO! disable this if DEBUG mode on. optimizer will get rid of it

  // setup audio shield
  AudioMemory(12); // todo: tune this. so far max i've seen is 11
  audioShield.enable();
  audioShield.muteHeadphone(); // to avoid any clicks
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  // audioShield.lineInLevel(4);  // 0-15. 5 is default. 0 is highest voltage
  audioShield.volume(0.5);  // for debugging
  audioShield.micGain(63);  // 0-63

  audioShield.audioPreProcessorEnable();  // todo: i dont think post goes to the

  //audioShield.enhanceBassEnable();
  //audioShield.enhanceBass(1.0, -1.0, false, 5);  // todo: tune this

  // bass, mid_bass, midrange, mid_treble, treble
  audioShield.eqSelect(GRAPHIC_EQUALIZER);
  audioShield.eqBands(-1.0, -0.10, 0.10, 0.10, 0.33);  // todo: tune this

  // todo: autoVolume?

  // setup outputs
  for (int i = 0; i < MAX_OUTPUTS; i++) {
    pinMode(outputPins[i], OUTPUT);
  }

  // setup numOutputs knob
  pinMode(INPUT_NUM_OUTPUTS_KNOB, INPUT);

  // read text off the SD card and translate it into morse code
  String morseString = "";
  SPI.setMOSI(7);
  SPI.setSCK(14);
  if (SD.begin(10)) {
    // todo: support a directory with multiple text files?
    File morseFile = SD.open("morse.txt", FILE_READ);
    if (morseFile) {
      Serial.println("Reading morse.txt...");
      // if the file is available, read it:
      while (morseFile.available()) {
        // todo: this is wrong, but im just testing
        morseString += (char)morseFile.read();
      }
      morseFile.close();
    }

    // todo: should we just do this as we read? theres no real need to build a String besides the pretty serial.print
    string2morseArray(morseString);

    // todo: read minimumOnMs from SD card
    // todo: read minInputSensitivity from SD card
    // todo: read randomizeOutputs from SD card
  }

  Serial.print("morse array length: ");
  Serial.println(morseArrayLength);

  audioShield.unmuteHeadphone();  // for debugging

  randomSeed(morseArrayLength + analogRead(0));

  Serial.println("Waiting for EL Sequencer...");
  delay(500);

  Serial.println("Starting...");
}


elapsedMillis nowMs = 0;    // todo: do we care if this overflows?

int blinkOutput = OUTPUT_A;
int numOutputs = 1;
int randomizedOutputIds[MAX_OUTPUTS];

float lastLoudestLevel = 0;

unsigned long lastOnMs;
unsigned long lastOnMsArray[MAX_OUTPUTS];
bool outputStates[MAX_OUTPUTS];

float avgInputLevel[MAX_FFT_BINS];

int lastMorseCommandId = -1;


void updateNumOutputs() {
  // configure number of outputs
  int oldNumOutputs = numOutputs;
  numOutputs = (int)analogRead(INPUT_NUM_OUTPUTS_KNOB) / (1024 / MAX_OUTPUTS) + 1;

  if (oldNumOutputs == numOutputs) {
    return;
  }
  // we changed numOutputs

  Serial.print("New numOutputs: ");
  Serial.println(numOutputs);

  for (int i=0; i<MAX_OUTPUTS; i++) {
    // turn all the wires off
    digitalWrite(outputPins[i], LOW);

    // put the randomized inputs back in order in case we lowered numOutputs
    randomizedOutputIds[i] = i;
  }

  if (randomizeOutputs) {
    // shuffle the outputs
    bubbleUnsort(randomizedOutputIds, numOutputs);
    bubbleUnsort(randomizedOutputIds, numOutputs);
  }

  // https://en.wikipedia.org/wiki/Piano_key_frequencies
  // TODO! TUNE THESE
  switch(numOutputs) {
    case 1:
      outputBins[0] = 82;  // todo: tune this
      break;
    case 2:
      outputBins[0] = 10;  // everything below 440 Hz
      outputBins[1] = 82;  // everything below 6 kHz
      break;
    case 3:
      outputBins[0] = 6;   // todo: tune this
      outputBins[1] = 10;  // todo: tune this
      outputBins[2] = 82;  // todo: tune this
      break;
    case 4:
      outputBins[0] = 3;   // todo: tune this
      outputBins[1] = 6;   // todo: tune this
      outputBins[2] = 10;  // todo: tune this
      outputBins[3] = 186;  // todo: tune this
      break;
    case 5:
      outputBins[0] = 6;   // todo: tune this
      outputBins[1] = 10;   // todo: tune this
      outputBins[2] = 20;  // todo: tune this
      outputBins[3] = 41;  // todo: tune this
      outputBins[4] = 186;  // todo: tune this
      break;
    case 6:
      outputBins[0] = 3;   // 110
      outputBins[1] = 6;   // 220
      outputBins[2] = 10;  // 440
      outputBins[3] = 20;  // 880
      outputBins[4] = 41;  // 1763
      outputBins[5] = 186;  // 7998 todo: tune this
      break;
    case 7:
      outputBins[0] = 3;   // 110
      outputBins[1] = 6;   // 220
      outputBins[2] = 10;  // 440
      outputBins[3] = 20;  // 880
      outputBins[4] = 41;  // 1760
      outputBins[5] = 82;  // 3520
      outputBins[6] = 186;  // todo: tune this
      break;
    case 8:
      outputBins[0] = 3;   // 110
      outputBins[1] = 6;   // 220
      outputBins[2] = 10;  // 440
      outputBins[3] = 20;  // 880
      outputBins[4] = 41;  // 1760
      outputBins[5] = 82;  // 3520
      outputBins[6] = 164;  // todo: tune this
      outputBins[7] = 186;  // todo: tune this
      break;
  }

  // blink the new number of outputs on all the wires
  // todo: do this in morse code?
  for (int i = 0; i < numOutputs; i++) {
    // turn all the wires on
    for (int j = 0; j < numOutputs; j++) {
      digitalWrite(outputPins[j], HIGH);
    }
    // wait
    delay(morseDitMs);
    // turn all the wires off
    for (int j=0; j<numOutputs; j++) {
      digitalWrite(outputPins[j], LOW);
    }
    // wait
    delay(morseElementSpaceMs);
  }
  // wait
  delay(morseWordSpaceMs);
}

unsigned long lastUpdate = 0;


void updateOutputStatesFromFFT() {
  // parse FFT data
  // The 1024 point FFT always updates at approximately 86 times per second.
  if (myFFT.available()) {
    int numOutputBins, nextOutputStartBin;
    int outputStartBin = FFT_IGNORED_BINS;  // ignore the first FFT_IGNORED_BINS bins as they can be far too noisy

    // set the overall sensitivity based on how loud the previous inputs were
    // todo: i think this should be an exponential moving average
    // todo: we might want to do this for the current data instead of using last data
    float inputSensitivity = lastLoudestLevel * MINIMUM_INPUT_RANGE;
    if (inputSensitivity < minInputSensitivity) {
      inputSensitivity = minInputSensitivity;
    }

    // now that we've used lastLoudestLevel, reset it to 0 so we can get the latest.
    lastLoudestLevel = 0;

    // DEBUGGING
    Serial.print("FFT >");
    Serial.print(inputSensitivity, 3);
    Serial.print(" across ");
    Serial.print(numOutputs);
    Serial.print(" outputs: | ");
    // END DEBUGGING

    float inputLevelAccumulator = 0;
    for (int outputId = 0; outputId < numOutputs; outputId++) {
      nextOutputStartBin = outputBins[outputId] + 1;
      numOutputBins = nextOutputStartBin - outputStartBin;
      if (numOutputBins < 1) {
        numOutputBins = 1;
      }

      // find the loudest bin in this outputs frequency range
      float inputLevel = 0;
      for (int binId = outputStartBin; binId < nextOutputStartBin; binId++) {
        float binInputLevel = myFFT.read(binId);

        // go numb to sounds that are constantly loud. todo: this probably needs some tuning
        avgInputLevel[binId] += NUMB_EMA_ALPHA * (binInputLevel - avgInputLevel[binId]);
        binInputLevel -= avgInputLevel[binId] * NUMB_PERCENT;

        if (binInputLevel > inputLevel) {
          inputLevel = binInputLevel;
        }
      }

      // do this by keeping a long moving average

      // save our loudest (modified) level
      if (inputLevel > lastLoudestLevel) {
        lastLoudestLevel = inputLevel;
      }

      // keep track of the sum of our modified levels so we can calculate an average later
      inputLevelAccumulator += inputLevel;

      // turn the light on if inputLevel > inputSensitivity
      if (inputLevel > inputSensitivity) {
        outputStates[outputId] = HIGH;

        // save the time this output turned on. morse code waits for this to be old
        lastOnMs = nowMs;

        Serial.print(inputLevel, 3);

        // save the time we turned on unless we are in the front X% of the minimumOnMs window of a previous on
        if (nowMs - lastOnMsArray[outputId] > minimumOnMs * 0.80) {   // todo: tune this
          // todo: instead of setting to the true time, set to some average of the old time and the new?
          lastOnMsArray[outputId] = lastOnMs;
        } else {
          // ignore this input since we recently turned this light on
        }
      } else {
        if (nowMs - lastOnMsArray[outputId] > minimumOnMs) {
          // the output has been on for at least minimumOnMs. turn it off
          // don't actually turn off here because that might break the morse code
          outputStates[outputId] = LOW;
          Serial.print("     ");   // we turned the output off. show blank space
          //Serial.print(inputLevel, 3);
        } else {
          Serial.print(" .   ");   // we left the output on
        }
      }
      Serial.print(" | ");

      // prepare for the next iteration
      outputStartBin = nextOutputStartBin;
    }
    Serial.println(nowMs - lastUpdate);
    lastUpdate = nowMs;
  }
}


bool blinkMorseCode() {
  // blink all of the wires according to morseArray
  bool morseCommandFound = false;
  unsigned long checkMs = MAX_OFF_MS;
  for (int morseCommandId = 0; morseCommandId < morseArrayLength; morseCommandId++) {
    TimedAction morseCommand = morseArray[morseCommandId];
    checkMs += morseCommand.checkMs;
    if (nowMs < checkMs) {
      if (lastMorseCommandId != morseCommandId) {
        // this is the first time we've seen this command this iteration

        // save that we've done this command already
        lastMorseCommandId = morseCommandId;

        // randomize the outputs if this is the first command
        if ((morseCommandId = 0) && (randomizeOutputs)) {
          bubbleUnsort(randomizedOutputIds, numOutputs);
        }

        // todo: only do half (rounded up) of the lights?
        for (int outputId = 0; outputId < numOutputs; outputId++) {
          digitalWrite(outputId, morseCommand.outputAction);
        }
      }

      morseCommandFound = true;
      break;
    }
  }

  // return true when we are done playing the message
  return not morseCommandFound;
}

bool morseCommandCompleted = not (bool)morseArrayLength;

void loop() {
  updateNumOutputs();

  updateOutputStatesFromFFT();

  // todo: check our buttons to see if we should update minInputSensitivity

  if (nowMs - lastOnMs < MAX_OFF_MS) {
    // we turned a light on recently. send output states to the wires
    for (int i = 0; i < numOutputs; i++) {
      digitalWrite(outputPins[i], outputStates[i]);
    }
    morseCommandCompleted = false;
  } else {
    if (morseCommandCompleted) {
      // it is quiet and we've played our morse code message. shuffle the outputs
      bubbleUnsort(randomizedOutputIds, numOutputs);
      lastOnMs = lastUpdate = 0;
      nowMs = MAX_OFF_MS;
    } else {
      // no lights have been turned on for at least MAX_OFF_MS
      // blink more code
      // todo: play the message twice?
      morseCommandCompleted = blinkMorseCode();
    }
  }
}
