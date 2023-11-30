#include "input.h"
#include <cstring>

Input *Input::Instance = nullptr;

void Input::Update() {
    activeFrame = (activeFrame + 1) % FRAME_COUNT;
    memcpy(keyStates[activeFrame],
           keyStates[Wrap(activeFrame - 1, FRAME_COUNT)],
           KEY_MAX_STATES * sizeof(Input::KeyState));
}

void Input::UpdateKey(unsigned int keyCode, bool keyState) {
    keyStates[activeFrame][keyCode].isDown = keyState;
}

bool Input::KeyPressed(unsigned int key) {
    return (keyStates[Wrap(activeFrame, FRAME_COUNT)][key].isDown &&
            !keyStates[Wrap(activeFrame - FRAME_PREVIOUS, FRAME_COUNT)][key].isDown);
}

bool Input::KeyDown(unsigned int key) {
    return keyStates[Wrap(activeFrame, FRAME_COUNT)][key].isDown;
}

bool Input::KeyReleased(unsigned int key) {
    return (!keyStates[Wrap(activeFrame, FRAME_COUNT)][key].isDown &&
            keyStates[Wrap(activeFrame - FRAME_PREVIOUS, FRAME_COUNT)][key].isDown);
}