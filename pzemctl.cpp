// Including Arduino.h is required for using Serial functions
#include <Arduino.h>
#include <SPI.h>
#include <HardwareSerial.h>
#include "Pzem004t.h"
#include "Network.h"
#include <src/Arduino_FreeRTOS.h>
#include <src/task.h>
#include "DisplaySSD1306.h"
#include "Led.h"
#include "Input.h"
#include "WebServer.h"
#include "Configuration.h"

const int SWITCH_PIN = 28;

Configuration* configuration;
hw::Pzem004t* pzem;
hw::Input* input;
hw::DisplaySSD1306 display;
Network *nw;
web::WebServer* webServer;
hw::Led led;

hw::pzem004tvalues phasesCombined;
hw::pzem004tvalues phase1Values;
hw::pzem004tvalues phase2Values;
hw::pzem004tvalues phase3Values;

bool setupModeRequested = false;
bool setupModeEnabled = false;

bool switchState = false;

void setupMode()
{
    setupModeEnabled = true;
    Serial.println(F("SETUP"));
    display.showMessage("Starting\nSetup");
    nw->enableConfigMode();

    IPAddress ip = nw->ipAddress();
    char message[18];
    sprintf(message, "%i.%i.\ņ%i.%i", ip[0], ip[1], ip[2], ip[3]);
    display.showMessage(message);

    webServer->enable();
    while(webServer->enabled() && setupModeEnabled)
    {
        vTaskDelay( 500 / portTICK_PERIOD_MS);
    }
    if(setupModeEnabled)
    {
        display.showMessage("Configu\nsaved");
    }
    else
    {
        display.showMessage("Setup\ncancelled");
    }

    webServer->disable();
    nw->disableConfigMode();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    display.clearMessage();

    setupModeEnabled = false;
}

void TaskPollPzem(void *pvParameters)
{
    while(true)
    {
        pzem->update();

        phase1Values = pzem->values1();
        phase2Values = pzem->values2();
        phase3Values = pzem->values3();

        phasesCombined.voltage = (phase1Values.voltage + phase2Values.voltage + phase3Values.voltage) / 3;
        phasesCombined.current = phase1Values.current + phase2Values.current + phase3Values.current;
        phasesCombined.energy = phase1Values.energy + phase2Values.energy + phase3Values.energy;
        phasesCombined.frequency = (phase1Values.frequency + phase2Values.frequency + phase3Values.frequency) / 3;
        phasesCombined.pf = (phase1Values.pf + phase2Values.pf + phase3Values.pf) / 3;

        vTaskDelay( 200 / portTICK_PERIOD_MS);
    }
}

void TaskDisplay(void *pvParameters)
{
    while(true)
    {
        display.update(phasesCombined, phase1Values, phase2Values, phase3Values);

        vTaskDelay( 250 / portTICK_PERIOD_MS);
    }
}

void TaskInput(void *pvParameters)
{
    while(true)
    {
        input->update();

        vTaskDelay( 25 / portTICK_PERIOD_MS);
    }
}

void TaskNetwork(void *pvParameters)
{
    while(true)
    {
        nw->update(phase1Values, phase2Values, phase3Values);

        vTaskDelay( 100 / portTICK_PERIOD_MS);
    }
}

void TaskSetupMode(void *pvParameters)
{
    while(true)
    {
        if(setupModeRequested)
        {
            setupModeRequested = false;
            if(!setupModeEnabled)
            {
                setupMode();
            }
        }

        vTaskDelay( 200 / portTICK_PERIOD_MS);
    }
}

void TaskWebServer(void *pvParameters)
{
    while(true)
    {
        webServer->update();
        vTaskDelay( 500 / portTICK_PERIOD_MS);
    }
}

void setupTasks()
{
    xTaskCreate(
            TaskPollPzem
            ,  "Pzm"
            ,  64
            ,  NULL
            ,  2
            ,  NULL );

    xTaskCreate(
            TaskDisplay
            ,  "Dsp"
            ,  256
            ,  NULL
            ,  0
            ,  NULL );

    xTaskCreate(
            TaskInput
            ,  "Input"
            ,  64
            ,  NULL
            ,  0
            ,  NULL );

    xTaskCreate(
            TaskNetwork
            ,  "Nw"
            ,  768
            ,  NULL
            ,  1
            ,  NULL );

    xTaskCreate(
            TaskSetupMode
            ,  "Setup"
            ,  256
            ,  NULL
            ,  0
            ,  NULL );

    xTaskCreate(
            TaskWebServer
            ,  "WS"
            ,  448
            ,  NULL
            ,  0
            ,  NULL );

}

void buttonPressed(hw::ButtonId buttonId)
{
    switch(buttonId)
    {
        case hw::ButtonId::SwitchPhase:
            display.switchPhase();
            break;
        case hw::ButtonId::SwitchPhaseLong:
        {
            if(setupModeEnabled)
            {
                setupModeEnabled = false;
            }
            else
            {
                setupModeRequested = true;
            }

            break;
        }
        case hw::ButtonId::SwitchView:
            display.switchView();
            led.setBrightnessSwitchState(switchState ? 0 : 255);
            break;
        case hw::ButtonId::SwitchOnOff:
            Serial.println("ON_OFF");
            switchState = !switchState;
            digitalWrite(SWITCH_PIN, switchState);
            break;
    }
}

void configurationChanged()
{
    nw->configurationChanged();
}

void setup()
{
	Serial.begin(9600);
    Serial.println(F("Start"));

    pinMode(SWITCH_PIN, OUTPUT);
    digitalWrite(SWITCH_PIN, switchState);

    randomSeed(analogRead(0));

    configuration = new Configuration(configurationChanged);

    input = new hw::Input(buttonPressed);
    nw = new Network(configuration);
    webServer = new web::WebServer(nw->ethernetClient(), configuration);

    input->initialize();
    display.initialize();
    led.initialize();

    pzem = new hw::Pzem004t(&Serial1, &Serial2, &Serial3);
    nw->initialize();

	setupTasks();

    setupModeRequested = !configuration->hasValidSignature();
}

void loop()
{

}
