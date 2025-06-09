//
// Created by lukas on 31.05.25.
//

#include "DeyeInverter.h"

DeyeInverter::DeyeInverter(uint64_t serial){
    _serial = serial;
    _alarmLogParser.reset(new DeyeAlarmLog());
    _devInfoParser.reset(new DeyeDevInfo());
    _powerCommandParser.reset(new PowerCommandParser());
    _statisticsParser.reset(new DefaultStatisticsParser());

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             static_cast<uint32_t>((serial >> 32) & 0xFFFFFFFF),
             static_cast<uint32_t>(serial & 0xFFFFFFFF));
    _serialString = serial_buff;
}

String DeyeInverter::serialToModel(uint64_t serial) {
    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             ((uint32_t)((serial >> 32) & 0xFFFFFFFF)),
             ((uint32_t)(serial & 0xFFFFFFFF)));
    String serialString = serial_buff;

    if(serialString.startsWith("415") || serialString.startsWith("414") || serialString.startsWith("221")){//TODO find out more ids and check if correct
        return "SUN600G3-EU-230";
    }else if(serialString.startsWith("413") || serialString.startsWith("411")){//TODO find out more ids and check if correct
        return "SUN300G3-EU-230";
    }else if(serialString.startsWith("384") || serialString.startsWith("385") || serialString.startsWith("386")){
        return "SUN-M60/80/100G4-EU-Q0";
    }/*else if(serialString.startsWith("41")){//TODO find out full serial
        return "EPP-300G3-EU-230";
    }*/

    return "Unknown";
}

uint64_t DeyeInverter::serial() const {
    return _serial;
}

bool DeyeInverter::isProducing() {
    auto stats = getStatistics();
    float totalAc = 0;
    for (auto& c : stats->getChannelsByType(TYPE_AC)) {
        if (stats->hasChannelFieldValue(TYPE_AC, c, FLD_PAC)) {
            totalAc += stats->getChannelFieldValue(TYPE_AC, c, FLD_PAC);
        }
    }

    return _enablePolling && totalAc > 0;
}

String DeyeInverter::typeName() const {
    return _devInfoParser->getHwModelName();
}

inverter_type DeyeInverter::getInverterType() const {
    return inverter_type::Inverter_DeyeSun;
}
