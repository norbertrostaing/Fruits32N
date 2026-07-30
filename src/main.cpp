#include <Arduino.h>

#include "Olgalimex/_config.h"
#include "Olgalimex/_ethernet.h"
#include "Olgalimex/_wifi.h"
#include "Olgalimex/_dmx.h"
#include "Olgalimex/_artnet.h"
#include "Olgalimex/_osc.h"
#include "Olgalimex/_web.h"

int pinHeater = 2;

int pinTemperatureSensor = 32;
int pinLevelSensor = 35;

int pinRefillEnable = 4;
int pinRefillDirection = 5;
int pinRefillStep = 33;

int pinFloodEnable = 14;
int pinFloodDirection = 15;
int pinFloodStep = 13;

int pinSDA = 16;
int pinSCL = 3;

bool isFlooding = false;
bool isRestartingFlood = false;

bool alarmStatus = false;

float ml2steps = 266.666;

#include "Wire.h"
const uint8_t AS5600_ADDR = 0x36;

#include "FastAccelStepper.h"
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepperRefill = NULL;
FastAccelStepper *stepperMotorFlood = NULL;

#include <OneWire.h>
#include <DallasTemperature.h>

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(pinTemperatureSensor);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
DeviceAddress temperatureSensorAddress;

uint16_t readAS5600RawAngle()
{
    Wire.beginTransmission(AS5600_ADDR);
    Wire.write(0x0C); // RAW ANGLE MSB
    if (Wire.endTransmission(false) != 0) {
        //Serial.println("AS5600: erreur I2C");
        return 0;
    }

    Wire.requestFrom(AS5600_ADDR, (uint8_t)2);
    if (Wire.available() < 2) {
        //Serial.println("AS5600: lecture incomplete");
        return 0;
    }

    uint8_t high = Wire.read();
    uint8_t low = Wire.read();

    return ((high & 0x0F) << 8) | low; // 12 bits : 0-4095
}


float getAngleSensorValue()
{
    uint16_t angle = readAS5600RawAngle();
    return angle * 360.0 / 4096.0;
}

float getLevelSensorValue() {
	int levelSensorValue = analogRead(pinLevelSensor);
	return levelSensorValue;
}

float getTemperatureSensorValue() {
	sensors.requestTemperatures();
	float temperature = sensors.getTempC(temperatureSensorAddress);
	return temperature;
}

void relevelTanks() {
	float initAngle = getAngleSensorValue();
	float targetAngle = config["leveledAngle"].as<float>();
	float deltaAngle = targetAngle - initAngle;
	while (deltaAngle > 180.0) deltaAngle -= 360.0;
	while (deltaAngle < -180.0) deltaAngle += 360.0;
	stepperMotorFlood->setCurrentPosition(-deltaAngle * config["halfTurnFloodSteps"].as<int>() / 180.0);
	digitalWrite(pinFloodEnable, LOW);
	stepperMotorFlood->setSpeedInHz(300);
	stepperMotorFlood->setAcceleration(300);
	stepperMotorFlood->moveTo(0);
}


void startRefill() {
	if (isFlooding) return;
	relevelTanks();
	int steps = config["refillMilliliters"].as<float>() * ml2steps;
	stepperRefill->move(steps - config["currentRefillSteps"].as<int>());
}

void abortRefill() {
	if (isFlooding) return;
	stepperRefill->stopMove();
}

void emptyRefill() {
	if (isFlooding) return;
	stepperRefill->move(-config["currentRefillSteps"].as<int>());
}


void flood() {
	if (isFlooding) return;
	abortRefill();
	 
	if (digitalRead(pinFloodEnable) == HIGH) {
		digitalWrite(pinFloodEnable, LOW);
	    vTaskDelay(pdMS_TO_TICKS(100));
	}
	isFlooding = true;
	stepperMotorFlood->setSpeedInHz(config["floodSpeed"].as<int>());
	stepperMotorFlood->setAcceleration(config["floodAcceleration"].as<int>());
	stepperMotorFlood->move(-config["halfTurnFloodSteps"].as<int>());
	stepperRefill->setCurrentPosition(0);
}

void resetFlood() {
	if (!isFlooding) return;
	if (stepperMotorFlood->isRunning()) { return; }
	isFlooding = false;
	stepperMotorFlood->setSpeedInHz(config["resetFloodSpeed"].as<int>());
	stepperMotorFlood->setAcceleration(config["resetFloodAcceleration"].as<int>());
	stepperMotorFlood->setAutoEnable(true);
	stepperMotorFlood->move(-config["halfTurnFloodSteps"].as<int>());
	isRestartingFlood = true;
	stepperRefill->setCurrentPosition(0);

}

void startHeater() {
	analogWrite(pinHeater, config["heatRatePercent"].as<int>() * 255 / 100);
	info["isHeaterOn"] = true;
}


void stopHeater() {
	analogWrite(pinHeater, 0);
	info["isHeaterOn"] = false;
}

void setup() {
	// generic parameters
	oscPort = 9004;
	chipName = "Final32N";
	eepromMarker = 0x5D;

	pinMode(pinHeater, OUTPUT);
	pinMode(pinLevelSensor, INPUT);
	pinMode(pinRefillEnable, OUTPUT);
	pinMode(pinRefillDirection, OUTPUT);
	pinMode(pinRefillStep, OUTPUT);
	pinMode(pinFloodEnable, OUTPUT);
	pinMode(pinFloodDirection, OUTPUT);
	pinMode(pinFloodStep, OUTPUT);

	// config : read and write parameters
	// infos : read only parameters
	// addIntConfig("offFade", 1000);
	addFloatConfig("leveledAngle", 40.0);

	addIntConfig("heatRatePercent", 100);
	addFloatConfig("temperatureTriggerHigh", 40.0);
	addFloatConfig("temperatureTriggerLow", 25.0);

	addBoolConfig("disableLevelSensor", false);
	addIntConfig("levelSensorHeatValue", 1000);
	addIntConfig("levelSensorAlarmValue", 1700);

	addIntConfig("refillSpeed", 1777);
	addIntConfig("refillAcceleration", 1777);
	addIntConfig("refillMilliliters", 2000);

	addIntConfig("halfTurnFloodSteps", 2400);

	addIntConfig("floodSpeed", 20000);
	addIntConfig("floodAcceleration", 20000);

	addIntConfig("resetFloodSpeed", 15000);
	addIntConfig("resetFloodAcceleration", 15000);

	addIntConfig("currentRefillSteps", 0);

	addStringConfig("alarmIP", "");

	addTrigger("FLOOD");
	addTrigger("resetFlood");
	addTrigger("startRefill");
	addTrigger("abortRefill");
	addTrigger("emptyRefill");

	addTrigger("startHeater");
	addTrigger("stopHeater");

	info["isHeaterOn"] = false;
	Serial.begin(115200);
	initConfig();
	setupEthernet();
	setupWifi();
	setupOSC();
	setupWebServer();
	// setupArtnet();
	// receivePin = 36;
	// setupDMX();

	Wire.begin(pinSDA, pinSCL);

    sensors.begin();
	sensors.getAddress(temperatureSensorAddress, 0);
    sensors.setResolution(temperatureSensorAddress, 12);
    sensors.setWaitForConversion(true);


	// moteur pas à pas
	engine.init();
	stepperRefill = engine.stepperConnectToPin(pinRefillStep);
	if (stepperRefill) {
		stepperRefill->setDirectionPin(pinRefillDirection);
		stepperRefill->setEnablePin(pinRefillEnable);
		stepperRefill->setAutoEnable(true);

		stepperRefill->setSpeedInHz(config["refillSpeed"].as<int>());
		stepperRefill->setAcceleration(config["refillAcceleration"].as<int>());
	   	//stepperRefill->enableOutputs();
	}

	stepperMotorFlood = engine.stepperConnectToPin(pinFloodStep);
	if (stepperMotorFlood) {
		stepperMotorFlood->setDirectionPin(pinFloodDirection);
		// stepperMotorFlood->setEnablePin(pinFloodEnable);
		// stepperMotorFlood->setAutoEnable(true);
		// stepperMotorFlood->setDelayToEnable(50000);
		// stepperMotorFlood->setDelayToDisable(50000);
		
		stepperMotorFlood->setSpeedInHz(config["floodSpeed"].as<int>());
		stepperMotorFlood->setAcceleration(config["floodAcceleration"].as<int>());
	   	//stepperMotorFlood->enableOutputs();
	}

    // //Serial.print("DS18B20 count = ");
    // //Serial.println(sensors.getDeviceCount());

    // //Serial.print("Parasite power = ");
    // //Serial.println(sensors.isParasitePowerMode() ? "YES" : "NO");
	digitalWrite(pinFloodEnable, HIGH);

	writeConfig("disableLevelSensor", false);
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));

	if (config["currentRefillSteps"].as<int>() != stepperRefill->getCurrentPosition()) {
		writeConfig("currentRefillSteps", stepperRefill->getCurrentPosition());
	}

	float angle = getAngleSensorValue();
	float level = getLevelSensorValue();
	float temperature = getTemperatureSensorValue();
	bool allowedToHeat = true;

	info["currentAngle"] = angle;
	info["levelSensorValue"] = level;

	if (!config["disableLevelSensor"].as<bool>()) {
		if (level > config["levelSensorAlarmValue"].as<int>()) {
			allowedToHeat = true; // ?
			info["levelStatus"] = "ALARM";
			String ip = config["alarmIP"].as<String>();
			if (ip != "") {
				OscWiFi.getClient().send(ip, oscPort, "/ALARM"); 
			}
		} else if (level > config["levelSensorHeatValue"].as<int>()) {
			// ok to heat
			info["levelStatus"] = "can heat";
			allowedToHeat = true;
		} else {
			allowedToHeat = false;
			info["levelStatus"] = "empty";
		}
	}

	info["currentTemperature"] = temperature;
	if (temperature != -127) {

		float trigTempHigh = config["temperatureTriggerHigh"].as<float>();
		float trigTempLow = config["temperatureTriggerLow"].as<float>();

		if (info["isHeaterOn"].as<bool>()) {
			if (temperature > trigTempHigh || !allowedToHeat) {
				stopHeater();
			} 
		} else {
			if (temperature < trigTempLow && allowedToHeat) {
				startHeater();
			}
		}
		
	}

	if (isRestartingFlood) {
		if (stepperMotorFlood->getCurrentPosition() == stepperMotorFlood->getPositionAfterCommandsCompleted()) {
			isRestartingFlood = false;
			digitalWrite(pinFloodEnable, HIGH);
			writeConfig("currentRefillSteps", 0);
		}
	}

}


void onArtnetFrame(uint16_t universeRcv, uint16_t length, uint8_t sequence, uint8_t *data, IPAddress remoteIP)
{
}

void onDmxFrame()
{
}


void triggerTriggered(String str) {
	if (str == "FLOOD") {
		flood();
	} else if (str == "resetFlood") {
		resetFlood();
	} else if (str == "startRefill") {
		startRefill();
	} else if (str == "abortRefill") {
		abortRefill();
	} else if (str == "emptyRefill") {
		emptyRefill();
	} else if (str == "startHeater") {
		startHeater();
	} else if (str == "stopHeater") {
		stopHeater();
	}
}

void configUpdatedMain(String str) {
	if (str == "refillSpeed") {
		stepperRefill->setSpeedInHz(config["refillSpeed"].as<int>());
	} else if (str == "refillAcceleration") {
		stepperRefill->setAcceleration(config["refillAcceleration"].as<int>());
	} else if (str == "floodSpeed") {
		stepperMotorFlood->setSpeedInHz(config["floodSpeed"].as<int>());
	} else if (str == "floodAcceleration") {
		stepperMotorFlood->setAcceleration(config["floodAcceleration"].as<int>());
	}
}