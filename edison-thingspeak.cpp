#include <aio.hpp>
#include <bmpx8x.h>
#include <common.hpp>
#include <curl/curl.h>
#include <curl/easy.h>
#include <lm35.h>
#include <string.h>
#include <types.hpp>
#include <unistd.h>
#include <cstdio>
#include <iostream>

/*
 * Description: An Intel Edison project, where we, simply, post data from various sensors to a ThingsSpeak channel every 15 seconds.
 *
 *
 * Hardware: 1 x Gravity - Analog LM35 Temperature Sensor
 *           1 x Gravity - Analog Ambient Light Sensor
 *           1 x HIH4030 Humidity Sensor
 *           1 x BMP180 Barometer Module
 *           Resting on a Gravity - GPIO Shield
 *
 * Additional linker flags: upm-bmpx8x upm-lm35 curl
 */

#define THINGSPEAK_HOST "https://api.thingspeak.com"
#define API_KEY "YOUR_API_KEY"

#define TIMEOUT_IN_SECS 15 // Delay between requests

enum SensorPinMap {
	BUS_BMP180,
	PIN_PT550,
	PIN_LM35,
	PIN_HIH4030
};

// ------------ Global variables -----------------
// I2C interface for the BMP180 barometer module.
upm::BMPX8X *i2c_BMP180;
// Analog pin for the HIH4030 humidity sensor.
mraa::Aio *aio_HIH4030;
// Analog pin for the PT550 light sensor.
mraa::Aio *aio_PT550;
// Analog pin for the LM35 temperature sensor.
upm::LM35 *aio_LM35;
// Not really necessary since there is no exit mechanism, but there may be in the future.
bool APP_RUNNING = true;
// Voltage value for calculating relative humidity.
float supply_voltage = 5;

// ------------ Global functions -----------------
float getTemperature();
int getHumidity(int temp);
int getLightLevel();
int getPressure();

int main()
{
	// ---------------------- Platform Verification --------------------------
	mraa::Platform platform = mraa::getPlatformType();
	if (platform != mraa::INTEL_EDISON_FAB_C) {
		std::cerr << "Unsupported platform, exiting" << std::endl;
		return mraa::ERROR_INVALID_PLATFORM;
	}

	// ---------------------- Sensor Initialization --------------------------
	aio_HIH4030 = new mraa::Aio(PIN_HIH4030);
	if (aio_HIH4030 == NULL) {
		std::cerr << "HIH4030 initialization failed, exiting" << std::endl;
		return mraa::ERROR_UNSPECIFIED;
	}

	aio_PT550 = new mraa::Aio(PIN_PT550);
	if (aio_PT550 == NULL) {
		std::cerr << "PT550 initialization failed, exiting" << std::endl;
		return mraa::ERROR_UNSPECIFIED;
	}

	aio_LM35 = new upm::LM35(PIN_LM35);
	if (aio_LM35 == NULL) {
		std::cerr << "LM35 initialization failed, exiting" << std::endl;
		return mraa::ERROR_UNSPECIFIED;
	}

	i2c_BMP180 = new upm::BMPX8X(BUS_BMP180);
	if (i2c_BMP180 == NULL) {
		std::cerr << "BMP180 initialization failed, exiting" << std::endl;
		return mraa::ERROR_UNSPECIFIED;
	}

	// ------------------------- Curl Initialization ---------------------------
	CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
	if (res != CURLE_OK) {
		std::cerr << "Curl global initialization failed, exiting" << std::endl;
		return res;
	}
	// Initialize a curl object.
	CURL *curl = curl_easy_init();
	// If the curl initialization was successful, we can start our endless loop!
	if (curl) {
		// Don't verify SSL certificates. We could probably set some HTTP headers here for a more sophisticated request.
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

		char url[128];
		int humidity, pressure, light_level;
		float temperature;

		while (APP_RUNNING) {

			temperature = getTemperature();

			humidity = getHumidity(temperature);

			pressure = getPressure();

			light_level = getLightLevel();

			sprintf(url, "%s/update?api_key=%s&field1=%i&field2=%i&field3=%i&field4=%.2f", THINGSPEAK_HOST, API_KEY,
					humidity, pressure, light_level, temperature);

			// Set the URL for the curl object.
			curl_easy_setopt(curl, CURLOPT_URL, url);
			// Perform the request.
			res = curl_easy_perform(curl);

			if (res != CURLE_OK) {
				fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
			}
			// Clear bytes
			memset(url, 0, sizeof(url));

			sleep(TIMEOUT_IN_SECS);
		}

		curl_easy_cleanup(curl);

	} else {
		fprintf(stderr, "curl_easy_init() failed\n");
	}

	curl_global_cleanup();

	delete aio_LM35;
	delete aio_HIH4030;
	delete aio_PT550;
	delete i2c_BMP180;

	return mraa::SUCCESS;
}

float getTemperature()
{
	float raw_value = aio_LM35->getTemperature();
	float  calibrated_value = raw_value * 5.0/10.24;
	int rounded_value = round(calibrated_value);
	std::cout << "\nTemperature: " << rounded_value << std::endl;

	return rounded_value;
}

int getHumidity(int temp)
{
	int current_value = aio_HIH4030->read();
	std::cout << "Read value: " << current_value << std::endl;
	float voltage = supply_voltage*(0.0062*current_value + 0.16); // convert to voltage value
	std::cout << "Voltage: " << voltage << "V" << std::endl;
	// The following calculations derive from the HIH-4030 datasheet.
	float sensorRH =  (voltage - 0.958)/0.0307;
	std::cout << "sensorRH: " << sensorRH << "%" << std::endl;
	float trueRH = sensorRH / (1.0546 - (0.00216 * temp)); //temperature adjustment
	// Trim to two digits.
	trueRH = round(trueRH * 100.) / 100.;
	std::cout << "trueRH: " << trueRH  << "%"<< std::endl;

	return trueRH;
}

int getPressure()
{
	int current_value = i2c_BMP180->getPressure();
	std::cout << "Pressure: " << current_value << std::endl;

	return current_value;
}

int getLightLevel()
{
	int current_value = aio_PT550->read();
	std::cout << "Light voltage signal: " << current_value << std::endl;

	return current_value;
}




