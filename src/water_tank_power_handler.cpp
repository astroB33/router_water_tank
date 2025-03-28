#include <Arduino.h>
#include "water_tank_power_handler.h"

WaterTankPowerHandler::WaterTankPowerHandler(uint8_t pinRelay1, uint8_t pinRelay2, uint8_t pinRelayDimmerFull, uint8_t pinRelayDimmerDim, uint8_t pinDimmer, uint8_t zeroCrossing, uint32_t powerPerRelay)
: _currentPower(0), _minDimPower(0),
  _pinRelay1(pinRelay1), _pinRelay2(pinRelay2), _pinRelayDimmerFull(pinRelayDimmerFull), _pinRelayDimmerDim(pinRelayDimmerDim), _pinDimmer(pinDimmer), _zeroCrossing(zeroCrossing), _dimmer(pinDimmer, zeroCrossing), _powerPerRelay(powerPerRelay)
{
  
}

void WaterTankPowerHandler::setWaterTankPower(uint32_t pow) {

  _currentPower = 0;

  bool pinRelay1On = false;
  bool pinRelay2On = false;
  bool pinRelayDimmerFullOn = false;
  bool pinRelayDimmerDimOn = false;

  if (pow >= _powerPerRelay) {
    pinRelay1On = true;
    _currentPower += _powerPerRelay;
    pow -= _powerPerRelay;
  }
  
  if (pow >= _powerPerRelay) {
    pinRelay2On = true;
    _currentPower += _powerPerRelay;
    pow -= _powerPerRelay;
  }

  if (pow >= _powerPerRelay) {
    pinRelayDimmerFullOn = true;
    _dimmer.setPower(0);
    _currentPower += _powerPerRelay;
    pow -= _powerPerRelay;
  }
  else if (pow >= _minDimPower) {    
     pinRelayDimmerDimOn = true;
    _dimmer.setPower(100 * pow / _powerPerRelay);
    _currentPower += pow;
    pow = 0;
  }
  else {
    _dimmer.setPower(0);
  }
 
  // Relay are ON on state low
  digitalWrite(_pinRelay1, !pinRelay1On);
  digitalWrite(_pinRelay2, !pinRelay2On);
  digitalWrite(_pinRelayDimmerFull, !pinRelayDimmerFullOn);
  digitalWrite(_pinRelayDimmerDim, !pinRelayDimmerDimOn);
}

void WaterTankPowerHandler::setWaterTankNightPower() {
  setWaterTankPower(_powerPerRelay * 4);
}

void WaterTankPowerHandler::setWaterTankOff() {
  setWaterTankPower(0);
}

void WaterTankPowerHandler::begin() {
  _dimmer.begin(NORMAL_MODE, ON);
  pinMode(_pinRelay1, OUTPUT);
  pinMode(_pinRelay2, OUTPUT);
  pinMode(_pinRelayDimmerFull, OUTPUT);
  pinMode(_pinRelayDimmerDim, OUTPUT);
  pinMode(_pinDimmer, OUTPUT);
  pinMode(_zeroCrossing, INPUT);
  setWaterTankOff();
}

uint32_t WaterTankPowerHandler::getWaterTankPower() {
  return _currentPower;
}

WaterTankPowerHandler::~WaterTankPowerHandler()
{
}