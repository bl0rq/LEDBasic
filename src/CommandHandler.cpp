#include "CommandHandler.h"
#include <Arduino.h>
#include <ctype.h>
#include "BasicInterpreter.h"

static CommandHandlerCallbacks _callbacks;
static String serialBuffer;
static const uint16_t kMaxSerialLine = 128;

void initCommandHandler(const CommandHandlerCallbacks& callbacks) {
  _callbacks = callbacks;
  serialBuffer = "";
}

static void handleCommand(const String& input, BasicLEDController* basicControllers[], int numStrips) {
  Serial.print(input);

  int colonPos = input.indexOf(':');
  if (colonPos <= 0) {
    return;
  }

  int stripIndex = input.substring(0, colonPos).toInt();
  String command = input.substring(colonPos + 1);

  if (stripIndex < 0 || stripIndex >= numStrips) {
    return;
  }
  if (!basicControllers || !basicControllers[stripIndex]) {
    Serial.println("Controller not initialized");
    return;
  }

  if (command.length() == 1 && isdigit((unsigned char)command[0])) {
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
        bool value = (paramValue.equalsIgnoreCase("true") || paramValue == "1" || paramValue.equalsIgnoreCase("on"));
        basicControllers[stripIndex]->setParameterValue(paramName, Value(value ? 1.0f : 0.0f));
        Serial.print("Set ");
        Serial.print(paramName);
        Serial.print(" = ");
        Serial.println(value ? "true" : "false");
      } else if (param->type == PARAM_NUMBER) {
        float value = paramValue.toFloat();
        basicControllers[stripIndex]->setParameterValue(paramName, Value(value));
        Serial.print("Set ");
        Serial.print(paramName);
        Serial.print(" = ");
        Serial.println(value);
      } else if (param->type == PARAM_ENUM) {
        int index = -1;
        for (int i = 0; i < (int)param->enumValues.size(); i++) {
          if (param->enumValues[i].equalsIgnoreCase(paramValue)) {
            index = i;
            break;
          }
        }
        if (index == -1) {
          index = paramValue.toInt();
          if (index < 0 || index >= (int)param->enumValues.size()) {
            index = -1;
          }
        }

        if (index >= 0) {
          basicControllers[stripIndex]->setParameterValue(paramName, Value((float)index));
          Serial.print("Set ");
          Serial.print(paramName);
          Serial.print(" = ");
          Serial.println(param->enumValues[index]);
        } else {
          Serial.print("Invalid value for ");
          Serial.print(paramName);
          Serial.print(". Valid options: ");
          for (int i = 0; i < (int)param->enumValues.size(); i++) {
            if (i > 0) Serial.print(", ");
            Serial.print(param->enumValues[i]);
          }
          Serial.println();
        }
      }
    } else {
      Serial.print("Parameter '");
      Serial.print(paramName);
      Serial.println("' not found. Send 's:p' to see available parameters.");
    }
  }
}

void processSerialInput(BasicLEDController* basicControllers[], int numStrips) {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      serialBuffer.trim();
      if (serialBuffer.length() > 0) {
        handleCommand(serialBuffer, basicControllers, numStrips);
      }
      serialBuffer = "";
    } else if (serialBuffer.length() < kMaxSerialLine) {
      serialBuffer += c;
    }
  }
}
