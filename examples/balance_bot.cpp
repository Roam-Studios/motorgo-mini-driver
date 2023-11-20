#include <Arduino.h>

#include "Adafruit_AHRS_Madgwick.h"
#include "Adafruit_MPU6050.h"
#include "Wire.h"
#include "common/lowpass_filter.h"
#include "common/pid.h"
#include "configurable.h"
#include "motorgo_mini.h"
#include "pid_manager.h"

MotorGo::MotorGoMini* motorgo_mini;
MotorGo::MotorParameters motor_params_ch0;
MotorGo::MotorParameters motor_params_ch1;

// declare PID manager object
MotorGo::PIDManager pid_manager;

// declare two pre-built PID controller objects for individual motor velocity
// control
// MotorGo::PIDParameters velocity_pid_params;

// MotorGo::PIDParameters velocity_controller_params;
MotorGo::PIDParameters velocity_controller_params;
LowPassFilter velocity_lpf(velocity_controller_params.lpf_time_constant);
PIDController velocity_controller(velocity_controller_params.p,
                                  velocity_controller_params.i,
                                  velocity_controller_params.d,
                                  velocity_controller_params.output_ramp,
                                  velocity_controller_params.limit);

// declare and configure custom balance controller object
MotorGo::PIDParameters balancing_controller_params;
LowPassFilter balancing_lpf(balancing_controller_params.lpf_time_constant);
PIDController balancing_controller(balancing_controller_params.p,
                                   balancing_controller_params.i,
                                   balancing_controller_params.d,
                                   balancing_controller_params.output_ramp,
                                   balancing_controller_params.limit);

// declare and configure custom steering controller object
MotorGo::PIDParameters steering_controller_params;
LowPassFilter steering_lpf(steering_controller_params.lpf_time_constant);
PIDController steering_controller(steering_controller_params.p,
                                  steering_controller_params.i,
                                  steering_controller_params.d,
                                  steering_controller_params.output_ramp,
                                  steering_controller_params.limit);

bool motors_enabled = false;
ESPWifiConfig::Configurable<bool> enable_motors(motors_enabled, "/enable",
                                                "Enable motors");

Adafruit_MPU6050 mpu;
Adafruit_Madgwick filter(0.8);
float pitch_zero = 0.0092;
float initial_angle_ch0;
float initial_angle_ch1;

void enable_motors_callback(bool value)
{
  if (value)
  {
    Serial.println("Enabling motors");
    motorgo_mini->enable_ch0();
    motorgo_mini->enable_ch1();
  }
  else
  {
    Serial.println("Disabling motors");
    motorgo_mini->disable_ch0();
    motorgo_mini->disable_ch1();
  }
}

// Function to print at a maximum frequency
void freq_println(String str, int freq)
{
  static unsigned long last_print_time = 0;
  unsigned long now = millis();

  if (now - last_print_time > 1000 / freq)
  {
    Serial.println(str);
    last_print_time = now;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(3000);

  //   Configure Wire on pins 38, 47
  Wire1.begin(38, 47);

  //   Begin IMU on Wire
  if (!mpu.begin(0x68, &Wire1))
  {
    Serial.println("Failed to find MPU6050 chip");
    while (1)
    {
      delay(10);
    }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);
  mpu.setHighPassFilter(MPU6050_HIGHPASS_DISABLE);

  filter.begin(300);

  // Setup motor parameters
  motor_params_ch0.pole_pairs = 7;
  motor_params_ch0.power_supply_voltage = 5.0;
  motor_params_ch0.voltage_limit = 5.0;
  motor_params_ch0.current_limit = 300;
  motor_params_ch0.velocity_limit = 100.0;
  motor_params_ch0.calibration_voltage = 2.0;

  motor_params_ch1.pole_pairs = 7;
  motor_params_ch1.power_supply_voltage = 5.0;
  motor_params_ch1.voltage_limit = 5.0;
  motor_params_ch1.current_limit = 300;
  motor_params_ch1.velocity_limit = 100.0;
  motor_params_ch1.calibration_voltage = 2.0;

  // Instantiate motorgo mini board
  motorgo_mini = new MotorGo::MotorGoMini();

  // Setup Ch0 with FOCStudio enabled
  bool calibrate = false;
  bool enable_foc_studio = false;
  motorgo_mini->init_ch0(motor_params_ch0, calibrate, enable_foc_studio);
  motorgo_mini->init_ch1(motor_params_ch1, calibrate, enable_foc_studio);

  // Set velocity controller parameters
  // Setup PID parameters - velocity

  float vel_p = 3.0;
  float vel_i = 0.3;
  float vel_d = 0.0;

  //   velocity_pid_params.p = vel_p;
  //   velocity_pid_params.i = vel_i;
  //   velocity_pid_params.d = vel_d;
  //   velocity_pid_params.output_ramp = 10000.0;
  //   velocity_pid_params.lpf_time_constant = 0.11;

  // Instantiate controllers
  //   motorgo_mini->set_velocity_controller_ch0(velocity_pid_params);
  //   motorgo_mini->set_velocity_controller_ch1(velocity_pid_params);

  //   Set closed-loop position mode
  motorgo_mini->set_control_mode_ch0(MotorGo::ControlMode::Voltage);
  motorgo_mini->set_control_mode_ch1(MotorGo::ControlMode::Voltage);

  // wrap controller params into a configurable object, pass anonymous function
  // to allow board to update controller values after receiving input over wifi.
  pid_manager.add_controller(
      "/velocity", velocity_controller_params,
      []()
      {
        velocity_controller.P = velocity_controller_params.p;
        velocity_controller.I = velocity_controller_params.i;
        velocity_controller.D = velocity_controller_params.d;
        velocity_controller.output_ramp =
            velocity_controller_params.output_ramp;
        velocity_controller.limit = velocity_controller_params.limit;
        velocity_lpf.Tf = velocity_controller_params.lpf_time_constant;
        velocity_controller.reset();
      });

  // wrap controller params into a configurable object, pass anonymous function
  // to allow board to update controller values after receiving input over wifi.
  pid_manager.add_controller(
      "/balancing", balancing_controller_params,
      []()
      {
        balancing_controller.P = balancing_controller_params.p;
        balancing_controller.I = balancing_controller_params.i;
        balancing_controller.D = balancing_controller_params.d;
        balancing_controller.output_ramp =
            balancing_controller_params.output_ramp;
        balancing_controller.limit = balancing_controller_params.limit;
        balancing_lpf.Tf = balancing_controller_params.lpf_time_constant;
        balancing_controller.reset();
      });

  // wrap controller params into a configurable object, pass anonymous function
  // to allow board to update controller values after receiving input over wifi.
  pid_manager.add_controller(
      "/steering", steering_controller_params,
      []()
      {
        if (motors_enabled)
        {
          motorgo_mini->disable_ch0();
          motorgo_mini->disable_ch1();
        }

        // Compute new initial angle
        initial_angle_ch0 = motorgo_mini->get_ch0_position();
        initial_angle_ch1 = motorgo_mini->get_ch1_position();

        steering_controller.P = steering_controller_params.p;
        steering_controller.I = steering_controller_params.i;
        steering_controller.D = steering_controller_params.d;
        steering_controller.output_ramp =
            steering_controller_params.output_ramp;
        steering_controller.limit = steering_controller_params.limit;
        steering_lpf.Tf = steering_controller_params.lpf_time_constant;
        steering_controller.reset();

        if (motors_enabled)
        {
          motorgo_mini->enable_ch0();
          motorgo_mini->enable_ch1();
        }
      });

  enable_motors.set_post_callback(enable_motors_callback);

  // initialize the PID manager
  pid_manager.init();

  // Run the filter for 1000 steps to get a good initial estimate of the yaw
  for (int i = 0; i < 1000; i++)
  {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    filter.updateIMU(g.gyro.x, g.gyro.y, g.gyro.z, a.acceleration.x,
                     a.acceleration.y, a.acceleration.z);
  }

  initial_angle_ch0 = motorgo_mini->get_ch0_position();
  initial_angle_ch1 = motorgo_mini->get_ch1_position();

  // enable controllers and prepare for the loop
  //   motorgo_mini->enable_ch0();
  //   motorgo_mini->enable_ch1();
}

void loop()
{
  // Every 2 seconds, switch between 0 rad/s and 10 rad/s
  static unsigned long last_time = 0;
  unsigned long now = millis();

  // Run Ch0
  motorgo_mini->loop_ch0();
  motorgo_mini->loop_ch1();

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  filter.updateIMU(g.gyro.x, g.gyro.y, g.gyro.z, a.acceleration.x,
                   a.acceleration.y, a.acceleration.z);

  float pitch = filter.getRollRadians();
  // Print pitch using frequency print
  freq_println("Pitch: " + String(pitch, 5), 10);

  float wheel_velocity =
      (motorgo_mini->get_ch0_velocity() - motorgo_mini->get_ch1_velocity()) / 2;

  float ch0_pos = motorgo_mini->get_ch0_position() - initial_angle_ch0;
  float ch1_pos = -(motorgo_mini->get_ch1_position() - initial_angle_ch1);

  float velocity_command = velocity_controller(wheel_velocity);
  float balance_command = balancing_controller(pitch - pitch_zero);
  float steering_command = steering_controller(ch0_pos - ch1_pos);

  // Print balance_command
  freq_println("Balancing error: " + String(pitch - pitch_zero, 5), 10);

  float command_ch0 = balance_command - steering_command - velocity_command;
  float command_ch1 = balance_command + steering_command - velocity_command;

  motorgo_mini->set_target_voltage_ch0(command_ch0);
  motorgo_mini->set_target_voltage_ch1(command_ch1);
}