#include <Arduino.h>
#include "water_tank_power_handler.h"

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

typedef std::function<void()> OnStartFunction;
typedef std::function<void()> OnStopFunction;
typedef std::function<void(String)> PrintlnFunction;
typedef std::function<void()> OnDayChange;

class PowerManager {
  private:
    WaterTankPowerHandler* _handler;
	int8_t _min_time_day;
	int8_t _max_time_day; 
	int8_t _min_time_night; 
	int8_t _max_time_night;
        float _night_power;
        float _water_volume;
	int8_t _missed_power_update;
	bool _manual_mod;
        float _min_temperature_night;        
        float _night_anticipation;
        float _legionelosis_temperature;   
        int8_t  _legionelosis_max_delay;
        int8_t  _legionelosis_delay;
        bool _legionelosis_activation;
        float _temperature;
        int8_t _inhibitions_days_left;
        float _dayEnergyCounter; // Wh
        float _nightEnergyCounter; // Wh
        float _totalDayEnergyCounter; // kWh
        float _totalNightEnergyCounter; // kWh
        bool _isDayChange;
        int64_t _last_time_us;

	OnStartFunction _onStart;
	OnStopFunction _onStop;	
        PrintlnFunction _println;
	void onFailure();
	void startPowerListener();
	void stopPowerListener();
        void println(String s);

        void updatePowerCounter(bool isDay);
        void updateLastTime();

        float getNightTemperature();
  public:
    PowerManager(WaterTankPowerHandler* handler, int8_t min_time_day, int8_t max_time_day, int8_t min_time_night, int8_t max_time_night, float min_temperature_night, float night_power, float water_volume);
	void onRefresh(bool isTimer = false);
	void onPowerInfo(int32_t pow);
	
        void setManual(bool manual) { _manual_mod = manual; }
	bool getManual() { return _manual_mod; }
        
        bool isPowerReceiptionOk() { return _missed_power_update <= 1; } // 0 or 1 is fine
        int8_t getMissedPowerUpdate() { return _missed_power_update; }

        void setTemperature(float temp);
        float getTemperature() { return _temperature; }

        void setInhibitionsDaysLeft(int8_t d) { _inhibitions_days_left = d; }
        int8_t getInhibitionsDaysLeft() { return _inhibitions_days_left; }
        
        void setMinTemperatureNight(int8_t temp) { _min_temperature_night = (float) temp; }
        int8_t getMinTemperatureNight() { return (int8_t) _min_temperature_night; }

        void setNightAnticipation(float hours) { _night_anticipation = hours; }        
        float getNightAnticipation() { return _night_anticipation; }

        void setLegionelosisActivation(bool active) { _legionelosis_activation = active; }        
        bool getLegionelosisActivation() { return _legionelosis_activation; }

        void setLegionelosisTemperature(int8_t temp) { _legionelosis_temperature = (float) temp; }        
        int8_t getLegionelosisTemperature() { return (int8_t) _legionelosis_temperature; }

        void setLegionelosisMaxDelay(int8_t days) { _legionelosis_max_delay = days; }
        int8_t getLegionelosisMaxDelay() { return _legionelosis_max_delay; }

        void setLegionelosisDelay(int8_t days) { _legionelosis_delay = days; }
        int8_t getLegionelosisDelay() { return _legionelosis_delay; }

        void resetEnergyCounter() {_dayEnergyCounter = 0.0; _nightEnergyCounter = 0.0; _totalDayEnergyCounter = 0.0; _totalNightEnergyCounter = 0.0; }
        float getDayEnergyCounter() { return _dayEnergyCounter; }
        void setDayEnergyCounter(float p) { _dayEnergyCounter = p; }
        float getNightEnergyCounter() { return _nightEnergyCounter; }
        void setNightEnergyCounter(float p) { _nightEnergyCounter = p; }
        float getTotalDayEnergyCounter() { return _totalDayEnergyCounter; }
        void setTotalDayEnergyCounter(float p) { _totalDayEnergyCounter = p; }
        float getTotalNightEnergyCounter() { return _totalNightEnergyCounter; }
        void setTotalNightEnergyCounter(float p) { _totalNightEnergyCounter = p; }

	void setOnStartPowerListener(OnStartFunction cb) { _onStart = cb; }
        void setOnStopPowerListener(OnStopFunction cb) { _onStop = cb; }
        void setPrintFunction(PrintlnFunction cb) { _println = cb; }
        ~PowerManager();
};

#endif