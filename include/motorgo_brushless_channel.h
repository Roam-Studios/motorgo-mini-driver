// Header file for the Brushless Channel class.

#ifndef MOTORGO_6PWM_CHANNEL_H
#define MOTORGO_6PWM_CHANNEL_H

#include <SPI.h>
#include <SimpleFOC.h>
#include <Wire.h>

#include "encoders/calibrated/CalibratedSensor.h"
#include "encoders/mt6701/MagneticSensorMT6701SSI.h"
#include "motorgo_channel.h"
#include "motorgo_common.h"

namespace MotorGo
{

class PWM6Channel : public MotorChannel
{
 public:
  PWM6Channel(PWM6ChannelParameters params, const char* name);
  PWM6Channel(const PWM6Channel&) = delete;  // Delete copy constructor
  PWM6Channel& operator=(const PWM6Channel&) =
      delete;  // Delete copy assignment operator

  void init(ChannelConfiguration channel_config) override;

  void init(ChannelConfiguration channel_config,
            bool should_calibrate) override;

  void loop() override;

 private:
  ChannelParameters params;
};

}  // namespace MotorGo

#endif  // MOTORGO_6PWM_CHANNEL_H