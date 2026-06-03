#include <stdint.h>
#include <Wire.h>

// -- Buttons

class Button {
private:
    uint8_t pin;
    bool state;
    bool lastReading;
    uint32_t lastDebounceTime;
    const uint32_t debounceDelay = 50;

public:
    Button(uint8_t p) : pin(p), state(HIGH), lastReading(HIGH), lastDebounceTime(0) {
        pinMode(pin, INPUT_PULLUP);
    }

    bool isReleased() {
        bool reading = digitalRead(pin);
        uint32_t now = millis();

        if (reading != lastReading) {
            lastDebounceTime = now;
        }

        if ((now - lastDebounceTime) > debounceDelay) {
            if (state != reading) {
                state = reading;
                if (state == HIGH) {
                    lastReading = reading;
                    return true;
                }
            }
        }

        lastReading = reading;
        return false;
    }
};

// -- LCD

const int LCD_E = 12;
const int LCD_RS = 11;
const int LCD_DB4 = 7;
const int LCD_DB5 = 8;
const int LCD_DB6 = 9;
const int LCD_DB7 = 10;

void LCD_Write4Bits(uint8_t dtw) {
	digitalWrite(LCD_E, LOW);
	digitalWrite(LCD_DB4, dtw & 0x01);
	digitalWrite(LCD_DB5, dtw & 0x02);
	digitalWrite(LCD_DB6, dtw & 0x04);
	digitalWrite(LCD_DB7, dtw & 0x08);
	digitalWrite(LCD_E, HIGH);
	delay(2);
	digitalWrite(LCD_E, LOW);
	delay(2);
}
void LCD_WriteData(uint8_t dtw) {
	digitalWrite(LCD_RS, HIGH);
	LCD_Write4Bits(dtw >> 4);
	LCD_Write4Bits(dtw & 0x0F);
}
void LCD_WriteCommand(uint8_t dtw) {
	digitalWrite(LCD_RS, LOW);
	LCD_Write4Bits(dtw >> 4);
	LCD_Write4Bits(dtw & 0x0F);
}
void LCD_Init() {
	pinMode(LCD_E, OUTPUT);
	pinMode(LCD_RS, OUTPUT);
	pinMode(LCD_DB4, OUTPUT);
	pinMode(LCD_DB5, OUTPUT);
	pinMode(LCD_DB6, OUTPUT);
	pinMode(LCD_DB7, OUTPUT);
	
	digitalWrite(LCD_RS, LOW);
	digitalWrite(LCD_E, LOW);
	LCD_Write4Bits(0x03);
	delay(5);
	LCD_Write4Bits(0x03);
	delay(2);
	LCD_Write4Bits(0x03);
	delay(2);
	LCD_Write4Bits(0x02);
	LCD_WriteCommand(0x28);
	LCD_WriteCommand(0x0C);
	LCD_WriteCommand(0x01);
	LCD_WriteCommand(0x06);
}

void LCD_WriteText(const char *ttw) {
	while(*ttw)
		LCD_WriteData(*ttw++);
}

void LCD_WriteTextFormat(const char* fmt, ...) {
	char buffer[64] = {0};
	va_list args;
	va_start(args, fmt);
    int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    buffer[sizeof(buffer) - 1] = '\0';
    LCD_WriteText(buffer);
}

void LCD_Clear() {
	LCD_WriteCommand(0x01);
}

// -- LED

const int LED_0 = A0;
const int LED_1 = A1;
const int LED_2 = A2;
const int LED_3 = A3;

void LED_SetValue(int value) {
	if (value < 0 || value > 9)
		return;
	digitalWrite(LED_0, value & 0x01);
	digitalWrite(LED_1, value & 0x02);
	digitalWrite(LED_2, value & 0x04);
	digitalWrite(LED_3, value & 0x08);
}

void LED_Init() {
	pinMode(LED_0, OUTPUT);
	pinMode(LED_1, OUTPUT);
	pinMode(LED_2, OUTPUT);
	pinMode(LED_3, OUTPUT);
}

// -- PCF

const int PCF8574 = 0x20;
uint32_t PCFLastRequest = 0;
uint8_t PCFLastValue = 0;

uint8_t PCF_ReadByte() {
	if (PCFLastRequest == millis())
		return PCFLastValue;
	
    Wire.requestFrom(PCF8574, 1);
	uint8_t value = 0;
    if (Wire.available())
        value = Wire.read();
	
	PCFLastRequest = millis();
	PCFLastValue = value;
	
  	return value;
}

uint8_t PCF_ReadPin(uint8_t index) {
	return PCF_ReadByte() & (1 << index);
}

// -- Main

#define MAX_TIME 0xFFFFFFFF

Button nextButton(6);
Button clickButton(5);
Button backButton(4);
const int SOUND_PIN = 13;
const int LED_PIN = 3;

bool isAnyReleased() {
	return nextButton.isReleased() || clickButton.isReleased() || backButton.isReleased();
}

void setup() {
	LCD_Init();
	LCD_WriteCommand(0x80);
  
	LED_Init();
	LED_SetValue(0);
  
	pinMode(SOUND_PIN, OUTPUT);
	pinMode(LED_PIN, OUTPUT);
  
	Serial.begin(9600);
  	Wire.begin();
  
	LCD_Clear();
	LCD_WriteText("System alarmowy");
  
  	while (true) {
		if (isAnyReleased())
			break;
    }
	
	LCD_Clear();
}


bool bEnabled = false;
int activationSensor = -1;
uint32_t activationTime = MAX_TIME;

bool bSensorsMode = false;
int sensorIndex = 0;
int lastSensorIndex = -1;
uint8_t enabledSensors = 0x00;

bool bInfoMode = false;
uint32_t lastUpdateDisplay = 0;


int menuIndex = 0;
int lastMenuIndex = -1;
const char* menuOptions[] = {"Toggle", "Reset", "Info", "Czujniki"};
#define MENU_SIZE 4

void updateLED() {
	if (isAlarmActivated()) {
		LED_SetValue(3);
	} else {
		if (!bEnabled)
			LED_SetValue(0);
		else
			LED_SetValue(1);
	}
}

void activateAlarm(uint8_t sensor) {
	activationSensor = sensor;
	activationTime = millis();
	updateLED();
}

void toggleAlarm() {
	bEnabled = !bEnabled;
	if (!bEnabled)
		resetAlarm();
	else {
		uint16_t value = 10;
		bool bDirty = true;
		while (true) {
			if (bDirty) {
				LCD_Clear();
				LCD_WriteTextFormat("%is", value);
				bDirty = false;
			}
			
			if (nextButton.isReleased() && value < UINT32_MAX) {
				value++;
				bDirty = true;
			} else if (backButton.isReleased() && value > 0) {
				value--;
				bDirty = true;
			} else if (clickButton.isReleased())
				break;
		}
		
		LED_SetValue(2);
		for (int i = 0; i < value; ++i) {
			LCD_Clear();
			LCD_WriteTextFormat("%is / %is", i, value);
			tone(SOUND_PIN, 500 + 500 * ((float)i / value), 250);
			delay(1000);
		}
		
		LCD_Clear();
		LCD_WriteText("Alarm uzbrojony!");
		
		tone(SOUND_PIN, 523);
		delay(200);
		noTone(SOUND_PIN);
		delay(50);

		tone(SOUND_PIN, 659);
		delay(200);
		noTone(SOUND_PIN);
		delay(50);

		tone(SOUND_PIN, 784);
		delay(400);
		noTone(SOUND_PIN);
		delay(50);

		tone(SOUND_PIN, 1047);
		delay(400);
		noTone(SOUND_PIN);
		delay(50);

		tone(SOUND_PIN, 784);
		delay(200);
		noTone(SOUND_PIN);
		delay(50);

		tone(SOUND_PIN, 659);
		delay(200);
		noTone(SOUND_PIN);
		delay(50);

		tone(SOUND_PIN, 523);
		delay(400);
		noTone(SOUND_PIN);
	}
	updateLED();
}

void resetAlarm() {
	activationTime = MAX_TIME;
	activationSensor = -1;
	updateLED();
}

bool isAlarmActivated() {
	return activationTime != MAX_TIME;
}

bool isSensorEnabled(uint8_t index) {
	return enabledSensors & (1 << index);
}

void toggleSensor(uint8_t index) {
	if (isSensorEnabled(index))
		enabledSensors &= ~(1 << index);
	else
		enabledSensors |= (1 << index);
}

void detect() {
	if (isAlarmActivated())
		return;
  
	for (int i = 0; i < 8; ++i) {
		if (!isSensorEnabled(i))
			continue;
		
		if (PCF_ReadPin(i)) {	
			activateAlarm(i);
			break;
		}
	}
}

void drawInfo() {
	if (isAnyReleased()) {
		bInfoMode = false;
		return;
	}
  
	if (lastUpdateDisplay >= millis() - 1000)
		return;
  
	lastUpdateDisplay = millis();
	LCD_Clear();
  
	if (!bEnabled)
		LCD_WriteText("Alarm OFF");
	else if (activationTime == MAX_TIME)
		LCD_WriteText("Alarm ON");
	else {
		LCD_WriteCommand(0x80);
		LCD_WriteTextFormat("Sensor %i", activationSensor);
		LCD_WriteCommand(0xC0);
		LCD_WriteTextFormat("%lus temu", (millis() - activationTime) / 1000);
	}
}

void drawMenu() {
	if (nextButton.isReleased())
		menuIndex = (menuIndex + 1) % MENU_SIZE;
	if (backButton.isReleased())
		menuIndex = (menuIndex - 1 + MENU_SIZE) % MENU_SIZE;
  
	if (clickButton.isReleased()) {
		lastMenuIndex = -1;
		if (menuIndex == 0)
			toggleAlarm();
		else if (menuIndex == 1)
			resetAlarm();
		else if (menuIndex == 2) {
			bInfoMode = 1;
			return;
		}
		else if (menuIndex == 3) {
			bSensorsMode = 1;
			return;
		}
	}
  
	if (lastMenuIndex != menuIndex) {
		LCD_Clear();
    
		if (menuIndex == 0)
			LCD_WriteText(bEnabled == 1 ? "Wylacz" : "Wlacz");
		else
			LCD_WriteText(menuOptions[menuIndex]);
	}
  
	lastMenuIndex = menuIndex;
}

void drawSensors() {
	if (nextButton.isReleased())
		sensorIndex = (sensorIndex + 1) % 10;
	if (backButton.isReleased())
		sensorIndex = (sensorIndex + 9) % 10;
  
	if (clickButton.isReleased()) {
		lastSensorIndex = -1;
		if (sensorIndex == 9) {
			bSensorsMode = false;
			return;
		} else if (sensorIndex == 8) {
			uint8_t lastByte = 0x00;
			bool bActive = false;
			while (!isAnyReleased()) {
				if (lastByte != PCF_ReadByte()) {
					bActive = false;
					LCD_Clear();
					LCD_WriteCommand(0x80);
					LCD_WriteText("01234567");
					LCD_WriteCommand(0xC0);
					for (int i = 0; i < 8; ++i) {
						if (isSensorEnabled(i) && PCF_ReadPin(i)) {
							LCD_WriteText("X");
							bActive = true;
						} else {
							LCD_WriteText(" ");
						}
					}
					
					lastByte = PCF_ReadByte();
				}
				
				if (bActive)
					LED_SetValue(4);
				else
					updateLED();
			}
		} else 
			toggleSensor(sensorIndex);
	}
  
	if (lastSensorIndex != sensorIndex) {
		LCD_Clear();
		if (sensorIndex == 8)
			LCD_WriteText("Test");
		else if (sensorIndex == 9)
			LCD_WriteText("Wyjdz");
		else
			LCD_WriteTextFormat("Sensor %i: %s", sensorIndex, isSensorEnabled(sensorIndex) ? "V" : "X");
	}
  
	lastSensorIndex = sensorIndex;
}

void updateAlarm() {
	if (activationTime == MAX_TIME) {
		noTone(SOUND_PIN);
		digitalWrite(LED_PIN, HIGH);
		return;
	}
  
	digitalWrite(LED_PIN, millis() % 500 > 250 ? HIGH : LOW);
	tone(SOUND_PIN, 100 + (sin(millis() / 1000.0f * 3.1415f) + 1.0f) * 250);
}

void draw() {
	if (bSensorsMode)
		drawSensors();
	else if (bInfoMode)
		drawInfo();
	else
		drawMenu();
}

void loop() {  
	updateAlarm();
  
	if (bEnabled)
		detect();
  
	draw();
}
