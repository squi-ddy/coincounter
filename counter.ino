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
#define MAGENTA 0xFA3F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define LBLUE   0xD7FF

#define WIDTH       320
#define HEIGHT      480
#define KEYPAD_SIZE 15
#define CYCLE_LEN   3000

#define PRESSURE 50

Waveshare_ILI9486 Waveshield;

int screen = 0; //0 -> main, 1 -> keypad, 2 -> calibration, 3 -> loading, 4 -> confirmation, 5 -> A - N, 6 -> N - Z
int current_target = 0;
bool teaching = false;
String current_number = "";

int sensors[4] = {A0, A1, A2, A3};
int led_pins[4] = {42, 43, 44, 45};
int coin_counts[4] = {0, 0, 0, 0};
int values[4] = {100, 50, 20, 10};
int button_pins[4] = {47, 49, 46, 48};
unsigned int colors[4] = {YELLOW, BLUE, GREEN, RED};
unsigned int text_colors[4] = {BLACK, WHITE, BLACK, WHITE};
int to_blink[4] = {-1, -1, -1, -1};
bool button_status[4] = {HIGH, HIGH, HIGH, HIGH};
int money = 0;

//number - x1 - y1 - x2 - y2
//x -> 0 - WIDTH
//y -> 200 - 480
//-1 -> bcsp, -2 is a back button, -3 is enter, -4 is remove button (revert to not set), -5 -> A - N, -6 -> O - Z
//Order is for aesthetic purposes (drawing is not instant)
int keypad_hitbox[KEYPAD_SIZE][5] = {
	{ -2, 0, 0, 140, 50},
	{ -5, 0, 150, 80, 200},
	{ -6, 240, 150, WIDTH, 200},
	{1, 0, 200, 106, 270},
	{2, 106, 200, 213, 270},
	{3, 213, 200, WIDTH, 270},
	{6, 213, 270, WIDTH, 340},
	{5, 106, 270, 213, 340},
	{4, 0, 270, 106, 340},
	{7, 0, 340, 106, 410},
	{8, 106, 340, 213, 410},
	{9, 213, 340, WIDTH, 410},
	{ -1, 213, 410, WIDTH, 480},
	{0, 106, 410, 213, 480},
	{ -3, 0, 410, 106, 480}
};

int withdraw_hb[4] = {15, 115, 305, 165}; //hitbox for the withdraw button
int stocks_hb[4] = {15, 175, 305, 225}; //hitbox for the stocks button
int teach_btn_hb[4] = {170, 377, 235, 405}; //teaching button hitbox
int reset_btn_hb[4] = {120, 420, 200, 445}; //reset button hitbox
int bk_btn_hb[4] = {0, 0, 140, 50}; // back button hitbox

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
	renderMessage(message, 4);
}

void renderMessage(String message, int size) {
	String arr[1] = {message};
	renderMessage(arr, 1, size);
}

void renderMessage(String message[], int length, int size) {
	//Set screen
	screen = 3;

	//Set colour
	Waveshield.fillScreen(LBLUE);
	Waveshield.setTextColor(BLACK);

	//Draw message
	Waveshield.setTextSize(size);
	printCentredText(message, length, 200, WIDTH, 0, false);

	// small delay
	delay(200);
}

void renderNumber() {
	//Cover current number
	Waveshield.fillRect(113, 160, 100, 27, YELLOW);

	//draw number
	Waveshield.setTextSize(3);
	printCentredText(correct_format(current_number, 5) + "c", 165, WIDTH);
}

void renderKeypad() {
	//base render function for general keypad layout
	//set colour
	Waveshield.fillScreen(LBLUE);
	Waveshield.setTextColor(BLACK);

	//draw prompt box
	Waveshield.setTextSize(3);
	printCentredText(F("Enter Withdrawal"), 90, WIDTH);
	Waveshield.setCursor(65, 120);

	//draw box for number on top
	Waveshield.fillRect(0, 150, WIDTH, 50, YELLOW);
	Waveshield.drawRect(0, 150, WIDTH, 50, BLACK);

	//Render number -> adds number & updates it
	renderNumber();

	// draw underscores
	printCentredText("_____ ", 170, WIDTH);

	//render the 0-9 keypad
	render09();
}

void render09() {
	//set screen
	screen = 1;

	Waveshield.fillRect(0, 200, WIDTH, 280, YELLOW);
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
	Waveshield.fillRoundRect(15, 320, 290, 135, 5, WHITE);
	Waveshield.drawRoundRect(15, 320, 290, 135, 5, BLACK);

	//Settings
	Waveshield.setTextSize(3);
	printCentredText(F("Settings"), 340, WIDTH);

	//Settings -> Teaching
	Waveshield.setCursor(100, 385);
	Waveshield.setTextSize(2);
	Waveshield.print(F("Teach"));
	renderTeaching();

	//Settings -> Reset
	Waveshield.fillRoundRect(reset_btn_hb[0], reset_btn_hb[1], reset_btn_hb[2] - reset_btn_hb[0], reset_btn_hb[3] - reset_btn_hb[1], 4, RED);
	Waveshield.drawRoundRect(reset_btn_hb[0], reset_btn_hb[1], reset_btn_hb[2] - reset_btn_hb[0], reset_btn_hb[3] - reset_btn_hb[1], 4, BLACK);
	Waveshield.setCursor(reset_btn_hb[0] + 12, reset_btn_hb[1] + 5);
	Waveshield.setTextSize(2);
	Waveshield.setTextColor(WHITE);
	Waveshield.print(F("Reset"));
	Waveshield.setTextColor(BLACK); //set text colour back to black
}

void renderTeaching() {
	//render over previous button
	Waveshield.fillRect(teach_btn_hb[0], teach_btn_hb[1], teach_btn_hb[2] - teach_btn_hb[0], teach_btn_hb[3] - teach_btn_hb[1], WHITE);

	//draw new
	drawOnOff(185, 385, teaching);
}

void printCentredText(String text, int y, int mw) {
	printCentredText(text, y, mw, 0, false);
}


void printCentredText(String text, int y, int mw, int sx, bool centreAnchor) {
	String arr[1] = {text};
	printCentredText(arr, 1, y, mw, sx, centreAnchor);
}

void printCentredText(String text[], int length, int y, int mw, int sx, bool centreAnchor) {
	for (int i = 0 ; i < length; i++) {
		int x1, y1; //this is always 0, 0
		unsigned int w, h;
		Waveshield.getTextBounds(text[i], 0, 0, &x1, &y1, &w, &h);

		//Set cursor to be centered
		Waveshield.setCursor((mw - w) / 2 + sx, y - h / 2 * centreAnchor);
		Waveshield.print(text[i]);

		y = y + h;
	}
}

void renderMain() {
	//set screen
	screen = 0;
	current_target = 0;

	//set colour
	Waveshield.fillScreen(LBLUE);
	Waveshield.setTextColor(BLACK);

	//Draw top text
	Waveshield.setTextSize(4);
	Waveshield.setCursor(30, 30);
	printCentredText(F("Coin Counter"), 30, WIDTH);

	//Draw money
	Waveshield.setTextSize(2);
	String printed = "You have ";
	printed = printed + money + "c.";
	printCentredText(printed, 80, WIDTH);

	//Draw button -> withdraw
	Waveshield.fillRoundRect(withdraw_hb[0], withdraw_hb[1], withdraw_hb[2] - withdraw_hb[0], withdraw_hb[3] - withdraw_hb[1], 4, YELLOW);
	Waveshield.drawRoundRect(withdraw_hb[0], withdraw_hb[1], withdraw_hb[2] - withdraw_hb[0], withdraw_hb[3] - withdraw_hb[1], 4, BLACK);
	Waveshield.setTextSize(2);
	printCentredText(F("Withdraw"), (withdraw_hb[1] + withdraw_hb[3]) / 2, withdraw_hb[2] - withdraw_hb[0], withdraw_hb[0], true);

	//Draw button -> see counts
	Waveshield.fillRoundRect(stocks_hb[0], stocks_hb[1], stocks_hb[2] - stocks_hb[0], stocks_hb[3] - stocks_hb[1], 4, MAGENTA);
	Waveshield.drawRoundRect(stocks_hb[0], stocks_hb[1], stocks_hb[2] - stocks_hb[0], stocks_hb[3] - stocks_hb[1], 4, BLACK);
	Waveshield.setTextSize(2);
	printCentredText(F("Coin Counts"), (stocks_hb[1] + stocks_hb[3]) / 2, stocks_hb[2] - stocks_hb[0], stocks_hb[0], true);

	//render settings dialog
	renderSettings();
}

void renderMoneyCount() {
	Waveshield.fillRect(0, 80, WIDTH, 20, LBLUE);

	Waveshield.setTextSize(2);
	String printed = "You have ";
	printed = printed + money + "c.";
	printCentredText(printed, 80, WIDTH);
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
			//Blink button
		    if (p.x > teach_btn_hb[0] && p.x < teach_btn_hb[2] && p.y > teach_btn_hb[1] && p.y < teach_btn_hb[3]) {
				teaching = !teaching;
				writeEEPROM();
				renderTeaching();
				delay(200);
			} else if (p.x > stocks_hb[0] && p.x < stocks_hb[2] && p.y > stocks_hb[1] && p.y < stocks_hb[3]) {
				//setVariables();
				renderStocks();
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
						renderMain();
					} else if (keypad_hitbox[i][0] == -1) {
						//bksp button
						current_number = current_number.substring(0, current_number.length() - 1);
						renderNumber();
						delay(200);
					} else if (keypad_hitbox[i][0] == -3) {
						//enter button
						renderLoading();
						startWithdraw(money - current_number.toInt());
						current_number = "";
					}
				}
			}
		} 
		else if (scrn == 2 || scrn == 6) {
			if (p.x > bk_btn_hb[0] && p.x < bk_btn_hb[2] && p.y > bk_btn_hb[1] && p.y < bk_btn_hb[3]) {
				for (int i = 0; i < 4; i++) {
					digitalWrite(led_pins[i], LOW);
					to_blink[i] = -1;
				}
				renderMain();
			}
		}
		else if (scrn == 4) {
			if (p.x > yes_btn_hb[0] && p.x < yes_btn_hb[2] && p.y > yes_btn_hb[1] && p.y < yes_btn_hb[3]) {
				//yes button on confirmation screen
				money = 0;
				for (int i = 0; i < 4; i++) {
					coin_counts[i] = 0;
				}
				for (int pin = 0; pin < 4; pin++) {
					pinMode(led_pins[pin], OUTPUT);
					digitalWrite(led_pins[pin], LOW);
				}
				writeEEPROM();
				renderMain();
			} else if (p.x > no_btn_hb[0] && p.x < no_btn_hb[2] && p.y > no_btn_hb[1] && p.y < no_btn_hb[3]) {
				renderMain();
			}
		}
	}
}

void setup() {
	//Initialise
	SPI.begin();
	Waveshield.begin();
	Serial.begin(9600);

	// add pullup resistors 
	for (int i = 0; i < 4; i++) {
		pinMode(button_pins[i], INPUT_PULLUP);
	}

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

	// reset money and coin counts from EEPROM
	readEEPROM();

	// Render the screen
	renderMain();
}

void loop() {
	doHitboxes(screen);
	handleIR();
	handleButton();
	handleFlash();
}

void handleIR() {
	for (int i = 0; i < 4; i++) {
		int val = digitalRead(sensors[i]);
		if (val == 0) {
			money = money + values[i];
			coin_counts[i]++;
			writeEEPROM();
			if (screen == 0) {
				renderMoneyCount();
			} else {
				renderMain();
			}
			delay(200);
		}
	}
}

void handleFlash() {
	for (int i = 0; i < 4; i++) {
		if (to_blink[i] != -1) {
			to_blink[i]--;
		}
		if (to_blink[i] == CYCLE_LEN / 2) {
			digitalWrite(led_pins[i], LOW);
		}
		if (to_blink[i] == 0) {
			digitalWrite(led_pins[i], HIGH);
			to_blink[i] = CYCLE_LEN;
		}
	}
}

void handleButton() {
	for (int i = 0; i < 4; i++) {
		// INPUT_PULLUP reverses the status
		if (digitalRead(button_pins[i]) == LOW && button_status[i] == HIGH) {
			button_status[i] = LOW;
		} else if (digitalRead(button_pins[i]) == HIGH && button_status[i] == LOW) {
			button_status[i] = HIGH;
			if (coin_counts[i] > 0) {
				coin_counts[i]--;
				money = max(0, money - values[i]);
			}
			writeEEPROM();
			if (screen == 2 || screen == 5) {
				// 2 -> teach mode; 5 -> assisted mode
				withdrawStep();
			} else if (screen == 0) {
				renderMoneyCount();
			} else {
				renderMain();
			}
		}
	}
}

void readEEPROM() {
	if (EEPROM.read(0) == 255) {
		for (int i = 0; i < 8; i++) {
			EEPROM.update(i, 0);
		}
	}
	for (int i = 0; i < 4; i++) {
		coin_counts[i] = EEPROM.read(i + 1);
	}
	teaching = EEPROM.read(5) > 128;
	money = EEPROM.read(6) << 8;
	money = money + EEPROM.read(7);
}

void writeEEPROM() {
	for (int i = 0; i < 4; i++) {
		EEPROM.update(i + 1, coin_counts[i]);
	}
	EEPROM.update(5, teaching ? 255 : 0);
	EEPROM.update(6, money >> 8);
	EEPROM.update(7, money & 255);
}

void startWithdraw(int target) {
	current_target = target;
	withdrawStep();
}

void withdrawStep() {
	for (int i = 0; i < 4; i++) {
		digitalWrite(led_pins[i], LOW);
		to_blink[i] = -1;
	}
	int amt = money - current_target;
	if (amt == 0) {
		renderMessage(F("Good job!"));
		renderMain();
		return;
	}
	bool doable = calculateDoable(amt);
	if (!doable) {
		String message[2] = {F("Impossible"), F("Withdrawal!")};
		renderMessage(message, 2, 3);
		renderMain();
		return;
	}
	// get the next step
	int next = nextStep(amt);

	// tell them what to do
	// if in teaching mode
	if (teaching) {
		String message = "";
		message = message + values[next] + "c coin"; 
		String message2 = "";
		message2 = message2 + amt + "c more";
		String arr[4] = {F("Take out a"), message, F(""), message2};
		renderMessage(arr, 4, 3);
		to_blink[next] = CYCLE_LEN;
		screen = 5;
	} else {
		renderTeachScreen();
	}
}

bool calculateDoable(int amt) {
	if (amt <= 0) return false;
	for (int i = 0; i < 4; i++) {
		amt = amt - min(coin_counts[i], amt / values[i]) * values[i];
	}
	return amt == 0;
}

int nextStep(int amt) {
	for (int i = 0; i < 4; i++) {
		if (amt >= values[i] && coin_counts[i] > 0) return i;
	}
}

void renderTeachScreen() {
	screen = 2;

	Waveshield.fillScreen(LBLUE);
	drawBKbutton(0, 0);
	String message = "Withdraw ";
	message = message + (money - current_target) + "c";
	Waveshield.setTextSize(3);
	printCentredText(message, 65, WIDTH);

	Waveshield.setTextSize(2);
	printCentredText(F("Current coin counts"), HEIGHT / 2 - 125, WIDTH);
	for (int i = 0; i < 4; i++) {
		int x = WIDTH / 2 * (i % 2) + 5;
		int y = HEIGHT / 2 - 100 + 165 * (i / 2);
		int w = WIDTH / 2 - 10;
		int h = 155;

		Waveshield.fillRoundRect(x, y, w, h, 5, colors[i]);
		Waveshield.drawRoundRect(x, y, w, h, 5, BLACK);

		String lbl = "";
		lbl = lbl + values[i] + "c";
		Waveshield.setTextSize(2);
		Waveshield.setTextColor(text_colors[i]);
		printCentredText(lbl, y + 15, w, x, false);

		Waveshield.setTextSize(3);
		printCentredText(String(coin_counts[i]), y + h / 2, w, x, true);

		if (coin_counts[i] > 0) {
			to_blink[i] = CYCLE_LEN;
			digitalWrite(led_pins[i], HIGH);
		}
	}
	Waveshield.setTextColor(BLACK);
}

void renderStocks() {
	screen = 6;

	Waveshield.fillScreen(LBLUE);
	drawBKbutton(0, 0);

	Waveshield.setTextSize(2);
	printCentredText(F("Current coin counts"), HEIGHT / 2 - 135, WIDTH);
	for (int i = 0; i < 4; i++) {
		int x = WIDTH / 2 * (i % 2) + 5;
		int y = HEIGHT / 2 - 100 + 165 * (i / 2);
		int w = WIDTH / 2 - 10;
		int h = 155;

		Waveshield.fillRoundRect(x, y, w, h, 5, colors[i]);
		Waveshield.drawRoundRect(x, y, w, h, 5, BLACK);

		String lbl = "";
		lbl = lbl + values[i] + "c";
		Waveshield.setTextSize(2);
		Waveshield.setTextColor(text_colors[i]);
		printCentredText(lbl, y + 15, w, x, false);

		Waveshield.setTextSize(3);
		printCentredText(String(coin_counts[i]), y + h / 2, w, x, true);
	}
	Waveshield.setTextColor(BLACK);
}

