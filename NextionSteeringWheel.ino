//Uncomment this for HALFSTEP encoder operation
//#define HALF_STEP

#include <Keypad.h>
#include <Joystick.h>

struct rotariesdef
{
	byte pin1;
	byte pin2;
	int ccwchar;
	int cwchar;
	volatile unsigned char state;
};

rotariesdef rotaries[] = {
	{0, 1, 26, 27, 0},
	{2, 3, 28, 29, 0},
	{4, 5, 30, 31, 0}};
byte rows[] = {2, 3, 4, 5};
byte cols[] = {8, 9, 10, 11};

const int rowCount = sizeof(rows) / sizeof(rows[0]);
const int colCount = sizeof(cols) / sizeof(cols[0]);
const int rotariesCount = sizeof(rotaries) / sizeof(rotaries[0]);

byte buttons[colCount][rowCount] = {
	{1, 2, 3, 4},
	{5, 6, 7, 8},
	{9, 10, 11, 12},
	{13, 14, 15, 16},
};

Keypad buttbx = Keypad(makeKeymap(buttons), rows, cols, rowCount, colCount);

Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID,
				   JOYSTICK_TYPE_JOYSTICK, 24, 0,
				   false, false, false, false, false, false,
				   false, false, false, false, false);

void CheckAllButtons()
{
	if (buttbx.getKeys())
	{
		for (int i = 0; i < LIST_MAX; i++) // Scan the whole key list.
		{
			if (buttbx.key[i].stateChanged) // Only find keys that have changed state.
			{
				switch (buttbx.key[i].kstate)
				{ // Report active key state : IDLE, PRESSED, HOLD, or RELEASED
				case PRESSED:
				case HOLD:
					Joystick.setButton(buttbx.key[i].kchar, 1);
					break;
				case RELEASED:
				case IDLE:
					Joystick.setButton(buttbx.key[i].kchar, 0);
					break;
				}
			}
		}
	}
}

#define DIR_CCW 0x10
#define DIR_CW 0x20
#define R_START 0x0

#ifdef HALF_STEP
// Use the half-step state table (emits a code at 00 and 11)
#define R_CCW_BEGIN 0x1
#define R_CW_BEGIN 0x2
#define R_START_M 0x3
#define R_CW_BEGIN_M 0x4
#define R_CCW_BEGIN_M 0x5
const unsigned char ttable[6][4] = {
	// R_START (00)
	{R_START_M, R_CW_BEGIN, R_CCW_BEGIN, R_START},
	// R_CCW_BEGIN
	{R_START_M | DIR_CCW, R_START, R_CCW_BEGIN, R_START},
	// R_CW_BEGIN
	{R_START_M | DIR_CW, R_CW_BEGIN, R_START, R_START},
	// R_START_M (11)
	{R_START_M, R_CCW_BEGIN_M, R_CW_BEGIN_M, R_START},
	// R_CW_BEGIN_M
	{R_START_M, R_START_M, R_CW_BEGIN_M, R_START | DIR_CW},
	// R_CCW_BEGIN_M
	{R_START_M, R_CCW_BEGIN_M, R_START_M, R_START | DIR_CCW},
};
#else
// Use the full-step state table (emits a code at 00 only)
#define R_CW_FINAL 0x1
#define R_CW_BEGIN 0x2
#define R_CW_NEXT 0x3
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT 0x6

const unsigned char ttable[7][4] = {
	// R_START
	{R_START, R_CW_BEGIN, R_CCW_BEGIN, R_START},
	// R_CW_FINAL
	{R_CW_NEXT, R_START, R_CW_FINAL, R_START | DIR_CW},
	// R_CW_BEGIN
	{R_CW_NEXT, R_CW_BEGIN, R_START, R_START},
	// R_CW_NEXT
	{R_CW_NEXT, R_CW_BEGIN, R_CW_FINAL, R_START},
	// R_CCW_BEGIN
	{R_CCW_NEXT, R_START, R_CCW_BEGIN, R_START},
	// R_CCW_FINAL
	{R_CCW_NEXT, R_CCW_FINAL, R_START, R_START | DIR_CCW},
	// R_CCW_NEXT
	{R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};
#endif

void SetupRotary()
{
	for (int i = 0; i < rotariesCount; i++)
	{
		pinMode(rotaries[i].pin1, INPUT);
		pinMode(rotaries[i].pin2, INPUT);
#ifdef ENABLE_PULLUPS
		digitalWrite(rotaries[i].pin1, HIGH);
		digitalWrite(rotaries[i].pin2, HIGH);
#endif
	}
}

void CheckAllEncoders()
{
	for (int i = 0; i < rotariesCount; i++)
	{
		unsigned char result = rotary_process(i);
		if (result == DIR_CCW)
		{
			Joystick.setButton(rotaries[i].ccwchar, 1);
			delay(50);
			Joystick.setButton(rotaries[i].ccwchar, 0);
		};
		if (result == DIR_CW)
		{
			Joystick.setButton(rotaries[i].cwchar, 1);
			delay(50);
			Joystick.setButton(rotaries[i].cwchar, 0);
		};
	}
}

unsigned char rotary_process(int _i)
{
	unsigned char pinstate = (digitalRead(rotaries[_i].pin2) << 1) | digitalRead(rotaries[_i].pin1);
	rotaries[_i].state = ttable[rotaries[_i].state & 0xf][pinstate];
	return (rotaries[_i].state & 0x30);
}

// NEXTION DISPLAY
int readSize = 0;
int endofOutcomingMessageCount = 0;
int messageend = 0;
String command = "";

static long baud = 9600;
static long newBaud = baud;

void lineCodingEvent(long baud, byte databits, byte parity, byte charFormat)
{
	newBaud = baud;
}

void WriteToComputer()
{
	while (Serial1.available())
	{
		char c = (char)Serial1.read();
		Serial.write(c);
	}
}

void UpdateBaudRate()
{
	// Update baudrate if required
	newBaud = Serial.baud();
	if (newBaud != baud)
	{
		baud = newBaud;
		Serial1.end();
		Serial1.begin(baud);
	}
}

// NEXTION DISPLAY

void setup()
{

	Serial.begin(baud);
	Serial1.begin(baud);

	SetupRotary();
	Joystick.begin(false);
	while (!Serial)
	{
		CheckAllEncoders();
		// Refresh gamepad even if the serial port not open
		CheckAllButtons();
	}
}

/// MAIN LOOP
void loop()
{
	// Gamepad reading

	CheckAllEncoders();
	CheckAllButtons();

	UpdateBaudRate();

	while (Serial.available())
	{
		WriteToComputer();
		char c = (char)Serial.read();
		Serial1.write(c);
	}

	WriteToComputer();
}
