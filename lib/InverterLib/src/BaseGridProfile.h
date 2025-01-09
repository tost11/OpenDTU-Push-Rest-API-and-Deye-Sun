#pragma once

#ifndef OPENDTU_DEYE_SUN_BASEGRIDPROFILE_H
#define OPENDTU_DEYE_SUN_BASEGRIDPROFILE_H


#include <vector>
#include "Updater.h"

class BaseGridProfile : public Updater {
public:
    virtual std::vector<uint8_t> getRawData() const = 0;
};


#endif //OPENDTU_DEYE_SUN_BASEGRIDPROFILE_H
