#include "motorgo_mini.h"

SPIClass hspi = SPIClass(HSPI);
bool hspi_initialized = false;

Commander command = Commander(Serial);
BLDCMotor motor_ch0 = BLDCMotor(11);
BLDCMotor motor_ch1 = BLDCMotor(11);

void do_target_ch0(char* cmd) { command.motor(&motor_ch0, cmd); }
void do_target_ch1(char* cmd) { command.motor(&motor_ch1, cmd); }

MotorGo::MotorGoMini::MotorGoMini()
    : encoder_ch0(MagneticSensorMT6701SSI(k_ch0_enc_cs)),
      driver_ch0(BLDCDriver6PWM(k_ch0_gpio_uh, k_ch0_gpio_ul, k_ch0_gpio_vh,
                                k_ch0_gpio_vl, k_ch0_gpio_wh, k_ch0_gpio_wl)),
      sensor_calibrated_ch0(CalibratedSensor(encoder_ch0)),
      encoder_ch1(MagneticSensorMT6701SSI(k_ch1_enc_cs)),
      sensor_calibrated_ch1(CalibratedSensor(encoder_ch1)),
      driver_ch1(BLDCDriver6PWM(k_ch1_gpio_uh, k_ch1_gpio_ul, k_ch1_gpio_vh,
                                k_ch1_gpio_vl, k_ch1_gpio_wh, k_ch1_gpio_wl))
{
}

void MotorGo::MotorGoMini::init() { init(false, false); }

void MotorGo::MotorGoMini::init_helper(BLDCMotor& motor, BLDCDriver6PWM& driver,
                                       CalibratedSensor& sensor_calibrated,
                                       MagneticSensorMT6701SSI& encoder,
                                       const char* name)
{
  // Init encoder
  encoder.init(&hspi);

  // Link encoder to motor
  motor.linkSensor(&encoder);

  // Init driver and link to motor
  driver.voltage_power_supply = k_voltage_power_supply;
  driver.voltage_limit = k_voltage_limit;
  driver.init();
  motor.linkDriver(&driver);

  // Set motor control parameters
  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  motor.controller = MotionControlType::torque;
  motor.torque_controller = TorqueControlType::voltage;
  motor.velocity_limit = k_velocity_limit;
  motor.voltage_limit = k_voltage_limit;
  motor.current_limit = k_current_limit;

  // FOCStudio options
  if (enable_foc_studio)
  {
    // use monitoring
    motor.monitor_variables = _MON_TARGET | _MON_VEL | _MON_ANGLE;
    motor.useMonitoring(Serial);
  }

  // Initialize motor
  motor.init();

  // Calibrate encoders
  sensor_calibrated.voltage_calibration = k_voltage_calibration;
  if (should_calibrate)
  {
    sensor_calibrated.calibrate(motor, name);
  }
  // Use calibration data if it exists
  else if (!sensor_calibrated.loadCalibrationData(motor, name))
  {
    // If no data was found, calibrate the sensor
    sensor_calibrated.calibrate(motor, name);
  }

  // Link the calibrated sensor to the motor
  motor.linkSensor(&sensor_calibrated);

  // Init FOC
  motor.initFOC();

  // Print init message
  Serial.print("Initialized ");
  Serial.println(name);
}

void MotorGo::MotorGoMini::init(bool should_calibrate, bool enable_foc_studio)
{
  // Guard to prevent multiple initializations, which could cause a crash
  if (!hspi_initialized)
  {
    hspi_initialized = true;
    hspi.begin(enc_scl, enc_sda, 27, 3);
  }

  this->should_calibrate = should_calibrate;
  this->enable_foc_studio = enable_foc_studio;
  Serial.print("Enable FOC Studio? ");
  Serial.println(enable_foc_studio ? "Yes" : "No");

  // Initialize motors
  init_helper(motor_ch0, driver_ch0, sensor_calibrated_ch0, encoder_ch0, "ch0");
  init_helper(motor_ch1, driver_ch1, sensor_calibrated_ch1, encoder_ch1, "ch1");

  // Set PID parameters for both motors
  motor_ch1.PID_velocity.P = 0.75;
  motor_ch1.PID_velocity.I = 0.09;
  motor_ch1.PID_velocity.D = 0.001;
  motor_ch1.PID_velocity.output_ramp = 10000.0;

  motor_ch0.PID_velocity.P = 0.75;
  motor_ch0.PID_velocity.I = 0.09;
  motor_ch0.PID_velocity.D = 0.001;
  motor_ch0.PID_velocity.output_ramp = 10000.0;

  // add command to commander
  if (enable_foc_studio)
  {
    command.add('0', do_target_ch0, (char*)"target");
    command.add('1', do_target_ch1, (char*)"target");
  }

  motor_ch0.disable();
  motor_ch1.disable();
}

void MotorGo::MotorGoMini::set_target(float target_ch0, float target_ch1)
{
  if (!enable_foc_studio)
  {
    motor_ch0.move(-target_ch0);
    motor_ch1.move(target_ch1);
  }
}

void MotorGo::MotorGoMini::loop()
{
  motor_ch0.loopFOC();
  motor_ch1.loopFOC();

  // this function can be run at much lower frequency than loopFOC()
  motor_ch0.move();
  motor_ch1.move();

  // Monitoring, use only if necessary as it slows loop down significantly
  if (enable_foc_studio)
  {
    // user communication
    command.run();

    motor_ch0.monitor();
    motor_ch1.monitor();
  }
}

void MotorGo::MotorGoMini::enable()
{
  motor_ch0.enable();
  motor_ch1.enable();
}

void MotorGo::MotorGoMini::disable()
{
  motor_ch0.disable();
  motor_ch1.disable();
}

// Getters
float MotorGo::MotorGoMini::get_ch0_position()
{
  return motor_ch0.shaftAngle();
}
float MotorGo::MotorGoMini::get_ch0_velocity()
{
  return motor_ch0.shaftVelocity();
}
float MotorGo::MotorGoMini::get_ch0_voltage() { return motor_ch0.voltage.q; }

float MotorGo::MotorGoMini::get_ch1_position()
{
  return motor_ch1.shaftAngle();
}
float MotorGo::MotorGoMini::get_ch1_velocity()
{
  return motor_ch1.shaftVelocity();
}
float MotorGo::MotorGoMini::get_ch1_voltage() { return motor_ch1.voltage.q; }