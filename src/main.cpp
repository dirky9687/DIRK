#include <Arduino.h>
#include "core/DirkApp.h"

dirk::DirkApp app;

void setup() { app.begin(); }
void loop()  { app.update(); }
