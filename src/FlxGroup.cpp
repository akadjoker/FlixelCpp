#include "FlxGroup.h"
#include "FlxObject.h"

FlxGroup::FlxGroup() {
    entityType = FLX_GROUP;
}


FlxBasic* FlxGroup::add(FlxBasic *object) {
    if(object)
        members.push_back(object);

    return object;
}


bool FlxGroup::remove(FlxBasic *object) {

    if(!object) return false;

    for(unsigned int i = 0; i < members.size(); i++) {
        if(members[i] == object) {
            delete members[i];
            members.erase(members.begin() + i);
            return true;
        }
    }

    return false;
}


void FlxGroup::clear() {
    for(unsigned int i = 0; i < members.size(); i++) {
        delete members[i];
        members.erase(members.begin());
    }
}


void FlxGroup::update() {

    for(unsigned int i = 0; i < members.size(); i++) {
        if(members[i]) members[i]->update();
    }
}


void FlxGroup::draw() {

    for(unsigned int i = 0; i < members.size(); i++) {
        if(members[i]) members[i]->draw();
    }
}


FlxBasic* FlxGroup::overlaps(FlxBasic *object) {

    if(object->entityType == FLX_OBJECT) {
        return object->overlaps(this);
    }
    else if(object->entityType == FLX_GROUP) {
        FlxGroup *group = (FlxGroup*) object;

        for(unsigned int i = 0; i < group->members.size(); i++) {
            if(overlaps(group->members[i])) return group->members[i];
        }
    }

    return NULL;
}

FlxBasic* FlxGroup::collide(FlxBasic *object) {

    if(object->entityType == FLX_OBJECT) {
        FlxBasic *ent = NULL;

        for(int i = members.size() - 1; i >= 0; i--) {
            FlxObject *obj = (FlxObject*)members[i];
            ent = obj->collide(object);
        }

        return ent;
    }
    else if(object->entityType == FLX_GROUP) {
        FlxGroup *group = (FlxGroup*)object;
        FlxBasic *ent = NULL;

        for(int i = members.size() - 1; i >= 0; i--) {
            for(int j = group->members.size() - 1; j >= 0; j--) {
                ent = members[i]->collide(group->members[j]);
            }
        }

        return ent;
    }

    return NULL;
}
