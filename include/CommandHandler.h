#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <functional>

// Handles serial input and dispatches commands to the LED controllers.

class BasicLEDController;

// Callback types for decoupling from application-specific logic
struct CommandHandlerCallbacks {
    std::function<void(int stripIndex, int programIndex)> onLoadProgram;
    std::function<void(int stripIndex)> onDisplayParameters;
};

// Initialize command handler with application-specific callbacks.
// Must be called before processSerialInput().
void initCommandHandler(const CommandHandlerCallbacks& callbacks);

// Process serial input, dispatching commands to the given controllers.
void processSerialInput(BasicLEDController* controllers[], int numStrips);

#endif // COMMAND_HANDLER_H
