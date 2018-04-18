//October 29 2016 Parker Savka. Testing making changes via GitHub v6.3
//October 20 2016 Parker Savka. FOR GRAYHILL OPTICAL ENCODER: Removed "half-tick" code block, only applied to SAB encoder. New encoder increments correctly without it. Version 6.2 v2
//October 17 2016 Parker Savka. FOR BOURNES OPTICAL ENCODER: Added support for new optical encoders to replace SAB. Also removes pump priming to avoid wear. Version 6.2
// June 30 2016 Parker Savka. Added logic to stop motor PWM from going over 255
// June 19 2015 James Nation. Fixed minor errors, adjusted hydrograph run time to properly match name. Fixed pump priming so it properly primes the pump.
// Jan 8, final, tests, a few minor treaks to hydro adjust; all working well.  VERSION 6.0
//Jan 8, line by line comparison, fixed all bugs I think; also set code so encoder/pwm jump from 2 - 45 going up or down.
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC   <<<<<Comment out these in final; they are Serial.print commands
//Jan 7, tediously backtracking; introduced bug somehow.
//Jan 7; added hydro curve shift; left pwm_ and associated variables as "int" instead of changing to uint8_t; I fear
//this was causing the bug that made pwm_ values read out as gibberish (letters and characters)
//Rev. v 5.0; Alix made hydrograph step lenght default 30 seconds; UNTESTED
//Rev. Jan 4, 2012 with new hydrograph values.
//Rev. Sep 24, 2011 v. 9_24_2011_V 1.0
//Testing and working very nicely on Sep 24.
//Adding hydrographs crashes Arduino and appears to destroy bootloader; Nov 7.
//Fixed all problems Nov 7, 2011.
//Version 4.0  [revise software version in code about line 750 to read out on startup]


#include <LiquidCrystal.h>

// The maximum number of hydrographs supported (needed to size the hydrographs menu).
#define  MAX_HYDROGRAPHS    20

// The number of seconds we spend in each hydrograph step.
#define	HYDROGRAPH_STEP_SECONDS 30

// LCD display using 4-bit interface.
LiquidCrystal lcd(9, 8, 4, 5, 6, 7);

//
// Motor PCM control and flow calculations.

// The value of the millisecond clock when the cumulative totals were reset.
// This is used to compute the elapsed time shown on line 2 of the display.
uint32_t cumulative_start_ms = 0;

// The total volume in mL pumped since cumulative_start_time was reset.  
//BUG; this variable was set at "int" by Gough and was causing overflow problems.
uint32_t cumulative_volume_ml = 0;

// The current motor PWM setting (0 = off, 255 = maximum duty cycle).
int current_motor_pwm = 0;						//1_7 BUG may've been caused by changing this and pwm_, etc. to uint8_t variable,
//which is NOT the same at "int."

// The millisecond counter when we last accumulated flow.
static uint32_t last_flow_update_ms = 0;

// The millisecond counter when we last updated the display (0 = update immediately).
static uint32_t last_display_update_ms = 0;

//pwm calibration value, set with menu
//value of 95 gives a default curve; can go as low as 70.
//1_7 BUG  TODO  Check this; causing problems with curve shifting and pwmCal?  
int pwmCal = 95;

// Flow measurements for each of the 256 PCM settings, expressed in mL/sec.
int ml_per_sec_by_pwm[256];

int hydro_curve_shift = 0;    //value by which to shift pwm values in hydrographs depending on pwmCal value
// is = to default pwm value for curves - pwmCal (e.g. 95 - 5 = 30)
//this could have caused BUG; where is default pwm set?  What if default pwm is > hydro_curve_shift?
//would get negative overflow wiht int variable.
//BUG problem possibly

void set_motor_pwm(int pwm_)  //1_7 BUG may have also been caused by changing these to uint8_t; "int" version was working

{
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC
  // Serial.print ("set motor PWM called with pwm_ =  ");
  // Serial.println (pwm_);
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC

  // Remember the setting.
  current_motor_pwm = pwm_;

  // Set the PWM duty cycle for pin 3 (MPWM_D3 signal).
  analogWrite(3, pwm_);

  // Trigger an immediate display update.
  last_display_update_ms = 0;
}  

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reset the cumulative time and volume counters.
void reset_counters()
{
  cumulative_volume_ml  = 0;
  cumulative_start_ms = last_flow_update_ms = millis();
  last_display_update_ms = 0;
}

// Print an arbitrary number of digits with a leading character.
static void print_digits(uint32_t value_, uint8_t digits_, char lead_)
{
  // Print leading characters.
  uint32_t power = 10;
  for (uint8_t digit = 1; digit < digits_; digit++) {
    if (value_ < power) {
      lcd.print(lead_);
    }
    power *= 10;
  }

  // Print the value.
  lcd.print(value_, DEC);
}

// Print the accumulated accumulated flow and time.
static void print_flow_and_time()
{
  uint32_t instant_ml_per_sec;
  uint32_t total_liters, total_ml;
  uint32_t seconds, hours, minutes;

  // Compute current flow in ml/sec.
  instant_ml_per_sec = ml_per_sec_by_pwm[current_motor_pwm];

  // Compute total flow in liters and ml.
  total_ml = cumulative_volume_ml;
  total_liters = total_ml / 1000;
  total_ml -= 1000 * total_liters;

  // Compute elapsed hours, minutes and seconds.
  seconds = (millis() - cumulative_start_ms) / 1000;
  hours = seconds / 3600;
  seconds -= 3600 * hours;
  minutes = seconds / 60;
  seconds -= 60 * minutes;

  // Clear the display and go to the upper left corner.
  lcd.clear();

  // Print current flow as VVVmL/sec to line up with total volume below.
  lcd.print("Flow ");
  print_digits(instant_ml_per_sec, 3, ' ');
  lcd.print("mL/s");
  
  //  DEGUB
  

  // Go to the second line of the display.
  lcd.setCursor(0, 1);

  // Print time and volume as VVVV.VL HH:MM:SS
  print_digits(total_liters, 4, ' ');
  lcd.print(".");
  print_digits(total_ml / 100, 1, '0');
  lcd.print("L ");
  print_digits(hours, 2, '0');
  lcd.print(":");
  print_digits(minutes, 2, '0');
  lcd.print(":");
  print_digits(seconds, 2, '0');
}

// Update the total flow, and update the display if necessary.
void update_flow()
{
  uint32_t elapsed_ms;

  // Accumulate flow four times a second. GOUGH; changed to 2x per second by inserting 500 below
  elapsed_ms = millis() - last_flow_update_ms;
  if (elapsed_ms >= 500) {
    cumulative_volume_ml += (elapsed_ms * ml_per_sec_by_pwm[current_motor_pwm] / 1000);
    last_flow_update_ms = millis();
  }

  // Update the display twice per second.
  elapsed_ms = millis() - last_display_update_ms;
  if ((last_display_update_ms == 0) || (elapsed_ms >= 500)) {
    print_flow_and_time();
    last_display_update_ms = millis();
  }    
}

//
// Beeper handling.
//

// If non-zero, the duration of the beep.
uint32_t beep_duration_ms = 0;

// The millisecond counter at the time the beeper was turned on.
uint32_t beep_start_ms = 0;

// Turn on the beeper.  Gough:  Note this is NOT a blocking function; it runs in background

void beep(uint16_t duration_ms_)
{
  // Save the requested duration.
  beep_duration_ms = duration_ms_;

  // Remember when we turned the beeper on.
  beep_start_ms = millis();
  if (beep_start_ms == 0) {
    beep_start_ms = 1;
  }

  // Turn the beeper on (SPWM_D10).  A 50/255 duty cycle seems to work well.
  analogWrite(10, 50);  
}

// Check to see if it is time to turn the beeper off.
void check_beeper()
{
  if ((beep_duration_ms != 0) && ((millis() - beep_start_ms) > beep_duration_ms)) {

    // Time's up; turn beeper off.
    beep_duration_ms = 0;
    analogWrite(10, 0);
  }
}

//
// Encoder handling.
//

// The encoder count value.
static int16_t encoder_count = 0;

// The sequence of gray codes in the increasing and decreasing directions.
static uint8_t cw_gray_codes[4] = { 
  2, 0, 3, 1 };
static uint8_t ccw_gray_codes[4] = { 
  1, 3, 0, 2 };

// The intermediate delta in encoder count (-1, 0, or +1), incremented or decremented halfway between detents.
static int8_t half_ticks = 0;

// The gray code we last read from the encoder.
static uint8_t previous_gray_code = 0;

// Reset the encoder to zero.
static void reset_encoder()
{
  encoder_count = 0;
  half_ticks = 0;
}

// Look for encoder rotation, updating encoder_count as necessary.
static void check_encoder()
{
  // Get the Gray-code state of the encoder.
  // A line is "low" if <= 512; "high" > 512 or above.
  uint8_t gray_code = ((analogRead(3) > 512) << 1) | (analogRead(4) > 512);

  /*From Chris "It is a bitwise OR, so it is not limited to 0 and 1.  The <<1 is a multiply by 2, making the first operand either 0 or 2.  So it is basically looking at two signals.  Analog input 3 is the top bit and analog input 4 is the bottom bit.  The >512 is because the signals range from 0..1023, since I am using an analog input as a digital input."
   */

  // If the gray code has changed, adjust the intermediate delta. 
  if (gray_code != previous_gray_code) {

    // Each transition is half a detent.
    if (gray_code == cw_gray_codes[previous_gray_code]) {
      encoder_count++;
    } 
    else if (gray_code == ccw_gray_codes[previous_gray_code]) {
      encoder_count--;
    }
    // Remember the most recently seen code.
    previous_gray_code = gray_code;
  }
}

//
// Switch handling.
//

static uint8_t switch_was_down = 0;

static uint8_t switch_released()
{
  // The switch is depressed if its sense line is high (>512).
  uint8_t switch_is_down = (analogRead(5) > 512);

  // The action takes place when the switch is released.
  uint8_t released = (switch_was_down && !switch_is_down);

  // Remember the state of the switch.
  switch_was_down = switch_is_down;

  // Was the switch just released?
  return released;
}

//
// Hydrograph handling.
//

// The definition of a hydrograph.
//Gough: "struct" is a multi-item variable; see Arduino ref; 
//Jan 7 update; to save RAM Alix rewrote so this variable only has pwm values and step time is 30 seconds.

struct hydrograph_t
{
  // The name of the hydrograph as shown in the menu.
  const char* name;					// pointer 
  // Pointer to the first motor PWM value (1-255) for the hydrograph.
  // The hydrograph ends when a PWM value of 0 is encountered.
  const uint8_t* steps;      //+++++++++++++++++different phrasing in non-default-30-second version

};

// NOTE:  To add a hydrograph:
//
// 1.  Define an array below of type uint8_t containing PWM values.  The array must end with a 0 entry.
// 2.  Add an entry to the "hydrographs" array pointing to the array defined in step 1.
// 3.  Make sure that MAX_HYDROGRAPHS is still large enough to account for all the hydrographs.

//All hydrographs updated to lower values Jan 4, 2012, S. Gough
//Alix made hydrograph step length default 30 seconds to save memory space Jan 4.

//Hydrograph 1 Flashy 8m
static const uint8_t hydrograph_1[] = {
  110, 123, 172, 166, 144, 138, 135, 130, 123, 119, 118, 114, 112, 110, 103, 101, 00
};

//Hydrograph 2 Classic hiQ 17m
static const uint8_t hydrograph_2[] = {
  105, 108, 111, 116, 118, 123, 128, 133, 144, 167, 183, 193, 191, 188, 169, 161, 149, 136, 134, 133, 131, 127, 125, 122, 120, 118, 116, 115, 113, 111, 110, 108, 106, 99, 00
};

//Hydrograph 3 megaflood 16m
static const uint8_t hydrograph_3[] = {
  110, 124, 144, 184, 213, 241, 255, 255, 255, 255, 255, 255, 248, 241, 235, 225, 215, 205, 198, 188, 178, 155, 143, 135, 128, 123, 119, 115, 114, 114, 112, 112, 00
};

//Hydrograph 4 classic loQ 18m
static const uint8_t hydrograph_4[] = {
  103, 105, 113, 117, 123, 135, 152, 178, 148, 133, 129, 127, 125, 122, 148, 133, 129, 127, 125, 122, 120, 119, 118, 117, 115, 114, 113, 112, 110, 109, 107, 106, 103, 102, 101, 99, 00
};

//Hydrograph 5 flashy 5m
static const uint8_t hydrograph_5[] = {
  101, 208, 255, 248, 198, 169, 133, 119, 114, 110, 00
};

//Hydrograph 6 regulated 19m
static const uint8_t hydrograph_6[] = {
  110, 114, 114, 125, 125, 125, 125, 125, 125, 114, 114, 125, 125, 125, 125, 125, 114, 114, 114, 114, 125, 125, 125, 125, 114, 114, 114, 114, 114, 125, 125, 125, 125, 114, 114, 114, 125, 125, 00
};

// Master list of hydrographs.
static const struct hydrograph_t hydrographs[] =
{
  { 
    "flashy 8m",       hydrograph_1   }
  ,
  { 
    "classic hiQ 17m",         hydrograph_2   }
  ,
  { 
    "megaflood 16m",        hydrograph_3   }
  ,
  { 
    "classic loQ 18m",        hydrograph_4   }
  ,
  { 
    "flashy 5m",      hydrograph_5   }
  ,
  { 
    "regulated 19m",      hydrograph_6   }
};

// The size of the hydrograph array (the number of hydrographs).
static const uint8_t hydrograph_count = sizeof(hydrographs) / sizeof(struct hydrograph_t);

// The current hydrograph step.
static const uint8_t* current_hydrograph_step = NULL;  //BUG  differs from "magic" version; but this may be where Chris found an error when he changed hydrograph function

// The millisecond counter at which the current hydrograph step began.
static uint32_t current_hydrograph_step_start_ms = 0;

//--------------------------------------------

// Start a new hydrograph step.
//run_hydrograph keeps time and calls this function when step time
//has elapsed
//Gough; The first line declares a new cont struct variable "step_"
//but it looks as though the function returns nothing so why the "void?"
//Wait, it *accepts* step_ ,but returns nothing maybe?  
//YES -- run_hydrograph increments this and sends to new_hydrograph_step   Nov 15.

void new_hydrograph_step(const uint8_t* step_)  //This is the line Alix fixed after hydrograph shortening to pwm only
{
  beep(700);
  // Remember when we started the hydrograph.
  current_hydrograph_step_start_ms = millis();

  // Remember the current step.
  current_hydrograph_step = step_;

  // Fire up the motor for this step.
  //set_motor_pwm(*step_);  // original Alix code
  if (*step_ - hydro_curve_shift >= 255) {
    set_motor_pwm(255);                          //Savka - 6.2 - Stops hydrograph from setting PWM above 255
  }
  else{
  set_motor_pwm(*step_ - hydro_curve_shift);
  }  ///Added Gough 1_7 to adjust Qvals to low pwm values in Em3
  //Since curve is calibrated for zero flow at pwm = 95; then
  //hdro_curve_shift is = to 95-pwmCal.
}
//------------------------------------------------------------
// Start a new hydrograph.
void init_hydrograph(const struct hydrograph_t* hydrograph_)
{
  // Reset the cumulative counters.
  reset_counters();

  // Start with the first step of the selected hydrograph.
  new_hydrograph_step(hydrograph_->steps);
}
//-------------------------------------------------------------

// Service the hydrograph, proceeding to the next step as needed, and updating the display.
void run_hydrograph()
{
  // Already at the end?
  if (current_hydrograph_step == NULL) 
  {
    lcd.setCursor (0,0);			//Display holds with "hydrograph done" 
    lcd.print("Hydrograph done ");

    return;
  }

  // We stop once we get to a step with a zero PWM value.
  if (*current_hydrograph_step <= 0) {       //1_7 Gough changed from == to <= because curve shift routine
    //will produce negative number when 0 at end of hydrograph
    //comes up.  Seems to work OK but might cause bugs; this
    //will step to null if value is zero or less
    set_motor_pwm(0);

    // Beep to let them know it's done.
    beep(1000);

    // Nothing more to do.
    current_hydrograph_step = NULL;
  }

  // Time to go on to the next step?
  else if ((millis() - current_hydrograph_step_start_ms) >  (1000 * HYDROGRAPH_STEP_SECONDS)) {
    new_hydrograph_step(current_hydrograph_step + 1);

    //if seconds value has elapsed, increment step pointer by 1 and run new_hydrograph_step    
    // Beep to let them know it's a new step.
    beep(1000);
  }

  // Accumulate volume and update the display.
  else {
    update_flow();
  }
}

//
// Free run mode.
//

// Go into free run mode (counter starts ticking, motor stops).
void init_free_run()
{
  // reset_counters(); //Gough removed reset counters
  // set_motor_pwm(90);  // primes pump on startup, only initiation 
  reset_encoder();

}

// Service free-run mode, changing motor speed as requested, and updating total volume and the display.
//-----------------------------------------------
void run_free_run()

{
  // Limit the encoder count to the range 0-255 and jump from 2 to 45 going up or down; 
  // this is the "dead band" the pump needs to make enough head fill about half of
  // the plumbing system

  if (encoder_count < 0) 
  {                           //Changed by sg from 0 to 45; this primes system so 20 turns aren't
    encoder_count = 0;        //needed to initiate flow, this is more like 60 for the Em2; lower
    //for the Em3 because flow feeds from bottom of box and doesn't go over gunwale.
  }

  //Commented this out; may put back in later; it sends motor pwm to zero

  if ( encoder_count > 2 && encoder_count < 44)	//Any encoder count between 2 and 44 is set to 45
  {
    encoder_count = 45;
  }
  if (encoder_count <45 && encoder_count >2)	//And this one jumps down to zero after user goes less than 45
  {					//encoder count can only get past first two statements
    //if it's being turned down (I hope) 
    //so any encoder count between 44 and 2 is set to zero (off)
    encoder_count = 0;
  }

  if (encoder_count > 255) 
  {
    encoder_count = 255;
  }

  // If the user has changed the encoder value, update the motor speed.

  if (encoder_count != current_motor_pwm)
  {
    set_motor_pwm(encoder_count);
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC  
     Serial.print ("pwm ");    ///Gough; calibration print
     Serial.print (current_motor_pwm);
     Serial.print ("  Q ");
     Serial.print (ml_per_sec_by_pwm[current_motor_pwm]);
     Serial.print ("   pwmCal  ");
     Serial.println (pwmCal);
     
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC
  }

  // Accumulate volume and update the display.
  update_flow();
}

//
// Menu handling.
//

struct menu_item_t
{
  const char* text;
  uint8_t code;
};

//
// Menu handling.
//

// Which menu are we working with?
static const struct menu_item_t* current_menu = NULL;

// Zero-based index of the first-displayed item in the menu.
static uint8_t first_displayed_menu_item = 0;

// Zero-based index of the currently selected item in the menu,
static uint8_t selected_menu_item = 0;

// The number of items in the menu.
static uint8_t current_menu_count = 0;

static void draw_menu_item(int line)
{
  lcd.setCursor(0, line);
  lcd.print((first_displayed_menu_item + line) == selected_menu_item ? ">" : " ");
  lcd.print(current_menu[first_displayed_menu_item + line].text);
}

static void draw_menu()
{
  lcd.clear();
  for (int line = 0; line < 2; line++) {
    draw_menu_item(line);
  }
}

static void init_menu(const struct menu_item_t* menu_)
{
  // Remember which menu we are working with.
  current_menu = menu_;

  // Start at the top of the menu.
  first_displayed_menu_item = selected_menu_item = 0;

  // Reset the encoder counter.
  reset_encoder();

  // Count the number of menu items.
  for (current_menu_count = 0; current_menu[current_menu_count].text != NULL; current_menu_count++);

  draw_menu();
}

static void run_menu()
{
  // Constrain the counter to the menu length.
  if (encoder_count < 0) {
    encoder_count = 0;
  }
  if (encoder_count >= current_menu_count) {
    encoder_count = current_menu_count - 1;
  }

  // Has the menu selection changed?
  if (encoder_count != selected_menu_item) {

    // If the new selected item is already on the display, just change the selection.
    if ((first_displayed_menu_item <= encoder_count) && (encoder_count <= (first_displayed_menu_item + 1))) {
      selected_menu_item = encoder_count;
    }

    // Otherwise, are we moving the selection toward the top of the menu?
    else if (encoder_count < selected_menu_item) {

      // Put the new selection at the top of the display.
      first_displayed_menu_item = encoder_count;
      selected_menu_item = encoder_count;
    }

    // Otherwise, we are moving the selection toward the bottom of the menu.
    else {

      // Put the new selection it at the bottom of the display.
      first_displayed_menu_item = encoder_count - 1;
      selected_menu_item = encoder_count;
    }

    // Update the menu.
    draw_menu();
  }
}

//
// Main menu.
//

// Main menu items.
enum {
  MAIN_EXIT,
  MAIN_RESET,
  MAIN_FREE_RUN,
  MAIN_FULL_POWER,
  MAIN_HYDRO,
  MAIN_CALIBRATE
};

static const struct menu_item_t main_menu[] =
{
  { 
    "Exit",             MAIN_EXIT   }
  ,
  { 
    "Reset counters",   MAIN_RESET   }
  ,
  { 
    "Turn flow off",         MAIN_FREE_RUN   }
  ,	//"off" because this sets pwm to zero the most convenient way
  { 
    "Full power",		MAIN_FULL_POWER   }
  ,
  { 
    "Hydrographs",   MAIN_HYDRO   }
  ,
  { 
    "Calibrate", 	MAIN_CALIBRATE   }
  ,
  { 
    NULL,  0   }
};

//
// Hydrograph menu.
//

// Hydrograph menu items.
enum {
  HYDRO_EXIT,
  HYDRO_NEW          // first of hydrograph_count items
};

// The hydrograph menu, dynamically initialized in init_hydrograph_menu().
// It includes room for the "Exit" item and the { NULL , 0 } terminator.
static struct menu_item_t hydrograph_menu[MAX_HYDROGRAPHS + 2];

// Initialize the hydrograph menu (call one time only).
void init_hydrograph_menu()
{
  // A. the "Exit" item.
  hydrograph_menu[0].text = "Exit";
  hydrograph_menu[0].code = HYDRO_EXIT;

  // Add the hydrograph entries themselves.
  for (uint8_t hydrograph = 0; hydrograph < hydrograph_count; hydrograph++) {
    hydrograph_menu[hydrograph + 1].text = hydrographs[hydrograph].name;
    hydrograph_menu[hydrograph + 1].code = HYDRO_NEW + hydrograph;
  }

  // Add the NULL termination.
  hydrograph_menu[hydrograph_count + 1].text = NULL;
  hydrograph_menu[hydrograph_count + 1].code = 0;
}  

//
// Main program.
//

// Primary modes:
//
// 1.  Free run mode.  Dial controls flow.  Top line shows 
// 2.  Main menu.
// 3.  Hydrograph menu.
// 4.  Running hydrograph.

enum {
  MODE_FREE_RUN,
  MODE_MAIN_MENU,
  MODE_FULL_POWER,
  MODE_HYDROGRAPH_MENU,
  MODE_HYDROGRAPH,
  MODE_CALIBRATE
};

// The mode we are currently in (one of the MODE_ constants).
static uint8_t mode;

// The mode which we were in before we went to the menus (one of the MODE_ constants).
static uint8_t previous_mode;

// The encoder count before we went to the menus.
static int16_t previous_encoder_count;

void setup()
{

  // Initialize the beeper PWM frequency.
  // Mode 2 = divisor of 8 = 3906.25 Hz (4K is optimal for beeper)
  TCCR1B = TCCR1B & 0b11111000 | 0x02;

  // initialize the serial communications, added by GOUGH:

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC
   //Serial.begin(9600);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC
  // Initialize the LCD and show the startup message.
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Emriver digital");
  lcd.setCursor(0, 1);
  lcd.print("flow controller");
  tone(10,1000);			//bunch of beep/boops put here by Steve
  //nice to have a unique startup noise
  delay(250);
  noTone(10);
  tone(10,4000);
  delay(250);
  noTone(10);
  delay(1000);
  lcd.clear();
  lcd.print("software version");
  lcd.setCursor(0, 1);
  lcd.print("  ---- 6.3 ----");
  tone(10,1000);
  delay(100);
  noTone(10);
  tone(10,4000);
  delay(100);
  noTone(10);
  delay (700);
  
  digitalWrite(A5, HIGH); //PAS 10.17.16 - Changes analog 5 to use internal pullup. Having this pin as an output allows the other switch lead to be wired to ground, with this pin pulling low when the button is pressed.
  // Do one-time initializations of the hydrograph menu.
  init_hydrograph_menu();

  // Do initializations of the default pwm-to-flow table.
  init_flow_table();

  // Start up in free-run mode.
  init_free_run();
  mode = MODE_FREE_RUN;

  //prime the pump so many turns of knob aren't needed for first flow
  //encoder_count =(60);  //PAS 10.5.16 - commented to preserve pump motors
}

void loop()
{
  uint8_t switch_event;

  // Service the encoder and switch.
  check_encoder();
  switch_event = switch_released();

  // Stop any expired beep.
  check_beeper();

  switch (mode) {
  case MODE_FREE_RUN:
    // Free run mode.

    // If the switch was pressed, bring up the main menu,
    if (switch_event) {

      // Remember the mode we go back to on exit.
      previous_mode = MODE_FREE_RUN;
      previous_encoder_count = encoder_count;

      // Put up the main menu.
      init_menu(main_menu);
      mode = MODE_MAIN_MENU;
    }

    // Otherwise, service free-run mode and update display.
    else {
      run_free_run();
    }
    break;

  case MODE_MAIN_MENU:
    // Main menu mode.

    // If the switch was pressed, take action based on the selected menu item.
    if (switch_event) {
      switch (selected_menu_item) {
      case MAIN_EXIT:
        // Go back to the previous mode.
        mode = previous_mode;
        encoder_count = previous_encoder_count;
        break;

      case MAIN_RESET:
        // Reset cumulative and go back to the previous mode.
        reset_counters();
        mode = previous_mode;
        encoder_count = previous_encoder_count;
        break;

      case MAIN_FREE_RUN:
        // Go into "free run" mode.
        init_free_run();
        mode = MODE_FREE_RUN;
        break;

      case MAIN_FULL_POWER:			//Added by Gough; turns pump full on
        //turn pump full on, set pwm=255
        mode = MODE_FREE_RUN;

        encoder_count = 255;
        break;
        //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


      case MAIN_HYDRO:
        // Bring up the hydrograph menu.
        init_menu(hydrograph_menu);
        mode = MODE_HYDROGRAPH_MENU;
        break;

      case MAIN_CALIBRATE:
        //user is setting zero flow condition, set pwmCal = pwm_
        pwmCal = current_motor_pwm;

        lcd.clear();
        lcd.print ("zero set at");
        lcd.setCursor(0, 1);
        lcd.print ("pwm = ");
        lcd.print (pwmCal);    
        delay (3000);

        mode = MODE_FREE_RUN;                      ///sg copied from reset statement above
        encoder_count = previous_encoder_count;

        init_flow_table();
        break;
      }
    }

    // Otherwise, process the menu.
    else {
      run_menu();
    }
    break;

  case MODE_HYDROGRAPH_MENU:
    // Hydrograph menu mode.

    // If the switch was pressed, take action based on the selected menu item.
    if (switch_event) {
      switch (selected_menu_item) {
      case HYDRO_EXIT:
        // Go back to the main menu.
        init_menu(main_menu);
        mode = MODE_MAIN_MENU;
        break;

      default:
        // Start a new hydrograph.
        init_hydrograph(&hydrographs[selected_menu_item - HYDRO_NEW]);
        mode = MODE_HYDROGRAPH;
        break;
      }
    }

    // Otherwise, process the menu.
    else {
      run_menu();
    }
    break;

  case MODE_HYDROGRAPH:
    // Running a hydrograph.

    // If the switch was pressed, bring up the main menu,
    if (switch_event) {

      // Remember the mode we go back to on exit.
      previous_mode = MODE_FREE_RUN;

      // Put up the main menu.
      init_menu(main_menu);
      mode = MODE_MAIN_MENU;
    }

    // Otherwise, service the hydrograph and update display.
    else {
      run_hydrograph();
    }
    break;

  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Populate the lookup table for flow rate based on motor PWM setting.

void init_flow_table()
{

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC
  //Serial.println ("running init_flow_table ");
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++DIAGNOSTIC

  hydro_curve_shift = 95 - pwmCal;   //Gough 1_7; shifts curve from defalt of 95 fto lower value (e.g. 60) at start for lower
  // head in Em3 model.  NOTE  BUG -- what if pwmCal is > default of 95?  Then negative
  //hydro_curve_shift; since it's a uint8_t; random numbers.

  static uint8_t Qvals[] = {
    0,3,4,7,10,13,15,18,21,23,26,29,31,33,36,38,40,43,45,47,49,51,53,55,57,59,61,63,64,66,68,69,71,72,74,75,76,78,79,80,81,82,83,84,85,86,87,88,89,89,90,91,91,92,92,93,93,93,94,94,94,94,95,95,95,95,96,96,96,97,98,98,99,100,100,101,100,101,102,102,103,104,105,105,106,107,107,108,109,109,110,111,112,112,113,114,114,115,116,116,117,118,119,119,120,121,121,122,123,123,124,125,126,126,127,128,128,129,130,130,131,132,133,133,134,135,135,136,137,137,138,139,140,140,141,142,142,143,144,144,145,146,147,147,148,149,149,150,151,151,152,153,154,154,155,156,156,157,158,158,159,160,161,161,162,163,163,164,165,165,166,167,168,168,169,170,170,171,172,172,173,174,175,175,176,177,177,178,179,182,184,188,190,190,190,190,190,190,190,190,190,190,190,190,190,190,190,190  }  
  ;   // fill with calibration curve numbers, 70 through 256 
  //1_7 added array numbers at top for curve shifting
  //Nov 7; took number out of [].			

  for (int x = 0;  x < 256; x ++)
  {
    ml_per_sec_by_pwm[x] = 0;			////fill array with zeros so everything below pwmCal value is zero when all it done
  }

  // Qvals array simply begins with the first flow value beyond zero; so it's indexed to
  // mlps by filling that array beginning at pwmCal, which is where flow just begins as set by
  // user


  int QvalIndex = 0;						///used to increment array in following for statement
  for (int i = (pwmCal); i < 256; i++)    //pwm curve is based on zero flow = pwm of 70, curve is adjusted from there
  {

    if ( i == pwmCal)    //set QvalIndex to zero only for first iteration
    {
      QvalIndex = 0;    
    }
    QvalIndex = QvalIndex++;

    ml_per_sec_by_pwm[i]= Qvals [(QvalIndex)];  // begins filling mlps array with Qval starting at pwmCal (zero) value
    // stops at 256
    //Note on EM3; since stops at 256; should never overfill array, and Qvals could have as many values
    //as we like at the top end in case pwmCal is very low, e.g. = 50.


  }



}


