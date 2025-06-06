// I'm too lazy to make my own servo drivers so we will use Arduino's
#include <Servo.h>

// Says whether or not out program should be running
bool IsRunning = true;

// An Abstraction for getting input that the eye can use
struct _AnalogEyeInput
{
  int8_t horizontalInput, blinkInput; // Has to range from -100% to 100%

  void (*inputRefreshMethod)(struct _AnalogEyeInput *); // A function pointer to a updater

  // Converts raw input values to horizontal eye values
  int8_t GetLookingHorizontalAmount() {
    return map(horizontalInput, -100, 100, -60, 60);
  }

  /*
    Converts raw input values to blink strength 
    P.S. ignore that 0 is closed and 100 is wide open,
      That's a feature not a bug ;)
  */
  float GetBlinkStrength() {
    return (blinkInput + 100) / 2;
  }
};

// Lets make this a type defination because a void (*)(struct _AnalogEyeInput *) type gets old to type
typedef void (*AnalogMethod)(struct _AnalogEyeInput *);

/* 
  An nice little abstraction for the eyeball (you can load multiple if you want)
  Too bad that will never happen >:)
*/
struct _EyeBall {

  // Added _AnalogEyeInput via composition
  struct _AnalogEyeInput input;

  // Holds the servos
  Servo blinkServo;
  Servo horizontalServo;

  // 0% = Fully Closed | 100% = Wide Open
  void BlinkAmount(float blinkStrength) {
    // Convert blink strength to an angle
    int angle = map((blinkStrength+1.f/2)*10, 0, 1000, 0, 130);
    blinkServo.write(angle);
  }

  // -60 = looking fully left | 0 = looking forward | 60 = fully right
  void LookHorizontal(int8_t amount) {

    // Enforces range by truncation
    amount = amount <= -60 ? -60 : amount;
    amount = amount >= 60 ? 60 : amount;

    // Converts the horzontal amount to a nice little angle
    // Also those numbers are specific to my build so it doesn't break
    int angle = map(amount, -60, 60, 15, 70);
    horizontalServo.write(angle);
  }

  // Closes and straightens the eye so the eyelids and eyeball can be removed
  void EjectEye(void) {
    // Reset the eye, but this also ends the program so this will happen twice lol
    blinkServo.write(0);
    LookHorizontal(0);

    // Terminates program, hit the reset button on board to well
    // Reset....
    IsRunning = false;
  }

  // Gets the input then sets the eye to use it so easy peesy lemon squeese
  void SetBasedOnInput(void) {
    // Refreshes it
    input.inputRefreshMethod(&input);
    
    // Moves eye
    BlinkAmount(input.GetBlinkStrength());
    LookHorizontal(input.GetLookingHorizontalAmount());
  }

  // A crappy auto blink implementation I made, I'd go for a interpreter appoarch if I had more time
  void AutoBlink(void) {
    // To be far idk if all eyes would be syncronized here and yes I know I made a spelling mistake
    // that was on purpose
    static uint32_t clockBlink = 0; /* 
      We need a seperate variable to hold the time for the blink because if we use millis and stop,
      when we resume the eye jumps which looks bad and best and could damage at worst
    */

    // These two variables are used to calculate delta-time so we can accurately update our blink clock
    static uint32_t lastTime = 0;
    uint32_t currentTime = millis();

    // We just set the blink amount to be based of a sine wave (because sine waves are great :D)

    float blinkAmount = (sinf(clockBlink/500.f)+1.f) / 2 * 100;

    // Every 4 seconds lets have a lot around, but don't do that also if your eyes are closed
    // That would be bad
    if (millis()/1000 % 4 != 0 || blinkAmount < 80) {
      // Normally blink
      this->BlinkAmount(blinkAmount);

      clockBlink += currentTime-lastTime;
      lastTime = currentTime;
      return;
    }

    // It is 4 seconds and we can see!
    // Lets have a look around the room :D

    float lookAroundPercentage = (millis()-(uint16_t)(millis())) / 400 % 100;

    int8_t lookHorizontalAmount = map(sinf(lookAroundPercentage)*100, -100, 100, -60, 60);

    this->LookHorizontal(sinf((millis() % 1000)/1000.f*2*PI )*60);
    lastTime = currentTime;
  }

#define AFK_THRESHOLD 7.5f
#define DRIFT_ALLOWED_PERCENTAGE 5

  // Just says where we are afk or not :)
  bool IsActive(void) {
    static float LastBlinkInput = 0;
    static float LastHorizontalInput = 0;
    static float LastInput = 0;

    // Let's just double check so see that we haven't touched the input or anything :/
    this->input.inputRefreshMethod(&this->input);

    // Lets record that last input we found
    // Also, lets checks for drift because analog can something jump a little
    LastInput = 
      abs(this->input.blinkInput-LastBlinkInput) > DRIFT_ALLOWED_PERCENTAGE || 
      abs(this->input.horizontalInput-LastHorizontalInput) > DRIFT_ALLOWED_PERCENTAGE  ?
        millis() / 1000.f : 
        LastInput;
  
    LastBlinkInput = this->input.blinkInput;
    LastHorizontalInput = this->input.horizontalInput;

    // If the last time we did something was AFK_THRESHOLD ago, we are 110% not active
    return (millis() / 1000.f - LastInput < AFK_THRESHOLD);
  } 


  // What an absolute war-crime of a name that is, naming is the hardest part of writing code
  void SetBasedOnInputPlusAutoWhenAfk(void) {
    // Seriously this is cursed on every single level >:)
    
    // If we are afk, lets run auto blink then return so we avoid nesting
    if (!IsActive()) return AutoBlink(); 

    // Let's not repeat ourselves here and just SetBasedOnInput
    SetBasedOnInput();
  }
}; // No more eyeball struct method!


// Not a fan of C++ class initalization so lets use a helper function
_EyeBall LoadEyeball(uint8_t blinkPin, uint8_t horizontalServo, AnalogMethod inputRefreshMethod) {
  _EyeBall tempEye;
  tempEye.blinkServo.attach(blinkPin);
  tempEye.horizontalServo.attach(horizontalServo);
  tempEye.input.inputRefreshMethod = inputRefreshMethod;
  return tempEye;
}

// This will never be called but just in case you know, lets just be able to delete and free an eyeball
void KillEyeball(_EyeBall *target) {  //
  target->LookHorizontal(0);
  target->blinkServo.write(0);

  delay(1000); // Gives the servos a chance to move back
  
  target->blinkServo.detach();
  target->horizontalServo.detach();
}

// A macro to make an AnalogMethod function
#define CreateInputMethod(name) void name (_AnalogEyeInput * input)

// This was for debugging
CreateInputMethod(HorizontalDebug) {
  input->horizontalInput = analogRead(A0);
}

// This is my eye
struct _EyeBall myEye;

// It's this easy to add joystick support :}
CreateInputMethod(JoystickInput) {
  // We have to make it so values range from -100% to 100% remember?
  input->blinkInput = map(analogRead(A1), 0, 1023, -100, 100);
  input->horizontalInput = map(analogRead(A0), 0, 1023, -100, 100);
}

void setup() {
  // This here is just so you can visually know what each parameter is
  // The compiler will just replace the variable there with the number
  // Instead of pushing it to the stack so no fancy keywords are needed
  int blinkPin = 6;
  int horizontalServo = 3;

  myEye = LoadEyeball(blinkPin, horizontalServo, JoystickInput);
}

void loop() {
  // All that just for this absolutely beautiful procedure call right here
  // At least it is now a piece of cake to add more eyes and play around with them :D
  myEye.SetBasedOnInputPlusAutoWhenAfk();

  delay(10); // We don't want our servos or board to blow up do we
}