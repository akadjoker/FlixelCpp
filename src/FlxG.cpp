#include "FlxG.h"
#include "backend/BackendHolder.h"

FlxState *FlxG::state;
float FlxG::elapsed;
bool FlxG::exitMessage = false;
FlxBackendBase *FlxG::backend;
int FlxG::bgColor = 0xff000000;
FlxKeyboard *FlxG::key;
int FlxG::width, FlxG::height;
int FlxG::screenWidth, FlxG::screenHeight;
FlxPreloader *FlxG::preloader = NULL;
FlxMusic *FlxG::music = 0;
FlxRect FlxG::worldBounds;
FlxVector FlxG::scroolVector;
FlxObject *FlxG::toFollow = NULL;
std::vector<FlxMouse*> FlxG::mouse;
float FlxG::fps = 0;
float FlxG::fpsCounter = 0;
float FlxG::fixedTime = 0.01f;
FlxSprite FlxG::flashSprite;
float FlxG::flashCounter = 0, FlxG::flashMaxTime = 0;
bool FlxG::flashing = false;
FlxVector FlxG::screenScaleVector;
bool FlxG::scaleToScreen = false;

// quick help function
static int powerOf2(int input) {
    int value = 1;

    while (value < input) {
        value <<= 1;
    }

    return value;
}


/*
*  Main functions definitions starts here
*/
int FlxG::setup(const char *title, int Width, int Height, FlxState *state) {

    if(!backend) {
        std::cerr << "[Error] Backend is null" << std::endl;
        return 1;
    }

    width = Width;
    height = Height;

    BackendHolder::get().setBackend(backend);
    BackendHolder::get().getBackend()->setupSurface(title, width, height);

    FlxVector screenSize = BackendHolder::get().getBackend()->getScreenSize();
	screenWidth = screenSize.x;
	screenHeight = screenSize.y;
    screenScaleVector.x = screenWidth / (float)width;
    screenScaleVector.y = screenHeight / (float)height;

    key = new FlxKeyboard();
    srand(time(0));

    // flash screen sprite (texture size should be pow of 2)
    flashSprite.makeGraphic(powerOf2(width), powerOf2(height), 0xffffff);
    flashSprite.width = width;
    flashSprite.height = height;
    flashSprite.alpha = 0;

    // run preloader
    if(preloader) {
        preloader->create();
        BackendHolder::get().getBackend()->mainLoop(FlxPreloader::onUpdate, FlxPreloader::onDraw);

        delete preloader;
        FlxG::exitMessage = false;
    }

    switchState(state);

    // main loop
    BackendHolder::get().getBackend()->mainLoop(innerUpdate, innerDraw);


    delete key;

    BackendHolder::get().getBackend()->exitApplication();
    return 0;
}

void FlxG::followObject(FlxObject *object) {
    toFollow = object;
}


void FlxG::switchState(FlxState *newState) {
    if(!newState) return;
    if(state) delete state;

    state = newState;
    state->create();
}


FlxSound FlxG::play(const char *path, float vol) {
    FlxSound *s = new FlxSound(path);
    s->play(vol);
    return *s;
}


FlxMusic* FlxG::playMusic(const char *path, float vol) {
    FlxMusic *s = new FlxMusic(path);
    s->play(vol);

    music = s;
    return s;
}


void FlxG::updateMouses() {

    for(unsigned int i = 0; i < mouse.size(); i++) {

        #ifdef FLX_MOBILE
        if(mouse[i]->leftReleased) {
            mouse.erase(mouse.begin() + i);
            continue;
        }

        if(mouse[i]->leftPressed) {
            mouse[i]->leftPressed = false;
        }
        #endif

        mouse[i]->updateState();
    }
}


void FlxG::innerUpdate() {

    // sounds and music garbage collector
    for(unsigned int i = 0; i < FlxSound::Sounds.size(); i++) {
        if(FlxSound::Sounds[i]) {
            if(!FlxSound::Sounds[i]->isPlaying() && !FlxSound::Sounds[i]->isPaused()) {
                delete FlxSound::Sounds[i];
                FlxSound::Sounds.erase(FlxSound::Sounds.begin() + i);
            }
        }
        else {
            FlxSound::Sounds.erase(FlxSound::Sounds.begin() + i);
        }
    }

    for(unsigned int i = 0; i < FlxMusic::Music.size(); i++) {
        if(FlxMusic::Music[i]) {
            if(!FlxMusic::Music[i]->isPlaying() && !FlxMusic::Music[i]->isPaused()) {
                delete FlxMusic::Music[i];
                FlxMusic::Music.erase(FlxMusic::Music.begin() + i);
            }
        }
        else {
            FlxMusic::Music.erase(FlxMusic::Music.begin() + i);
        }
    }

    // update fps
    fpsCounter += elapsed;
    if(fpsCounter >= 1.f) {
        fps = 1.f / elapsed;
        fpsCounter = 0;
    }

    // update flashing screen stuff
    flashSprite.x = scroolVector.x;
    flashSprite.y = scroolVector.y;

    if(flashing) {
        flashCounter += fixedTime;

        if(flashCounter >= flashMaxTime) {
            flashing = false;
            flashCounter = flashMaxTime = 0;

            flashSprite.alpha = 0;
            flashSprite.color = 0xffffff;
        }
        else {
            flashSprite.alpha = flashCounter / flashMaxTime;
        }
    }

    // follow some object?
    if(toFollow) {
        FlxVector objectCenter = toFollow->getCenter();
        FlxVector move(objectCenter.x - (width / 2), objectCenter.y - (height / 2));

        if(move.x < worldBounds.x) move.x = worldBounds.x;
        if(move.x + width > worldBounds.width) move.x = worldBounds.width - width;

        if(move.y < worldBounds.y) move.y = worldBounds.y;
        if(move.y + height > worldBounds.height) move.y = worldBounds.height - height;

        scroolVector.x = -move.x;
        scroolVector.y = -move.y;
    }
    else {
        scroolVector.x = scroolVector.y = 0;
    }


    if(state) state->update();

    // update input devices
    updateMouses();
    key->updateState();

    // handle flashing sprite
    if(flashing) flashSprite.update();
}


void FlxG::innerDraw() {
    if(state) state->draw();

    // handle flashing sprite
    if(flashing) flashSprite.draw();
}


void FlxG::flash(int color, float time) {
    if(flashing) return;
    flashing = true;

    flashMaxTime = time;
    flashSprite.color = color;
}