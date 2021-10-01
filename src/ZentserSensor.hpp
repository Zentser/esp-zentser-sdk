#pragma once

#include <Arduino.h>

/*!
 *  @brief  Class that stores state for interacting with Zentser system
 *  @param  id
 *          sensor's id has given by the server. Required.
 *  @param  name
 *          sensor's name. Optional parameter for readability
 *  @param  float
 * 			    value returned by a sensor. Required
 */
struct Sensor {

  Sensor(String id, String name = "Sensor") {
    this->id = id;
    this->name = name;
  }

  String id;
  String name;
  float value;
  float alarmMinLimit = NAN;
  float alarmMaxLimit = NAN;

  bool isOutOfLimits() {
    if (isnan(alarmMinLimit) && isnan(alarmMaxLimit)) {
      return false;
    }

    if ((value < alarmMinLimit) || (value > alarmMaxLimit)) {
      return true;
    }

    return false;
  }
};