#include <Arduino.h>
#include <RBDdimmer.h>

#ifndef WATER_TANK_POWER_HANDLER_H
#define WATER_TANK_POWER_HANDLER_H

class WaterTankPowerHandler {
  private:
    uint32_t _currentPower;
    uint8_t _pinRelay1;
    uint8_t _pinRelay2;
    uint8_t _pinRelayDimmerFull;
    uint8_t _pinRelayDimmerDim;
    uint8_t _pinDimmer;
    uint8_t _zeroCrossing;
    uint32_t _powerPerRelay;
    uint32_t _minDimPower;
    dimmerLamp _dimmer;
  public:
    WaterTankPowerHandler(uint8_t pinRelay1, uint8_t pinRelay2, uint8_t pinRelayDimmerFull, uint8_t pinRelayDimmerDim, uint8_t pinDimmer, uint8_t zeroCrossing, uint32_t powerPerRelay);
    ~WaterTankPowerHandler();

    void setWaterTankPower(uint32_t pow);

    void setWaterTankNightPower();

    void setWaterTankOff();

    void setMinDimPower(uint32_t pow) { _minDimPower = pow; }

    void begin();

    uint32_t getWaterTankPower();
};

#endif