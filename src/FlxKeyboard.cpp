#include "FlxKeyboard.h"
#include "FlxG.h"

FlxKeyboard::FlxKeyboard() {
    for(unsigned int i = 0; i < 1024; i++) {
        lastState[i] = simulate[i] = false;
    }
}

bool FlxKeyboard::down(FlxKey::KeyCode code) {
    return FlxG::backend->isKeyDown(code) || simulate[code];
}

bool FlxKeyboard::pressed(FlxKey::KeyCode code) {
    return down(code) && !lastState[code];
}

bool FlxKeyboard::released(FlxKey::KeyCode code) {
    return !down(code) && lastState[code];
}

void FlxKeyboard::simulateKeyDown(FlxKey::KeyCode code) {
    simulate[code] = true;
}

void FlxKeyboard::simulateKeyUp(FlxKey::KeyCode code) {
    simulate[code] = false;
}

void FlxKeyboard::updateState() {
    bool *state = FlxG::backend->getKeysDown();

    for(unsigned int i = 0; i < 1024; i++) {
        lastState[i] = state[i];
        if(simulate[i]) lastState[i] = true;
    }
}
