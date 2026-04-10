#include <Arduino.h>
#include "AppRuntime.h"

AppRuntime app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}
