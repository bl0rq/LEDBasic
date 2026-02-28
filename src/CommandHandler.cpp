#include "CommandHandler.h"
#include <Arduino.h>
#include <ctype.h>
#include "BasicInterpreter.h"

// Stored callbacks (set via initCommandHandler)
static CommandHandlerCallbacks _callbacks;

void initCommandHandler(const CommandHandlerCallbacks& callbacks) {
  _callbacks = callbacks;
}

void processSerialInput(BasicLEDController* basicControllers[], int numStrips) {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    Serial.print(input);

    int colonPos = input.indexOf(':');
    if (colonPos > 0) {
      int stripIndex = input.substring(0, colonPos).toInt();
      String command = input.substring(colonPos + 1);

      if (stripIndex >= 0 && stripIndex < numStrips) {
        if (command.length() == 1 && isdigit(command[0])) {
          int programIndex = command.toInt();
          if (programIndex >= 0 && programIndex <= 7) {
            if (_callbacks.onLoadProgram) {
              _callbacks.onLoadProgram(stripIndex, programIndex);
            }
          }
        } else if (command.equalsIgnoreCase("p")) {
          if (_callbacks.onDisplayParameters) {
            _callbacks.onDisplayParameters(stripIndex);
          }
        } else if (command.indexOf('=') > 0) {
          int equalPos = command.indexOf('=');
          String paramName = command.substring(0, equalPos);
          String paramValue = command.substring(equalPos + 1);

          paramName.trim();
          paramValue.trim();

          Parameter* param = basicControllers[stripIndex]->getParameter(paramName);
          if (param) {
            if (param->type == PARAM_BOOLEAN) {
              bool value = (paramValue == "true" || paramValue == "1" || paramValue == "on");
              basicControllers[stripIndex]->setParameterValue(paramName, Value(value ? 1.0 : 0.0));
              Serial.print("Set ");
              Serial.print(paramName);
              Serial.print(" = ");
              Serial.println(value ? "true" : "false");
            } else if (param->type == PARAM_NUMBER) {
              double value = paramValue.toDouble();
              basicControllers[stripIndex]->setParameterValue(paramName, Value(value));
              Serial.print("Set ");
              Serial.print(paramName);
              Serial.print(" = ");
              Serial.println(value);
            } else if (param->type == PARAM_ENUM) {
              int index = -1;
              for (int i = 0; i < param->enumValues.size(); i++) {
                if (param->enumValues[i].equalsIgnoreCase(paramValue)) {
                  index = i;
                  break;
                }
              }
              if (index == -1) {
                index = paramValue.toInt();
                if (index < 0 || index >= param->enumValues.size()) {
                  index = -1;
                }
              }

              if (index >= 0) {
                basicControllers[stripIndex]->setParameterValue(paramName, Value((double)index));
                Serial.print("Set ");
                Serial.print(paramName);
                Serial.print(" = ");
                Serial.println(param->enumValues[index]);
              } else {
                Serial.print("Invalid value for ");
                Serial.print(paramName);
                Serial.print(". Valid options: ");
                for (int i = 0; i < param->enumValues.size(); i++) {
                  if (i > 0) Serial.print(", ");
                  Serial.print(param->enumValues[i]);
                }
                Serial.println();
              }
            }
          } else {
            Serial.print("Parameter '");
            Serial.print(paramName);
            Serial.println("' not found. Send 's:p' with p='p' to see available parameters.");
          }
        }
      }
    }

    while (Serial.available()) {
      Serial.read();
    }
  }
}
