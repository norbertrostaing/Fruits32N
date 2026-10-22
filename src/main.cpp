#include <Arduino.h>

#include "Olgalimex/_config.h"
#include "Olgalimex/_ethernet.h"
#include "Olgalimex/_wifi.h"
#include "Olgalimex/_dmx.h"
#include "Olgalimex/_artnet.h"
#include "Olgalimex/_osc.h"
#include "Olgalimex/_web.h"

// RX1 36
// TX1 4

const int nMotors = 10;
int oscPosition[nMotors];
int serialPosition[nMotors];
int inflate[nMotors];
int deflate[nMotors];

static constexpr size_t SerialBufferSize = 64;
static char serial_buffer[SerialBufferSize];
static size_t serial_buffer_len = 0;

uint16_t crc16(const String& str)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < str.length(); i++)
    {
        crc ^= (uint8_t)str[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

void sendMotor(int motor) {
	int oscPos = oscPosition[motor];
	if (oscPos == serialPosition[motor]) return;
	String motorName = "motor"+String(motor+1);
	int speed = config[motorName+"/speed"].as<int>();
	int acc = config[motorName+"/acceleration"].as<int>();
	String send = String(motor+1) + " " + String(oscPos) + " " + String(speed) + " " + String(acc);
	String frame = send + "*" + String(crc16(send), HEX);
	Serial1.println(frame);
	serialPosition[motor] = oscPos;
}

void setMotor(int motor, float to) {
	to = min(1.0f, to);
	to = max(0.0f, to);
	String motorName = "motor"+String(motor+1);
	int pos = config[motorName+"/maxPosition"].as<int>() * to;
	oscPosition[motor] = pos;
}

void resetMotor(int motor) {
	Serial1.print("reset ");
	Serial1.println(motor+1);
	Serial.print("reset ");
	Serial.println(motor+1);
}

void setup() {
	// generic parameters
	oscPort = 9004;
	chipName = "Fruit32N";
	eepromMarker = 0x56;

	// config : read and write parameters
	// infos : read only parameters
	// addIntConfig("offFade", 1000);

	for (int i = 0; i < nMotors; i++) {
		oscPosition[i] = 0;
		serialPosition[i] = 0;
		inflate[i] = false;
		deflate[i] = false;
		addIntConfig("motor" + String(i+1) + "/maxPosition", 4000);
		addIntConfig("motor" + String(i+1) + "/speed", 4000);
		addIntConfig("motor" + String(i+1) + "/acceleration", 4000);
		addTrigger("resetMotor" + String(i+1));
	}


	Serial.begin(115200);
	Serial1.begin(921600);
	initConfig();
	setupEthernet();
	setupWifi();
	setupOSC();
	setupWebServer();

	for (int i = 0; i < nMotors; i++) {
		oscSubscribe("/motor" + String(i+1) + "", [i](const OscMessage& m){
			if (m.size()>0) {
				setMotor(i, m.getArgAsFloat(0));
			}
		}); 
		oscSubscribe("/inflate" + String(i+1) + "", [i](const OscMessage& m){
			inflate[i] = true;
		}); 
		oscSubscribe("/deflate" + String(i+1) + "", [i](const OscMessage& m){
			deflate[i] = true;
		}); 
	}

	oscSubscribe("/serial", [](const OscMessage& m){
		if (m.size()>0) {
			Serial1.println(m.getArgAsString(0));
		}
	});

	// setupArtnet();
	// receivePin = 36;
	// setupDMX();
}

void loop()
{	
	for (int i = 0; i< nMotors; i++) {
		if (inflate[i]) {
			inflate[i] = false;
			int max = config["motor" + String(i+1) + "/maxPosition"].as<int>() + 2500;
			Serial.println("new max " + String(max));
			writeConfig("motor" + String(i+1) + "/maxPosition", max);
			setMotor(i, 1.0f);
		} 
		if (deflate[i]) {
			deflate[i] = false;
			int max = config["motor" + String(i+1) + "/maxPosition"].as<int>() - 2500;
			writeConfig("motor" + String(i+1) + "/maxPosition", max);
			setMotor(i, 1.0f);
		}
		sendMotor(i);
	}
    vTaskDelay(pdMS_TO_TICKS(200));
}


void onArtnetFrame(uint16_t universeRcv, uint16_t length, uint8_t sequence, uint8_t *data, IPAddress remoteIP)
{
}

void onDmxFrame()
{
}


void triggerTriggered(String str) {
	for (int i = 0; i< nMotors; i++) {
		if (str == "resetMotor"+String(i+1) ) {
			resetMotor(i);
		}
	}
}

void configUpdatedMain(String str) {
}