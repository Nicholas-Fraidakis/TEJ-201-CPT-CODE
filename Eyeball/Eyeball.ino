/*
    Zlib License

    Copyright (c) 2025 Nicholas A. Fraidakis

    This software is provided 'as-is', without any express or implied
    warranty. In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software
        in a product, an acknowledgment in the product documentation would be
        appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
        misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/

// I'm too lazy to make my own servo drivers so we will use Arduino's
#include <Servo.h>

#pragma region MonolithicEyeballAbstraction

#pragma region CoreEyeballStruct

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
  uint8_t GetBlinkStrength() {
    return ((uint16_t)blinkInput + 100) / 2.f;
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
  void BlinkAmount(uint8_t blinkStrength) {
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
    Serial.println(input.GetBlinkStrength());
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

#pragma endregion

#pragma region OcularVirtualMachine

// The instruction set used by the Ocular Virtual Machine
enum OVM_INSTRUCTION_SET {
  HALT = 0,

  PUSHS,    // Push with size
  PUSHR,    // Push raw (can be used if the data type is >2 bytes even though it's not supported)
  REMOVE,

  PRINT,

  ADD,
  SUB,
  MUL,
  DIV,

  SBS,      // Set Blink strength
  SEH,      // Set Eye horizontal

  COMP,     // Compares and pushes flag
  CCLR,     // Clears Comparison

  JP, 

  WAIT,

  SINE,
  COS,
  TAN,

  CAI,      // Check Analog Input
};

// Comparison bit flags used in OVM conditionals
enum OVM_COMPARISON {
  NO_COMPARISON=0,
  LESS_THAN=1,
  GREATER_THAN=2,
  EQUAL_TO=4,
};

// Struct representation of an OVM instruction
struct OVM_INSTRUCTION {
  uint8_t opcode;

  union { 
    struct {
      uint8_t arg8L;
      uint8_t arg8U;
    };
    uint16_t arg16;
  };

  uint8_t argA; // additional argument
};

// Struct representation of an OVM program
struct OVM_PROGRAM {
  uint16_t length;
  struct OVM_INSTRUCTION * instructions;
};

// A macro to define how big each OVM stack should be (total mem = 2x stack size)
#define OVM_STACK_SIZE 256

// Different OVM errors and warnings
enum OVM_Err {
  GENERAL_ERROR = 1,
  STACK_OVERFLOW,
  STACK_UNDERFLOW,
  EYENULL
};

// An absolute monster of a struct to represent decoded arguments from an instruction
struct OVM_DECODED_ARGS {
  union {
    struct {
      union {
        uint8_t arg8L;
        uint8_t A;
      };
      union {
        uint8_t arg8U;
        uint8_t B;
      };
    };
    uint16_t arg16;
    uint16_t AB;
  };
  union {
    uint8_t OPERAND_SIZE;
    uint8_t argA;
    uint8_t C;
  };
  uint8_t CONDITIONAL;
};

// An OVM instance def
struct OVM {
  bool IS_RUNNING = true;
  struct _EyeBall * EYE_TARGET;

  uint8_t DATA_STACK[OVM_STACK_SIZE]; // holds raw data
  uint8_t SIZE_STACK[OVM_STACK_SIZE]; // holds element sizes

  void * STACK_POINTER; // Points to top of stack

  uint16_t ELEMENTS_ALLOCATED; // the number of allocated elements

  struct OVM_PROGRAM PROGRAM;

  uint16_t PROGRAM_COUNTER;

  enum OVM_INSTRUCTION_SET OPCODE;
  uint8_t CONDITION;

  struct OVM_DECODED_ARGS OPERANDS;

  // Prints a panic and terminates
  void panic(const char * msg, const enum OVM_Err err_code) {
    Serial.print("O.V.M Panic: ");
    Serial.println(msg);

    Serial.print("Program Counter: ");
    Serial.println(PROGRAM_COUNTER);

    Serial.print("Terminating Program with Error Code: ");
    Serial.println(err_code);
    exit(err_code);
  }

  // Prints a warning
  void warning(const char * msg, const enum OVM_Err err_code) {
    Serial.print("O.V.M Warning: ");
    Serial.println(msg);

    Serial.print("Program Counter: ");
    Serial.println(PROGRAM_COUNTER);

    Serial.print("Warning Error Code: ");
    Serial.println(err_code);
  }

  // Print info 
  void info(const char * msg) {
    Serial.print("O.V.M Info: ");
    Serial.println(msg);

    Serial.print("Program Counter: ");
    Serial.println(PROGRAM_COUNTER);
  }

  // Pushs an element onto the stack
  void push_with_size(uint16_t data, uint8_t size) {
    if ((uint16_t)((char*)STACK_POINTER - (char*)DATA_STACK) + size >= OVM_STACK_SIZE) {
      panic("Stack Overflow!", STACK_OVERFLOW);
    }

    if (size == 1) {
      *(uint8_t*)(STACK_POINTER) = data;
      STACK_POINTER = (void*)((uint8_t *)STACK_POINTER + 1);
    } else if (size == 2) {
      *(uint16_t*)(STACK_POINTER) = data;

      STACK_POINTER = (void*)((uint8_t *)STACK_POINTER + 2);
    } else {
      panic("You might want to sit down for this... things can only be 8 or 16 bit >:)", GENERAL_ERROR);
    }

    SIZE_STACK[ELEMENTS_ALLOCATED] = size;
    ELEMENTS_ALLOCATED++;
  }

  void push_raw(uint16_t data, bool single_byte) {
    panic("Deprecated!",GENERAL_ERROR);
  }

  // Returns the last element pushed to staack without poping it
  uint16_t get_element(void) {
    if (SIZE_STACK[ELEMENTS_ALLOCATED-1] == 2) {
      return *(uint16_t*)((uint8_t*)(STACK_POINTER)-2);
    }
    return *((uint8_t*)(STACK_POINTER)-1);
  }

  // Frees stack mem
  void remove_element(void) {
    if ((uint16_t)((char*)STACK_POINTER - (char*)DATA_STACK) - SIZE_STACK[ELEMENTS_ALLOCATED-1] >= OVM_STACK_SIZE) {
      ELEMENTS_ALLOCATED = 0;
      SIZE_STACK[0] = 0;

      STACK_POINTER = DATA_STACK;

      return warning("Stack Underflow, Reseting Stacks!", STACK_UNDERFLOW);
    }

    uint8_t * TOP_STACK = (uint8_t*)(STACK_POINTER);
    TOP_STACK -= SIZE_STACK[ELEMENTS_ALLOCATED-1];
    STACK_POINTER = TOP_STACK;

    ELEMENTS_ALLOCATED--;
  }

  // Pops element from stack
  uint16_t pop_element(void) {
    uint16_t element = get_element();
    remove_element();
    return element;
  }

  // fetches and returns instruction
  OVM_INSTRUCTION fetch(void) {
    return PROGRAM_COUNTER < PROGRAM.length ? 
      PROGRAM.instructions[PROGRAM_COUNTER++] : 
      (OVM_INSTRUCTION){.opcode=HALT};
  }

  // Decodes the arguments from an instruction
  void decode(OVM_INSTRUCTION instruction) {
    OPCODE = (enum OVM_INSTRUCTION_SET)instruction.opcode;
    
    OPERANDS = (struct OVM_DECODED_ARGS){0};

    switch (OPCODE) {
      case PUSHS:
      case PUSHR:
        OPERANDS.AB = instruction.arg16;
        OPERANDS.OPERAND_SIZE = instruction.argA;
        break;
      
      case ADD:
      case SUB:
      case MUL:
      case DIV:
        OPERANDS.A = instruction.arg8L;
        OPERANDS.B = instruction.arg8U;
        OPERANDS.CONDITIONAL = instruction.argA;
        break;

      case WAIT:
        OPERANDS.AB = instruction.arg16;
        OPERANDS.C = instruction.argA;
        break;
      case JP:
        OPERANDS.AB = instruction.arg16;
        OPERANDS.CONDITIONAL = instruction.argA;
        break;

      case COMP:
        OPERANDS.AB = instruction.arg16;
        OPERANDS.C = instruction.argA;
        break;

      case SEH:
      case SBS:
        OPERANDS.AB = instruction.arg16; 
        OPERANDS.C = instruction.argA;
      case CAI:
        OPERANDS.AB = instruction.arg16;
        OPERANDS.C = instruction.argA;
      default:
        break;
    }
    
  }

  // runs the instruction
  void execute(void) {
    if (OPERANDS.CONDITIONAL && (CONDITION & OPERANDS.CONDITIONAL) == 0) {
      return;
    }

    switch (OPCODE) {
      case HALT:
        IS_RUNNING = false;
        info("Program Has Halted!");
        break;
      case PRINT:
        Serial.println(get_element());
      case PUSHS:
        push_with_size(OPERANDS.AB, OPERANDS.OPERAND_SIZE);
        break;

      case PUSHR:
        push_raw(OPERANDS.AB, (bool)OPERANDS.C);
        break;

      case REMOVE:
        remove_element();
        break;

      case ADD: {
        if (ELEMENTS_ALLOCATED <= 1) panic("Stack Underflow!", STACK_UNDERFLOW);

        uint16_t n1 = pop_element();
        Serial.println(n1);

        uint16_t n2 = pop_element();
        Serial.println(n1);
        uint16_t number = n1 + n2;

        push_with_size(number, 2);
        break;
      }

      case SUB: {
        if (ELEMENTS_ALLOCATED == 0) panic("Stack Underflow!", STACK_UNDERFLOW);

        uint8_t size = SIZE_STACK[ELEMENTS_ALLOCATED] > SIZE_STACK[ELEMENTS_ALLOCATED - 1] ? 
          SIZE_STACK[ELEMENTS_ALLOCATED] : SIZE_STACK[ELEMENTS_ALLOCATED - 1];

        uint16_t number = pop_element() - pop_element();

        push_with_size(number, size);
        break;
      }

      case MUL: {
        if (ELEMENTS_ALLOCATED == 0) panic("Stack Underflow!", STACK_UNDERFLOW);

        uint8_t size = SIZE_STACK[ELEMENTS_ALLOCATED] > SIZE_STACK[ELEMENTS_ALLOCATED - 1] ? 
          SIZE_STACK[ELEMENTS_ALLOCATED] : SIZE_STACK[ELEMENTS_ALLOCATED - 1];

        uint16_t number = pop_element() * pop_element();

        push_with_size(number, size);
        break;
      }

      case DIV: {
        if (ELEMENTS_ALLOCATED == 0) panic("Stack Underflow!", STACK_UNDERFLOW);

        uint8_t size = SIZE_STACK[ELEMENTS_ALLOCATED] > SIZE_STACK[ELEMENTS_ALLOCATED - 1] ? 
          SIZE_STACK[ELEMENTS_ALLOCATED] : SIZE_STACK[ELEMENTS_ALLOCATED - 1];

        uint16_t number = pop_element() / pop_element();

        push_with_size(number, size);
        break;
      }

      case WAIT:
        if (OPERANDS.C) {
          if (ELEMENTS_ALLOCATED == 0) panic("Stack Underflow!", STACK_UNDERFLOW);

          delay(pop_element());
          break;
        }
        
        delay(OPERANDS.AB);
        break;

      case JP:
        PROGRAM_COUNTER = OPERANDS.AB;
        break;
      
      case COMP: {
        if (ELEMENTS_ALLOCATED == 0) panic("Stack Underflow!", STACK_UNDERFLOW);

        uint16_t c1 = pop_element();

        uint16_t c2 = OPERANDS.AB;

        if (OPERANDS.C) {
          if (ELEMENTS_ALLOCATED == 0) panic("Stack Underflow!", STACK_UNDERFLOW);

          c2 = pop_element();
        }
        
        if (c1 > c2) CONDITION = GREATER_THAN;
        else if (c1 < c2) CONDITION = LESS_THAN;

        if (c1 == c2) CONDITION |= EQUAL_TO;
        break;
      }

      case CCLR:
        CONDITION = 0;
        break;

      case SBS:
        if (!EYE_TARGET) panic("EYE_TARGET is NULL!", EYENULL);

        if (OPERANDS.C) 
        {
          uint16_t num = pop_element();
          EYE_TARGET->BlinkAmount(num);
          break;
        }

        EYE_TARGET->BlinkAmount(OPERANDS.AB);

        break;

      case SEH:
      info("ERR");
        if (!EYE_TARGET) panic("EYE_TARGET is NULL!", EYENULL);

        if (OPERANDS.C) return EYE_TARGET->LookHorizontal(pop_element());

        EYE_TARGET->LookHorizontal(OPERANDS.AB);

        break;

      case CAI:
      info("ERR");
        if (!OPERANDS.C) return push_with_size((uint16_t)analogRead(OPERANDS.AB), 2);

        uint16_t num = pop_element();
        push_with_size((uint16_t)analogRead(A0+num), 2);
        break;
    }
  }

  // Runs an entire cycle
  void run_cycle(void) {
    if (!IS_RUNNING) return;
    
    struct OVM_INSTRUCTION instruction = fetch();
    decode(instruction);
    execute();
  }
};

// Safely initalizes a OVM instance
void OVM_Init(struct OVM * target, struct _EyeBall * eye_target, struct OVM_PROGRAM program) {
  target->EYE_TARGET = eye_target;

  target->STACK_POINTER = target->DATA_STACK;

  target->push_with_size(40, 1);

  target->PROGRAM = program;
}

#pragma endregion

#pragma endregion
#pragma region UserProgram

// This is my eye
struct _EyeBall myEye;
struct OVM myOVM;

// this is debug test stuff
struct OVM_INSTRUCTION myProgramInstructions[100] = {
};

// Unfounately the version of AVR-GCC in the Arduino IDE is incredibly outdated (either 7 or 12)
// The point is it does not fully conform to modern C/C++ standards so I can't initalize the array
// Via non-trivial designated initalizers due to them not being supported :(

// also debug and testing stuff
void LoadProgram() {
  myProgramInstructions[0].opcode = PUSHS;
  myProgramInstructions[0].arg16 = 120;
  myProgramInstructions[0].argA = 1;

  myProgramInstructions[1].opcode = PUSHS;
  myProgramInstructions[1].arg16 = 460;
  myProgramInstructions[1].argA = 2;

  myProgramInstructions[2].opcode = DIV;
  myProgramInstructions[2].arg16 = 0;
  //myProgramInstructions[2].argA = 1;

  myProgramInstructions[3].opcode = PRINT;
  //myProgramInstructions[3].argA = 1;

  myProgramInstructions[4].opcode = HALT;
}

struct OVM_PROGRAM myProgram = (struct OVM_PROGRAM) {
  sizeof(myProgramInstructions) / sizeof(struct OVM_INSTRUCTION),
  (struct OVM_INSTRUCTION*)&myProgramInstructions,
};

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

  Serial.begin(9600);
  int blinkPin = 6;
  int horizontalServo = 3;

  myEye = LoadEyeball(blinkPin, horizontalServo, JoystickInput);


  LoadProgram();

  OVM_Init(&myOVM, NULL, myProgram);

  myEye.EjectEye();
}

void loop() {
  // All that just for this absolutely beautiful procedure call right here
  // At least it is now a piece of cake to add more eyes and play around with them :D

  myEye.SetBasedOnInputPlusAutoWhenAfk();

  delay(10); // We don't want our servos or board to blow up do we
}

#pragma endregion