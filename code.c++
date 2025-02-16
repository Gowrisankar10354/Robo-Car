#include <ESP8266WiFi.h> 
#include <WebSocketsServer.h>//Download from Sketch->Include Libraries->Websockets(by Markus Sattler)
#include<Servo.h>

/* Wi-Fi credentials */
const char* ssid = "Robo Car";
const char* password = "12345678";

/* WebSocket server */
WebSocketsServer webSocket = WebSocketsServer(81);

/* Motor pins */
#define left_motor_forward    D1//-IN 4
#define left_motor_backward   D2//-IN-3
#define right_motor_forward   D3//-IN-2
#define right_motor_backward  D4//-IN-1

/* Ultrasonic sensor pins */
const int trigger_pin = D7;
const int echo_pin = D8;

/* Servo motors */
Servo ultrasonic_servo_motor;
Servo motor;  // Servo motor for the arm

/* IR sensor pins */
const int left_ir_sensor = A0;
const int right_ir_sensor = D0;

/* Command received from WebSocket */
String command = "";
int arm_angle = 90; // Default angle for arm servo

/* Flags */
int manual = 0;
int line = 0;
int obs = 0;
int ges = 0;

/* Function declarations */
void Forward(int speed);
void Backward(int speed);
void Left(int speed);
void Right(int speed);
void Stop();
float measure_distance();
void line_follower_mode();
void obstacle_avoidance_mode();

void setup() {
  Serial.begin(115200);

  /* Setup motor pins */
  pinMode(left_motor_forward, OUTPUT);
  pinMode(left_motor_backward, OUTPUT);
  pinMode(right_motor_forward, OUTPUT);
  pinMode(right_motor_backward, OUTPUT);

  /* Setup ultrasonic sensor pins */
  pinMode(trigger_pin, OUTPUT);
  pinMode(echo_pin, INPUT);

  /* Attach servo motors */
  ultrasonic_servo_motor.attach(D5); // Attach ultrasonic servo motor to D5
  ultrasonic_servo_motor.write(90);  // Set to center position initially

  motor.attach(D6); // Attach the arm servo motor to D6 
  motor.write(arm_angle); // Set the arm servo to default 90 degrees initially

  /* Setup Wi-Fi */
  WiFi.softAP(ssid, password);
  Serial.println("AP IP address: " + WiFi.softAPIP().toString());

  /* Setup WebSocket */
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");
}

void loop() {
  webSocket.loop();
  if (line == 1 && manual == 0 && obs == 0 && ges == 0) {
    line_follower_mode();
  } else if (line == 0 && manual == 0 && obs == 1 && ges == 0) {
    obstacle_avoidance_mode();
  } else if (line == 0 && manual == 0 && obs == 0 && ges == 0) {
    Stop();
  }
}

/*************************************************** ARM CODE ***************************************************/

void arm_up()
{  /* If the arm angle goes below zero, make it zero. */
  if(arm_angle<=0)
  {
    arm_angle = 0;
  }
  else{
  arm_angle += 10;
  Serial.println("Arm Up");
  motor.write(arm_angle);
}
}

void arm_down()
{ /* If the arm angle goes above 180, make it 180. */
   if(arm_angle>=180){
    arm_angle=180;
   }
   else{
  arm_angle -= 10;
  Serial.println("Arm Down");
  motor.write(arm_angle);
}
}


/***************************************** WebSocket Event Handler ******************************************/
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    command = String((char*)payload);
    Serial.println("Command received: " + command);

    if (command == "o") {
      manual = 0;
      line = 0;
      obs = 1;
      ges = 0;
    } else if (command == "l") {
      manual = 0;
      line = 1;
      obs = 0;
      ges = 0;
    } else if (command == "m") {
      manual = 1;
      line = 0;
      obs = 0;
      ges = 0;
      Stop();
    } else if (command == "g") {
      manual = 0;
      line = 0;
      obs = 0;
      ges = 1;
      Stop();
    }else if (command == "ST") {
      manual = 0;
      line = 0;
      obs = 0;
      ges = 0;
      Stop();
    } else if (command == "ST1") {
      Stop();
    } else if (manual == 1) {
      if (command == "Forward") {
        Forward(255);
      } else if (command == "Backward") {
        Backward(255);
      } else if (command == "Left") {
        Left(255);
      } else if (command == "Right") {
        Right(255);
      } else if (command == "U"){
        arm_up();
        Serial.println("up");
      } else if (command == "D"){
        arm_down();
        Serial.println("down");
      }else {
        Stop();
      }
    } else if (ges == 1) {
      if (command == "Forward") {
        Forward(255);
      } else if (command == "Backward") {
        Backward(255);
      } else if (command == "Left") {
        Left(255);
      } else if (command == "Right") {
        Right(255);
      } else {
        Stop();
      }
    }
    else {
      Serial.println("Unknown command");
    }
 }
}


/********************************************* LINE FOLLOWER MODE **********************************************/
    void line_follower_mode() {
      if (analogRead(left_ir_sensor) < 512 && digitalRead(right_ir_sensor) == LOW) {
        Forward(150);
      } else if (analogRead(left_ir_sensor) >= 512 && digitalRead(right_ir_sensor) == LOW) {
        Left(150);
      } else if (analogRead(left_ir_sensor) < 512 && digitalRead(right_ir_sensor) == HIGH) {
        Right(150);
      } else if (analogRead(left_ir_sensor) >= 512 && digitalRead(right_ir_sensor) == HIGH) {
        Stop();
      }
    }

/******************************************* OBSTACLE AVOIDANCE MODE *******************************************/
void obstacle_avoidance_mode() {
  while (obs == 1) { // Run only when the mode is obstacle avoidance
    float distance = measure_distance();

    if (distance <= 30) {
      ultrasonic_servo_motor.write(0); // Turn ultrasonic sensor to the right
      delay(150);
      distance = measure_distance();
      ultrasonic_servo_motor.write(90); // Reset to center
      delay(150);

    if (distance > 30) {
        Right(180);
        delay(275);
      } else if (distance > 5 && distance <= 30) {
        Left(180);
        delay(275);
      } else {
        Backward(180);
        delay(200);
      }
    } else {
      Forward(125);
    }

    // Allow WebSocket to process commands
    webSocket.loop();

    // Check if the mode has changed
    if (obs == 0 || line == 1 || manual == 1) {
      Stop(); // Stop the motors
      break;  // Exit the obstacle avoidance loop
    }
  }
}

/***************************************** Measure Distance *******************************************/
float measure_distance() {
  digitalWrite(trigger_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger_pin, LOW);

  float duration = pulseIn(echo_pin, HIGH);
  float distance = (duration * 0.034) / 2;

  Serial.print("Distance=");
  Serial.println(distance);

  return distance;
}

/* Motor control functions */
void Forward(int speed) {
  analogWriteFreq(1000); // Set PWM frequency
  analogWriteRange(255); // Set PWM range
  analogWrite(left_motor_forward, speed);
  analogWrite(left_motor_backward, 0);
  analogWrite(right_motor_forward, speed);
  analogWrite(right_motor_backward, 0);
}

void Backward(int speed) {
  analogWriteFreq(1000);
  analogWriteRange(255);
  analogWrite(left_motor_forward, 0);
  analogWrite(left_motor_backward, speed);
  analogWrite(right_motor_forward, 0);
  analogWrite(right_motor_backward, speed);
}

void Left(int speed) {
  analogWriteFreq(1000);
  analogWriteRange(255);
  analogWrite(left_motor_forward, 0);
  analogWrite(left_motor_backward, speed);
  analogWrite(right_motor_forward, speed);
  analogWrite(right_motor_backward, 0);
}

void Right(int speed) {
  analogWriteFreq(1000);
  analogWriteRange(255);
  analogWrite(left_motor_forward, speed);
  analogWrite(left_motor_backward, 0);
  analogWrite(right_motor_forward, 0);
  analogWrite(right_motor_backward, speed);
}

void Stop() {
  analogWrite(left_motor_forward, 0);
  analogWrite(left_motor_backward, 0);
  analogWrite(right_motor_forward, 0);
  analogWrite(right_motor_backward, 0);
}
