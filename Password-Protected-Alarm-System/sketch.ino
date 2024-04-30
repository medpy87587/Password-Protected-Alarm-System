#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD address to 0x27 for a 20 chars and 4 line display

#define PIR_PIN 5
#define BUTTON_PIN 18
#define BUZZER_PIN 12

/* Keypad setup */
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
byte rowPins[KEYPAD_ROWS] = {0, 4, 16, 17};
byte colPins[KEYPAD_COLS] = {25, 26, 27, 33};
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3', '+'},
    {'4', '5', '6', '-'},
    {'7', '8', '9', '*'},
    {'.', '0', '=', '/'}};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

SemaphoreHandle_t lcdMutex; // Mutex for protecting LCD access

TaskHandle_t handle_read_PIR;
TaskHandle_t handle_keypad;
TaskHandle_t handle_button;

static int PIR_val = 0;
bool systemActive = false;
bool alarmActive = false;
bool systemClosed = true;

const char *correctPassword = "1234"; // Define the correct password

bool checkPassword(const char *inputPassword)
{
    return strcmp(inputPassword, correctPassword) == 0;
}

void initializeSensors()
{
    pinMode(PIR_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void clearLCDLine(byte lineNumber)
{
    xSemaphoreTake(lcdMutex, portMAX_DELAY);
    lcd.setCursor(0, lineNumber);
    lcd.print("                    "); // Print empty spaces to clear the line
    xSemaphoreGive(lcdMutex);
}

void displayMessage(byte lineNumber, const char *message)
{
    xSemaphoreTake(lcdMutex, portMAX_DELAY);
    lcd.setCursor(0, lineNumber);
    lcd.print(message);
    xSemaphoreGive(lcdMutex);
}

void motionDetectionTask(void *parameter)
{
    Serial.println("Motion Detection Task started.");
    for (;;)
    {
        if (systemActive && !systemClosed)
        {
            PIR_val = digitalRead(PIR_PIN);
            if (PIR_val && !alarmActive)
            {
                tone(BUZZER_PIN, 100);
                displayMessage(1, "! Motion detected !");
            }
            
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void keypadTask(void *parameter)
{
    Serial.println("Keypad Task started.");
    char key;
    char enteredPassword[5] = ""; // Buffer to store entered password
    int passwordIndex = 0;

    for (;;)
    {
        key = keypad.getKey();
        if (key != NO_KEY && systemActive && !systemClosed)
        {
            Serial.println("Keypad input detected.");
            if (key == '=')
            {
                Serial.println("Check password.");
                if (checkPassword(enteredPassword))
                {
                    Serial.println("Password correct.");
                    systemClosed = true;
                    noTone(BUZZER_PIN);
                    clearLCDLine(0);
                    clearLCDLine(1);
                    clearLCDLine(2);
                    clearLCDLine(3);
                    systemActive = false;
                   

                    
                    displayMessage(0, "System closed");
                }
                else
                {
                    Serial.println("Incorrect password.");
                    displayMessage(3, "Incorrect password");
                    vTaskDelay(500 / portTICK_PERIOD_MS); // Clear message after demi seconds
                    clearLCDLine(3); 
                }
                memset(enteredPassword, 0, sizeof(enteredPassword));
                passwordIndex = 0;
            }
            else
            {
                Serial.println("Append key to entered password.");
                if (passwordIndex < 4)
                {
                    enteredPassword[passwordIndex++] = key;
                    displayMessage(2, "Entered Password: ");
                    displayMessage(3, String(enteredPassword).c_str());
                }
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void buttonTask(void *parameter)
{
    Serial.println("Button Task started.");
    for (;;)
    {
        if (digitalRead(BUTTON_PIN) == LOW)
        {
            if (!systemActive && systemClosed)
            {
                Serial.println("Button pressed. System activated.");
                systemActive = true;
                systemClosed = false;
                clearLCDLine(0);
                displayMessage(0, "System activated");
                displayMessage(1, "No motion detected");
            }
            
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    lcd.init();       // Initialize the LCD
    lcd.backlight();  // Turn on the backlight
    lcd.setCursor(0, 0);
    lcd.print("Initializing...");
    delay(2000);

    ledcSetup(0, 2500, 8);            // Use channel 0, frequency 2500 Hz, resolution 8 bits
    ledcAttachPin(BUZZER_PIN, 0);     // Attach the PWM channel to the pin

    lcd.clear();                       // Clear the LCD screen

    lcdMutex = xSemaphoreCreateMutex(); // Create the LCD mutex

    initializeSensors();
    xTaskCreate(motionDetectionTask, "MotionDetection", 2048, NULL, 1, &handle_read_PIR);
    xTaskCreate(keypadTask, "KeypadTask", 2048, NULL, 1, &handle_keypad);
    xTaskCreate(buttonTask, "ButtonTask", 2048, NULL, 1, &handle_button);

    Serial.println("Initialization complete");
}

void loop() {}
