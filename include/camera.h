#ifndef CAMERA_H
#define CAMERA_H

#include "glad/glad.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Defines several possible options for camera movement. Used as abstraction to
// stay away from window-system specific input methods
enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

// An abstract camera class that processes input and calculates the
// corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera {
public:
  // Default camera values
  static constexpr float DEFAULT_YAW = -90.0f;
  static constexpr float DEFAULT_PITCH = 0.0f;
  static constexpr float DEFAULT_MIN_SPEED = 10.0f;
  static constexpr float DEFAULT_MAX_SPEED = 200.0f;
  static constexpr float DEFAULT_ACCEL_TIME = 2.0f;
  static constexpr float DEFAULT_SENSITIVITY = 0.1f;
  static constexpr float DEFAULT_ZOOM = 45.0f;
  static constexpr float DEFAULT_ASPECT_RATIO = 16.0f / 9.0f;
  static constexpr float DEFAULT_NEAR_PLANE = 0.1f;
  static constexpr float DEFAULT_FAR_PLANE = 100000.0f;

  // camera attributes
  glm::vec3 position;
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 worldUp;
  // euler angles
  float yaw;
  float pitch;
  // camera options
  float currentSpeed;
  float minSpeed;
  float maxSpeed;
  float accelTime;
  float mouseSensitivity;
  float zoom;
  float aspectRatio;
  float nearPlane;
  float farPlane;

  // constructor with vectors
  Camera(glm::vec3 pos = glm::vec3(0.0f, 0.0f, 5.0f),
         glm::vec3 worldUpVec = glm::vec3(0.0f, 1.0f, 0.0f),
         float yawAngle = DEFAULT_YAW, float pitchAngle = DEFAULT_PITCH)
      : front(glm::vec3(0.0f, 0.0f, -1.0f)), currentSpeed(DEFAULT_MIN_SPEED),
        minSpeed(DEFAULT_MIN_SPEED), maxSpeed(DEFAULT_MAX_SPEED),
        accelTime(DEFAULT_ACCEL_TIME), mouseSensitivity(DEFAULT_SENSITIVITY),
        zoom(DEFAULT_ZOOM), aspectRatio(DEFAULT_ASPECT_RATIO),
        nearPlane(DEFAULT_NEAR_PLANE), farPlane(DEFAULT_FAR_PLANE) {
    position = pos;
    worldUp = worldUpVec;
    yaw = yawAngle;
    pitch = pitchAngle;
    updateCameraVectors();
  }

  // constructor with scalar values
  Camera(float posX, float posY, float posZ, float upX, float upY, float upZ,
         float yawAngle, float pitchAngle)
      : front(glm::vec3(0.0f, 0.0f, -1.0f)), currentSpeed(DEFAULT_MIN_SPEED),
        minSpeed(DEFAULT_MIN_SPEED), maxSpeed(DEFAULT_MAX_SPEED),
        accelTime(DEFAULT_ACCEL_TIME), mouseSensitivity(DEFAULT_SENSITIVITY),
        zoom(DEFAULT_ZOOM), aspectRatio(DEFAULT_ASPECT_RATIO),
        nearPlane(DEFAULT_NEAR_PLANE), farPlane(DEFAULT_FAR_PLANE) {
    position = glm::vec3(posX, posY, posZ);
    worldUp = glm::vec3(upX, upY, upZ);
    yaw = yawAngle;
    pitch = pitchAngle;
    updateCameraVectors();
  }

  // returns the view matrix calculated using Euler Angles and the LookAt Matrix
  glm::mat4 GetViewMatrix() {
    return glm::lookAt(position, position + front, up);
  }

  glm::mat4 GetProjectionMatrix() {
    return glm::perspective(glm::radians(zoom), aspectRatio, nearPlane,
                            farPlane);
  }

  void UpdateSpeed(bool isMoving, float deltaTime) {
    if (isMoving) {
      float accel = maxSpeed / accelTime;
      currentSpeed += accel * deltaTime;
      if (currentSpeed > maxSpeed) {
        currentSpeed = maxSpeed;
      }
    } else {
      currentSpeed = minSpeed;
    }
  }

  // processes input received from any keyboard-like input system. Accepts input
  // parameter in the form of camera defined ENUM (to abstract it from windowing
  // systems)
  void ProcessKeyboard(Camera_Movement direction, float deltaTime) {
    float velocity = currentSpeed * deltaTime;
    if (direction == FORWARD)
      position += front * velocity;
    if (direction == BACKWARD)
      position -= front * velocity;
    if (direction == LEFT)
      position -= right * velocity;
    if (direction == RIGHT)
      position += right * velocity;
    if (direction == UP)
      position += glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
    if (direction == DOWN)
      position -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
  }

  // processes input received from a mouse input system. Expects the offset
  // value in both the x and y direction.
  void ProcessMouseMovement(float xoffset, float yoffset,
                            GLboolean constrainPitch = true) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch) {
      if (pitch > 89.0f)
        pitch = 89.0f;
      if (pitch < -89.0f)
        pitch = -89.0f;
    }

    // update front, right and up vectors using the updated Euler angles
    updateCameraVectors();
  }

  // processes input received from a mouse scroll-wheel event. Only requires
  // input on the vertical wheel-axis
  void ProcessMouseScroll(float yoffset) {
    zoom -= (float)yoffset;
    if (zoom < 1.0f)
      zoom = 1.0f;
    if (zoom > 45.0f)
      zoom = 45.0f;
  }

private:
  // calculates the front vector from the Camera's (updated) Euler Angles
  void updateCameraVectors() {
    glm::vec3 f;
    f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    f.y = sin(glm::radians(pitch));
    f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(f);
    // also re-calculate the right and up vectors
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
  }
};
#endif
