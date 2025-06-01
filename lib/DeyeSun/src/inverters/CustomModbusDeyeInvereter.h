//
// Created by lukas on 31.05.25.
//

#pragma once

#include "DeyeInverter.h"

class CustomModbusDeyeInvereter : public DeyeInverter {

    static const uint16_t SEND_REQUEST_BUFFER_LENGTH = 300;


private:
    WiFiClient _client;

    //TODO implement long timer after some tries
    TimeoutHelper _reconnectTimeout;

    char _requestDataCommand[SEND_REQUEST_BUFFER_LENGTH];//TODO check how many caracters needed
    void createReqeustDataCommand();
public:
    explicit CustomModbusDeyeInvereter(uint64_t serial);

    deye_inverter_type getDeyeInverterType() const override;

    void update() override;

protected:
    void hostOrPortUpdated() override;
};
