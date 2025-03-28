#include <Arduino.h>
#include "power_manager.h"

PowerManager::PowerManager(WaterTankPowerHandler* handler, 
int8_t min_time_day, 
int8_t max_time_day, 
int8_t min_time_night,
int8_t max_time_night, 
float min_temperature_night, 
float night_power, 
float water_volume)
: _handler(handler), 
  _missed_power_update(0),
  _manual_mod(false),
  _onStart(0),
  _onStop(0),
  _println(0),
  _temperature(0),
  _min_temperature_night(min_temperature_night),
  _night_anticipation(0.0),
  _night_power(night_power),
  _water_volume(water_volume),
  _min_time_day(min_time_day),
  _max_time_day(max_time_day),
  _min_time_night(min_time_night),
  _max_time_night(max_time_night),
  _inhibitions_days_left(0),
  _dayEnergyCounter(0.0),
  _nightEnergyCounter(0.0),
  _totalDayEnergyCounter(0.0),
  _totalNightEnergyCounter(0.0),
  _isDayChange(false),
  _last_time_us(0),
  _legionelosis_activation(false),
  _legionelosis_temperature(0.0),
  _legionelosis_max_delay(0),
  _legionelosis_delay(0)
{
}

void PowerManager::println(String s) {
  if (_println) _println(s);
}

void PowerManager::startPowerListener() {
  if (_onStart) _onStart();
}

void PowerManager::stopPowerListener() {
  if (_onStop) _onStop();
}

void PowerManager::onFailure() {
  _handler->setWaterTankOff();
  stopPowerListener();
}

void PowerManager::updatePowerCounter(bool isDay) {
  int64_t current_time = esp_timer_get_time();

  if (_last_time_us == 0 || current_time <= _last_time_us) {
    // This is reset value or timer reset, just ignore it for the first time
    println("UpdatePowerCounter : Timer reset");
    return;
  }

  float currentPower = (float) _handler->getWaterTankPower();
  if (isDay && currentPower >= _night_power) {
    // During day, if max power is reached, it means the water tank is overheating, so ignore value
    println("UpdatePowerCounter : Probably overheating");
    return;
  }
  float addedEnergy = currentPower * ((float)(current_time - _last_time_us)) / (1000.0 * 1000.0 * 60.0 * 60.0);
  if (isDay) {
   _dayEnergyCounter += addedEnergy;
  }
  else {
    _nightEnergyCounter += addedEnergy;
  }
}

void PowerManager::updateLastTime() {
  _last_time_us = esp_timer_get_time();
}

float PowerManager::getNightTemperature() {
  if (_legionelosis_activation && _legionelosis_delay == 0) {
    return _legionelosis_temperature;  
  }

  return _min_temperature_night;
}

void PowerManager::setTemperature(float temp) {
  _temperature = temp;
  if (_legionelosis_activation && temp >= _legionelosis_temperature) {
    _legionelosis_delay = _legionelosis_max_delay;
  }
}

void PowerManager::onRefresh(bool isTimer) 
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    println("Failed to obtain time");
    onFailure();
    // Default : heat
    _handler->setWaterTankNightPower();
    return;
  }
  int8_t tm_hour = timeinfo.tm_hour;
  bool isDayTime = false;

  if (!_isDayChange && tm_hour == 0) {
     _isDayChange = true;
     // Done once at midnight
     if (_inhibitions_days_left > 0) {
        _inhibitions_days_left--;
     }
     if (_legionelosis_delay > 0) {
        _legionelosis_delay--;
     }
     _totalDayEnergyCounter += _dayEnergyCounter / 1000.0;
     _totalNightEnergyCounter += _nightEnergyCounter / 1000.0;
     _dayEnergyCounter = 0.0;
     _nightEnergyCounter = 0.0;
  }
  else if (tm_hour != 0) {
     _isDayChange = false;
  }

  if (_manual_mod) {
    println("Heating : manual mod");
    _handler->setWaterTankNightPower();
    updatePowerCounter(false);
  }
  else if (tm_hour >= _min_time_day && tm_hour < _max_time_day) {
    isDayTime = true;
    if (_missed_power_update > 0) {
      // No update for a while, turn off and wait for it
      startPowerListener();
      println("No heating : Start power listerners and turn off");
      _handler->setWaterTankOff();
    }
  }
  else if ((tm_hour >= _min_time_night && tm_hour < _max_time_night)) {
     if (_inhibitions_days_left > 0) {
        println(String("No heating : Inhibited for ") + _inhibitions_days_left);
       _handler->setWaterTankOff();
     }
     else if (_temperature < getNightTemperature()) {
        // Only during night, limit temperature
        // (day temperature is unlimited, handled by the water tank itself)
        // But start only when needed to reach _min_temperature_night at _max_time_night
        // delta(time) = volume * 1.163 * delta(temp) / power
        float time_needed = _water_volume * 1.163 * (getNightTemperature() - _temperature) / _night_power;
        float current_time_hour = (float) timeinfo.tm_hour + (float) timeinfo.tm_min / 60.0;
        if (current_time_hour + time_needed >= (float) _max_time_night - _night_anticipation) {
           println("Heating time");
           _handler->setWaterTankNightPower();
           updatePowerCounter(false);
        }
        else {
           println("No heating : not time");
           _handler->setWaterTankOff();
        }
     }
     else {
        println("No heating : night temp reached");
        _handler->setWaterTankOff();
     }
  }
  else {
    println("No heating : no condition");
    _handler->setWaterTankOff();
  }
  
  if (isTimer) {
    updateLastTime();
     // Until we get an update
     if (isDayTime) {
      _missed_power_update++;
     }
     else {
      _missed_power_update = 0;
     }
  }
}

void PowerManager::onPowerInfo(int32_t pow) {
  _missed_power_update = 0;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    println("Failed to obtain time");
    onFailure();
    return;
  }
  int8_t tm_hour = timeinfo.tm_hour;
  if ((tm_hour >= _min_time_day && tm_hour < _max_time_day) && !_manual_mod) {
     // Increase the total power by the new received power (< 0 if no over power) + current power
     int32_t total_power = ((int32_t) _handler->getWaterTankPower()) + pow;
     _handler->setWaterTankPower(total_power > 0 ? total_power : 0); 
     updatePowerCounter(true);
  }
  else {
    stopPowerListener();  
  }

  updateLastTime();
}

PowerManager::~PowerManager()
{
}