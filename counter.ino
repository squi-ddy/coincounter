#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Waveshare_ILI9486.h>
#include <Fonts/FreeSans9pt7b.h>
#include <EEPROM.h>

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define LBLUE   0xD7FF

#define SLEN        105
#define KEYPAD_SIZE 16
#define AM_SIZE     19
#define NZ_SIZE     19

#define PRESSURE 50
#define SEND_LOT_NO 30 //EEPROM location to tell us to resend parking lot data. ON -> no, OFF -> yes

Waveshare_ILI9486 Waveshield;

bool sound = true;
byte volume = 100;
bool light = true;
bool lblink = true;
String parking_no = "";
bool alarm_on = false;
int screen = 0; //0 -> main, 1 -> keypad, 2 -> calibration, 3 -> loading, 4 -> confirmation, 5 -> A - N, 6 -> N - Z
bool currently_pressed = false; //buttons only work if this is false
String current_number = "";

int threshold[4] = {100, 0, 0, 0};
int sensors[4] = {A0, A1, A2, A3};
int led_pins[4] = {42, 43, 44, 45};
int coin_counts[4] = {0, 0, 0, 0};
int values[4] = {10, 20, 50, 100};
int button_pins[4] = {46, 47, 48, 49};
int money = 0;

//number - x1 - y1 - x2 - y2
//x -> 0 - 320
//y -> 200 - 480
//-1 -> bcsp, -2 is a back button, -3 is enter, -4 is remove button (revert to not set), -5 -> A - N, -6 -> O - Z
//Order is for aesthetic purposes (drawing is not instant)
int keypad_hitbox[KEYPAD_SIZE][5] = {
	{ -2, 0, 0, 140, 50},
	{ -4, 190, 0, 320, 50},
	{ -5, 0, 150, 80, 200},
	{ -6, 240, 150, 320, 200},
	{1, 0, 200, 106, 270},
	{2, 106, 200, 213, 270},
	{3, 213, 200, 320, 270},
	{6, 213, 270, 320, 340},
	{5, 106, 270, 213, 340},
	{4, 0, 270, 106, 340},
	{7, 0, 340, 106, 410},
	{8, 106, 340, 213, 410},
	{9, 213, 340, 320, 410},
	{ -1, 213, 410, 320, 480},
	{0, 106, 410, 213, 480},
	{ -3, 0, 410, 106, 480}
};

int withdraw_hb[4] = {15, 115, 305, 165}; //hitbox for the withdraw button
int snd_btn_hb[4] = {170, 242, 235, 270}; //sound button hitbox
int lgt_btn_hb[4] = {170, 337, 235, 365}; //light button hitbox
int blk_btn_hb[4] = {170, 377, 235, 405}; //blink button hitbox
int vlm_btn_hb[5] = {86, 303, SLEN + 124, 331, 105}; //volume button hitbox, along with start pos. quite a lot longer than the act bar, to allow 0% and 100%
int reset_btn_hb[4] = {120, 420, 200, 445}; //reset button hitbox

int ok_btn_hb[4] = {120, 430, 200, 470}; //Used in the calibration screen's OK button

int yes_btn_hb[4] = {50, 250, 150, 300}; //Used in confirmation screen
int no_btn_hb[4] = {170, 250, 270, 300}; //Used in confirmation screen

void drawOnOff(int x, int y, bool onOff) {
	//draw base slider
	//-1, as ideally this should use same y as text
	if (onOff) {
	Waveshield.fillRoundRect(x, y - 1, 35, 14, 7, GREEN);
	Waveshield.drawRoundRect(x, y - 1, 35, 14, 7, BLACK);
	} else {
	Waveshield.fillRoundRect(x, y - 1, 35, 14, 7, RED);
	Waveshield.drawRoundRect(x, y - 1, 35, 14, 7, BLACK);
	}
	//draw dot
	if (!onOff) {
	Waveshield.fillCircle(x + 7, y + 6, 11, WHITE);
	Waveshield.drawCircle(x + 7, y + 6, 11, BLACK);
	} else {
	Waveshield.fillCircle(x + 28, y + 6, 11, WHITE);
	Waveshield.drawCircle(x + 28, y + 6, 11, BLACK);
	}
}

void volSlider(int x, int y, int vol) {
	Waveshield.drawLine(x, y, x + SLEN, y, BLACK);
	Waveshield.fillCircle(x + (SLEN * vol) / 100, y, 9, WHITE);
	Waveshield.drawCircle(x + (SLEN * vol) / 100, y, 9, BLACK);
}

void drawBKSP(int x, int y) {
	//outline
	Waveshield.drawLine(x + 20, y + 35, x + 40, y + 15, BLACK);
	Waveshield.drawLine(x + 40, y + 15, x + 85, y + 15, BLACK);
	Waveshield.drawLine(x + 85, y + 15, x + 85, y + 55, BLACK);
	Waveshield.drawLine(x + 85, y + 55, x + 40, y + 55, BLACK);
	Waveshield.drawLine(x + 40, y + 55, x + 20, y + 35, BLACK);

	//X
	Waveshield.drawLine(x + 75, y + 25, x + 50, y + 45, BLACK);
	Waveshield.drawLine(x + 50, y + 25, x + 75, y + 45, BLACK);
}

void drawBKbutton(int x, int y) {
	//<
	Waveshield.drawLine(x + 15, y + 25, x + 35, y + 10, BLACK);
	Waveshield.drawLine(x + 15, y + 25, x + 35, y + 40, BLACK);

	//back
	Waveshield.setCursor(x + 50, y + 15);
	Waveshield.setTextSize(3);
	Waveshield.print(F("Back"));
}

String correct_format(String no, int len) {
	//this function inserts spaces before a number
	String whitespace = "";
	for (int i = 0; i < len - no.length(); i++) {
	whitespace += ' ';
	}
	return whitespace + no;
}

void renderLoading() {
	renderMessage(F("Working..."));
}

void renderMessage(String message) {
	//Set screen
	screen = 3;

	//Set colour
	Waveshield.fillScreen(LBLUE);
	Waveshield.setTextColor(BLACK);

	//Draw loading
	Waveshield.setTextSize(4);
	printCentredText(message, 200, 320);
}

void renderFailed() {
	//Set screen
	screen = 3;

	//Set colour
	Waveshield.fillScreen(LBLUE);
	Waveshield.setTextColor(BLACK);

	//Draw loading
	Waveshield.setTextSize(2);
	Waveshield.setCursor(45, 180);
	Waveshield.print(F("Failed transmission."));

	//Draw error msg
	Waveshield.setTextSize(1);
	Waveshield.setCursor(70, 210);
	Waveshield.print(F("The receiver may be too far away"));

	Waveshield.setTextSize(1);
	Waveshield.setCursor(70, 220);
	Waveshield.print(F("       or it may be busy.       "));
}

void renderNumber() {
	//Cover current number
	Waveshield.fillRect(113, 160, 100, 27, YELLOW);

	//draw number
	Waveshield.setCursor(115, 165);
	Waveshield.setTextSize(3);
	Waveshield.print(correct_format(current_number, 5) + "c");
}

void renderKeypad() {
	//base render function for general keypad layout
	//set colour
	Waveshield.fillScreen(LBLUE);
	Waveshield.setTextColor(BLACK);

	//draw prompt box
	Waveshield.setTextSize(3);
	printCentredText(F("Enter Withdrawal"), 90, 320);
	Waveshield.setCursor(65, 120);

	//draw box for number on top
	Waveshield.fillRect(0, 150, 320, 50, YELLOW);
	Waveshield.drawRect(0, 150, 320, 50, BLACK);

	//Render number -> adds number & updates it
	renderNumber();

	//draw 5 rects below no.
	Waveshield.fillRect(115, 190, 16, 2, BLACK);
	Waveshield.fillRect(133, 190, 16, 2, BLACK);
	Waveshield.fillRect(151, 190, 16, 2, BLACK);
	Waveshield.fillRect(169, 190, 16, 2, BLACK);
	Waveshield.fillRect(187, 190, 16, 2, BLACK);

	//render the 0-9 keypad
	render09();
}

void render09() {
	//set screen
	screen = 1;

	Waveshield.fillRect(0, 200, 320, 280, YELLOW);
	//based on the hitboxes in keypad_hitbox.
	for (byte i = 0; i < KEYPAD_SIZE; i++) {
	if (keypad_hitbox[i][0] >= 0) {
		Waveshield.fillRect(keypad_hitbox[i][1], keypad_hitbox[i][2], keypad_hitbox[i][3] - keypad_hitbox[i][1], keypad_hitbox[i][4] - keypad_hitbox[i][2], YELLOW);
		Waveshield.drawRect(keypad_hitbox[i][1], keypad_hitbox[i][2], keypad_hitbox[i][3] - keypad_hitbox[i][1], keypad_hitbox[i][4] - keypad_hitbox[i][2], BLACK);
		Waveshield.setCursor(keypad_hitbox[i][1] + 42, keypad_hitbox[i][2] + 20);
		Waveshield.setTextSize(5);
		Waveshield.print(String(keypad_hitbox[i][0]));
	} else if (keypad_hitbox[i][0] == -1) {
		//backspace button
		Waveshield.fillRect(keypad_hitbox[i][1], keypad_hitbox[i][2], keypad_hitbox[i][3] - keypad_hitbox[i][1], keypad_hitbox[i][4] - keypad_hitbox[i][2], YELLOW);
		Waveshield.drawRect(keypad_hitbox[i][1], keypad_hitbox[i][2], keypad_hitbox[i][3] - keypad_hitbox[i][1], keypad_hitbox[i][4] - keypad_hitbox[i][2], BLACK);
		drawBKSP(keypad_hitbox[i][1], keypad_hitbox[i][2]);
	} else if (keypad_hitbox[i][0] == -2) {
		//back button
		drawBKbutton(keypad_hitbox[i][1], keypad_hitbox[i][2]);
	} else if (keypad_hitbox[i][0] == -3) {
		//enter text
		Waveshield.fillRect(keypad_hitbox[i][1], keypad_hitbox[i][2], keypad_hitbox[i][3] - keypad_hitbox[i][1], keypad_hitbox[i][4] - keypad_hitbox[i][2], YELLOW);
		Waveshield.drawRect(keypad_hitbox[i][1], keypad_hitbox[i][2], keypad_hitbox[i][3] - keypad_hitbox[i][1], keypad_hitbox[i][4] - keypad_hitbox[i][2], BLACK);
		Waveshield.setCursor(keypad_hitbox[i][1] + 10, keypad_hitbox[i][2] + 25);
		Waveshield.setTextSize(3);
		Waveshield.print(F("Enter"));
	} else if (keypad_hitbox[i][0] == -4) {
		//delete button
		Waveshield.setCursor(keypad_hitbox[i][1] + 15, keypad_hitbox[i][2] + 15);
		Waveshield.setTextSize(3);
		Waveshield.print(F("Delete"));
	}
	}
}

void renderSettings() {
	//Draw box around settings
	Waveshield.fillRoundRect(15, 185, 290, 270, 5, WHITE);
	Waveshield.drawRoundRect(15, 185, 290, 270, 5, BLACK);

	//Settings
	Waveshield.setCursor(87, 205);
	Waveshield.setTextSize(3);
	Waveshield.print(F("Settings"));

	//Settings -> Sound
	Waveshield.setCursor(100, 250);
	Waveshield.setTextSize(2);
	Waveshield.print(F("Sound"));
	renderSound();

	//Settings -> Volume
	Waveshield.setCursor(100, 287);
	Waveshield.setTextSize(2);
	Waveshield.print(F("Volume"));
	Waveshield.setCursor(185, 287);
	Waveshield.print(String(volume) + '%');
	renderVolume();

	//Settings -> Light
	Waveshield.setCursor(100, 345);
	Waveshield.setTextSize(2);
	Waveshield.print(F("Light"));
	renderLight();

	//Settings -> Blink
	Waveshield.setCursor(100, 385);
	Waveshield.setTextSize(2);
	Waveshield.print(F("Blink"));
	renderBlink();

	//Settings -> Reset
	Waveshield.fillRoundRect(reset_btn_hb[0], reset_btn_hb[1], reset_btn_hb[2] - reset_btn_hb[0], reset_btn_hb[3] - reset_btn_hb[1], 4, RED);
	Waveshield.drawRoundRect(reset_btn_hb[0], reset_btn_hb[1], reset_btn_hb[2] - reset_btn_hb[0], reset_btn_hb[3] - reset_btn_hb[1], 4, BLACK);
	Waveshield.setCursor(reset_btn_hb[0] + 12, reset_btn_hb[1] + 5);
	Waveshield.setTextSize(2);
	Waveshield.setTextColor(WHITE);
	Waveshield.print(F("Reset"));
	Waveshield.setTextColor(BLACK); //set text colour back to black
}

void renderSound() {
	//render over previous button
	Waveshield.fillRect(snd_btn_hb[0], snd_btn_hb[1], snd_btn_hb[2] - snd_btn_hb[0], snd_btn_hb[3] - snd_btn_hb[1], WHITE);

	//draw new
	drawOnOff(185, 250, sound);
}

void renderVolume() {
	//render over previous slider
	Waveshield.fillRect(vlm_btn_hb[0], vlm_btn_hb[1], vlm_btn_hb[2] - vlm_btn_hb[0], vlm_btn_hb[3] - vlm_btn_hb[1], WHITE);

	//draw text
	Waveshield.fillRect(185, 287, 70, 50, WHITE);
	Waveshield.setCursor(185, 287);
	Waveshield.print(String(volume) + '%');

	//draw new
	volSlider(105, 317, volume);
}

void renderLight() {
	//render over previous button
	Waveshield.fillRect(lgt_btn_hb[0], lgt_btn_hb[1], lgt_btn_hb[2] - lgt_btn_hb[0], lgt_btn_hb[3] - lgt_btn_hb[1], WHITE);

	//draw new
	drawOnOff(185, 345, light);
}

void renderBlink() {
	//render over previous button
	Waveshield.fillRect(blk_btn_hb[0], blk_btn_hb[1], blk_btn_hb[2] - blk_btn_hb[0], blk_btn_hb[3] - blk_btn_hb[1], WHITE);

	//draw new
	drawOnOff(185, 385, lblink);
}

void printCentredText(String text, int y, int mw) {
	printCentredText(text, y, mw, 0, false);
}

void printCentredText(String text, int y, int mw, int sx, bool centreAnchor) {
	int x1, y1; //this is always 0, 0
	unsigned int w, h;
	Waveshield.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

	//Set cursor to be centered
	Waveshield.setCursor((mw - w) / 2 + sx, y - h / 2 * centreAnchor);
	Waveshield.print(text);
}

void render() {
	//set screen
	screen = 0;

	//set colour
	Waveshield.fillScreen(LBLUE);
	Waveshield.setTextColor(BLACK);

	//Draw top text
	Waveshield.setTextSize(4);
	Waveshield.setCursor(30, 30);
	printCentredText(F("Coin Counter"), 30, 320);

	//Draw car parking location
	Waveshield.setTextSize(2);
	String printed = "You have ";
	printed = printed + money + "c.";
	printCentredText(printed, 80, 320);

	//Draw button -> Enter/Change parking lot no.
	Waveshield.fillRoundRect(withdraw_hb[0], withdraw_hb[1], withdraw_hb[2] - withdraw_hb[0], withdraw_hb[3] - withdraw_hb[1], 4, YELLOW);
	Waveshield.drawRoundRect(withdraw_hb[0], withdraw_hb[1], withdraw_hb[2] - withdraw_hb[0], withdraw_hb[3] - withdraw_hb[1], 4, BLACK);
	Waveshield.setTextSize(2);
	printCentredText(F("Withdraw"), (withdraw_hb[1] + withdraw_hb[3]) / 2, withdraw_hb[2], withdraw_hb[0], true);

	//render settings dialog
	renderSettings();
}

void renderConfirmation() {
	screen = 4;

	//we do not clear the screen, for effect
	//draw prompt box
	Waveshield.fillRoundRect(30, 170, 260, 140, 5, WHITE);
	Waveshield.drawRoundRect(30, 170, 260, 140, 5, BLACK);

	//draw text -> Are you sure?
	Waveshield.setTextSize(3);
	printCentredText(F("Are You Sure?"), 190, 260, 30, false);

	//warning abt recalibration
	Waveshield.setTextSize(1);
	printCentredText(F("Money will be reset."), 230, 260, 30, false);

	//Yes button
	Waveshield.fillRoundRect(yes_btn_hb[0], yes_btn_hb[1], yes_btn_hb[2] - yes_btn_hb[0], yes_btn_hb[3] - yes_btn_hb[1], 4, RED);
	Waveshield.drawRoundRect(yes_btn_hb[0], yes_btn_hb[1], yes_btn_hb[2] - yes_btn_hb[0], yes_btn_hb[3] - yes_btn_hb[1], 4, BLACK);
	Waveshield.setTextSize(3);
	Waveshield.setTextColor(WHITE);
	printCentredText(F("Yes"), (yes_btn_hb[1] + yes_btn_hb[3]) / 2, yes_btn_hb[2] - yes_btn_hb[0], yes_btn_hb[0], true);
	Waveshield.setTextColor(BLACK);

	//No button
	Waveshield.fillRoundRect(no_btn_hb[0], no_btn_hb[1], no_btn_hb[2] - no_btn_hb[0], no_btn_hb[3] - no_btn_hb[1], 4, GREEN);
	Waveshield.drawRoundRect(no_btn_hb[0], no_btn_hb[1], no_btn_hb[2] - no_btn_hb[0], no_btn_hb[3] - no_btn_hb[1], 4, BLACK);
	Waveshield.setCursor(no_btn_hb[0] + 33, no_btn_hb[1] + 15);
	Waveshield.setTextSize(3);
	printCentredText(F("No"), (no_btn_hb[1] + no_btn_hb[3]) / 2, no_btn_hb[2] - no_btn_hb[0], no_btn_hb[0], true);
}

void doHitboxes(int scrn) {
	//get touchscreen point
	TSPoint p = Waveshield.getPoint();
	Waveshield.normalizeTsPoint(p);

	//check if press ok
	if (p.x >= 0 && p.y >= 0 && Waveshield.pressure() > PRESSURE) {
	if (scrn == 0) {
		//sound btn
		if (p.x > snd_btn_hb[0] && p.x < snd_btn_hb[2] && p.y > snd_btn_hb[1] && p.y < snd_btn_hb[3]) {
		sound = !sound;
		//setVariables();
		renderSound();
		delay(200);
		//light btn
		} else if (p.x > lgt_btn_hb[0] && p.x < lgt_btn_hb[2] && p.y > lgt_btn_hb[1] && p.y < lgt_btn_hb[3]) {
		light = !light;
		//setVariables();
		//Note: the render function creates a natural delay.
		renderLight();
		delay(200);
		//Blink button
		} else if (p.x > blk_btn_hb[0] && p.x < blk_btn_hb[2] && p.y > blk_btn_hb[1] && p.y < blk_btn_hb[3]) {
		lblink = !lblink;
		//setVariables();
		//Note: the render function creates a natural delay.
		renderBlink();
		delay(200);
		//Volume button
		} else if (p.x > vlm_btn_hb[0] && p.x < vlm_btn_hb[2] && p.y > vlm_btn_hb[1] && p.y < vlm_btn_hb[3]) {
		int new_volume = max(0, min((p.x - vlm_btn_hb[4]) * 100 / SLEN, 100));
		if (new_volume != volume) {
			volume = new_volume;
			//setVariables();
			renderVolume();
		}
		} else if (p.x > withdraw_hb[0] && p.x < withdraw_hb[2] && p.y > withdraw_hb[1] && p.y < withdraw_hb[3]) {
		current_number = "";
		renderKeypad();
		} else if (p.x > reset_btn_hb[0] && p.x < reset_btn_hb[2] && p.y > reset_btn_hb[1] && p.y < reset_btn_hb[3]) {
		renderConfirmation();
		}
	} else if (scrn == 1) {
		//do via keypad_hitbox
		for (byte i = 0; i < KEYPAD_SIZE; i++) {
			if (p.x > keypad_hitbox[i][1] && p.x < keypad_hitbox[i][3] && p.y > keypad_hitbox[i][2] && p.y < keypad_hitbox[i][4]) {
				if (keypad_hitbox[i][0] >= 0) {
				//no. buttons
				current_number = current_number + String(keypad_hitbox[i][0]);

				//remove if length > 5
				if (current_number.length() > 5) {
					current_number = current_number.substring(1, current_number.length());
				}

				renderNumber();
				delay(200);
				} else if (keypad_hitbox[i][0] == -2) {
				//back button
				current_number = "";
				render();
				} else if (keypad_hitbox[i][0] == -1) {
				//bksp button
				current_number = current_number.substring(0, current_number.length() - 1);
				renderNumber();
				delay(200);
				} else if (keypad_hitbox[i][0] == -3) {
				//enter button
				parking_no = current_number;
				renderLoading();
				//send_data_parking();
				current_number = "";
				render();
				} else if (keypad_hitbox[i][0] == -4) {
				//delete data button
				parking_no = "";
				renderLoading();
				//send_data_parking();
				current_number = "";
				render();
				}
			}
		}
	} else if (scrn == 2) {
		if (p.x > ok_btn_hb[0] && p.x < ok_btn_hb[2] && p.y > ok_btn_hb[1] && p.y < ok_btn_hb[3]) {
		//ok button on the calibration screen
		//writeCalibrationData();
		render();
		} else {
		Waveshield.fillCircle(p.x, p.y, 2, RED);
		}
	} else if (scrn == 4) {
		if (p.x > yes_btn_hb[0] && p.x < yes_btn_hb[2] && p.y > yes_btn_hb[1] && p.y < yes_btn_hb[3]) {
		//yes button on confirmation screen
		EEPROM.update(0, 255);
		//resetEEPROM();
		setup();
		} else if (p.x > no_btn_hb[0] && p.x < no_btn_hb[2] && p.y > no_btn_hb[1] && p.y < no_btn_hb[3]) {
		render();
		}
	} 
	}
}

void setup() {
	//Initialise
	SPI.begin();
	Waveshield.begin();
	Serial.begin(9600);

	for (int pin = 0; pin < 4; pin++) {
		pinMode(led_pins[pin], OUTPUT);
		digitalWrite(led_pins[pin], LOW);
	}

	//Set portrait
	Waveshield.setRotation(0);

	//Set brightness
	Waveshield.setScreenBrightness(255);

	//turn off idle mode
	Waveshield.setIdleMode(false);

	//render loading scrn
	renderLoading();

	//grab the variables
	//parking_no = get_parking_no();
	//alarm_on = is_alarm_on();

	//reset EEPROM if required
	//resetEEPROM();

	//get other variables on EEPROM
	//getVariables();

	// Render the screen
	render();
}

void loop() {
	doHitboxes(screen);
	for (int i = 0; i < 4; i++) {
		int val = digitalRead(sensors[i]);
		if (val == 0) {
			String message = "";
			message = message + values[i];
			message = message + "c added!";
			renderMessage(message);
			money = money + values[i];
			coin_counts[i]++;
			delay(200);
			render();
		}
	}
}
