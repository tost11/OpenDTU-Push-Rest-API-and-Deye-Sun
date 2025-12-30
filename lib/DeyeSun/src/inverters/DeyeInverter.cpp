//
// Created by lukas on 31.05.25.
//

#include "DeyeInverter.h"
#include "DeyeSun.h"
#include "DeyeUtils.h"
#include <RestRequestHandler.h>
#include "utils/Base64.h"

DeyeInverter::DeyeInverter(uint64_t serial){
    _serial = serial;
    _alarmLogParser.reset(new DefaultAlarmLog("DeyeSun"));
    _devInfoParser.reset(new DeyeDevInfo());
    _powerCommandParser.reset(new PowerCommandParser());
    _statisticsParser.reset(new DefaultStatisticsParser());

    char serial_buff[sizeof(uint64_t) * 8 + 1];
    snprintf(serial_buff, sizeof(serial_buff), "%0x%08x",
             static_cast<uint32_t>((serial >> 32) & 0xFFFFFFFF),
             static_cast<uint32_t>(serial & 0xFFFFFFFF));
    _serialString = serial_buff;

    // TimeoutHelper initializes itself - starts at 0 (expired), so first fetch happens immediately
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

void DeyeInverter::handleDeyeDayCorrection() {
    //set new deye offline offset if configured
    for (auto& type : _statisticsParser->getChannelTypes()) {
        for (auto &channel: _statisticsParser->getChannelsByType(type)) {
            if(!_statisticsParser->hasChannelFieldValue(type,channel,FLD_YD)){
                continue;
            }
            _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, 0, 2);
            if(_statisticsParser->getDeyeSunOfflineYieldDayCorrection()) {
                float currentOffset = _statisticsParser->getChannelFieldOffset(type, channel, FLD_YD,1);
                ESP_LOGD(LogTag().c_str(),"Current Deye Offline offset: %f",currentOffset);
                if (!(currentOffset > 0.f || currentOffset < 0.f)) {
                    float val = _statisticsParser->getChannelFieldValue(type, channel, FLD_YD);
                    _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, val * -1.f,1);
                    float checkOffset = _statisticsParser->getChannelFieldOffset(type, channel, FLD_YD,1);
                    ESP_LOGD(LogTag().c_str(),"Set daily production offset for type: %d and channel: %d to:%f\n",(int)type,(int)channel,checkOffset);
                }
            }else{
                //MessageOutput.println("nope");
                if(_statisticsParser->getSettingByChannelField(type,channel,FLD_YD,1) != nullptr){
                    _statisticsParser->setChannelFieldOffset(type, channel, FLD_YD, 0, 1);
                }
            }
        }
    }
}

bool DeyeInverter::sendActivePowerControlRequest(float limit, PowerLimitControlType type) {
    if(typeName().startsWith("Unknown") && !DeyeSun.getUnknownDevicesWriteEnable()){
        _alarmLogParser->addAlarm(6,10 * 60,"limit command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");//alarm for 10 min
        ESP_LOGI(LogTag().c_str(),"limit command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");
        return false;
    }
    if(!(type == AbsolutPersistent || type == RelativPersistent)){
        ESP_LOGI(LogTag().c_str(),"Setting of temporary limit on deye inverter not possible");
        return false;
    }

    uint16_t realLimit;
    if(type == RelativPersistent){
        realLimit = (uint16_t)(limit + 0.5f);
    }else{
        uint16_t maxPower = _devInfoParser->getMaxPower();
        if(maxPower == 0){
            _alarmLogParser->addAlarm(6,10 * 60,"limit command not send because init data of device not received yet (max Power)");//alarm for 10 min
            getSystemConfigParaParser()->setLastLimitRequestSuccess(CMD_NOK);
            return false;
        }
        realLimit = (uint16_t)(limit / (float)maxPower * 100);
    }
    if(realLimit > 100){
        realLimit = 100;
    }
    getSystemConfigParaParser()->setLastLimitRequestSuccess(CMD_PENDING);
    _limitToSet = std::make_unique<uint16_t>(realLimit);
    return true;
}

bool DeyeInverter::sendPowerControlRequest(bool turnOn) {
    if(typeName().startsWith("Unknown") && !DeyeSun.getUnknownDevicesWriteEnable()){
        _alarmLogParser->addAlarm(6,10 * 60,"power command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");//alarm for 10 min
        ESP_LOGI(LogTag().c_str(),"power command not send because Deye Sun device unknown (checked by Serial number), it is possible to override this security check on DTU/Settings page");
        return false;
    }
    _powerTargetStatus = std::make_unique<bool>(turnOn);
    _powerCommandParser->setLastPowerCommandSuccess(CMD_PENDING);
    return true;
}


bool DeyeInverter::resendPowerControlRequest() {
    //todo implement
    return false;
}

bool DeyeInverter::sendRestartControlRequest() {
    // Check if restart already in progress
    //rust check without to return isntantly
    if (_restartCommandFuture.has_value()) {
        ESP_LOGW(LogTag().c_str(), "Restart command already in progress, skipping");
        return false;
    }

    std::lock_guard<std::mutex> lock(_restartCommandMutex);
    //again check now we are secure no other threads run intermediate
    if (_restartCommandFuture.has_value()) {
        ESP_LOGW(LogTag().c_str(), "Restart command already in progress, skipping");
        return false;
    }

    // Build REST request URL - use /up_succ.html endpoint
    String host = _oringalIpOrHostname.c_str();
    if(_resolvedIpByMacAdress != nullptr){
        host = _resolvedIpByMacAdress->c_str();
    }
    String url = "http://" + host;
    url += "/up_succ.html";

    // Prepare headers with authentication and Referer
    std::map<String, String> headers;

    // Form data body
    String body = "HF_PROCESS_CMD=RESTART";

    // Add Referer header (required by Deye protocol)
    String referer = "http://" + host + "/autorestart.html";
    headers["Referer"] = referer;

    // Add Content-Length header
    headers["Content-Length"] = String(body.length());

    // Basic Auth
    if (!addBasicAuthHeader(headers)) {
        return false;
    }

    // Content type
    String contentType = "application/x-www-form-urlencoded";

    // Queue request
    auto future = RestRequestHandler.queueRequestWithHeaders(
        url,
        "POST",
        body,           // Form data
        contentType,    // Content type
        headers,        // Auth + Referer + Content-Length
        2,              // maxRetries
        5000            // timeout
    );

    // Store future for result tracking
    _restartCommandFuture = std::move(future);

    ESP_LOGI(LogTag().c_str(), "Restart command queued for %s", serialString().c_str());
    return true;
}

bool DeyeInverter::addBasicAuthHeader(std::map<String, String>& headers) {
    if (_username.isEmpty() || _password.isEmpty()) {
        return true;  // No auth needed
    }

    String auth = _username + ":" + _password;
    size_t olen = 0;
    unsigned char encoded[128];
    int ret = Base64::encode(encoded, sizeof(encoded), &olen,
                        (const unsigned char*)auth.c_str(), auth.length());

    if (ret == 0) {
        String authHeader = "Basic " + String((char*)encoded);
        headers["Authorization"] = authHeader;
        return true;
    } else {
        ESP_LOGE(LogTag().c_str(), "Failed to encode authentication credentials");
        return false;
    }
}

String DeyeInverter::parseHtmlVariable(const String& htmlBody, const String& varName) {
    // Build search pattern: "var varName = ""
    String searchPattern = "var " + varName + " = \"";

    int startIdx = htmlBody.indexOf(searchPattern);
    if (startIdx < 0) {
        ESP_LOGW(LogTag().c_str(), "Could not find 'var %s' in HTML", varName.c_str());
        return "";
    }

    // Move past the search pattern
    startIdx += searchPattern.length();

    // Find closing quote
    int endIdx = htmlBody.indexOf("\"", startIdx);
    if (endIdx < 0) {
        ESP_LOGW(LogTag().c_str(), "Could not find closing quote for %s", varName.c_str());
        return "";
    }

    // Extract and return value
    return htmlBody.substring(startIdx, endIdx);
}

void DeyeInverter::checkAndFetchFirmwareVersion() {
    // Check if future is pending and ready
    if (_firmwareVersionFuture.has_value()) {
        auto& future = _firmwareVersionFuture.value();

        if (future.wait_for(0) == LightFuture<RestResponse>::Status::READY) {
            auto response = future.get();
            bool fetchSuccess = false;

            if (response.success && response.httpCode == 200) {
                // First validate serial number matches
                String htmlSerial = parseHtmlVariable(response.body, "cover_mid");
                if (htmlSerial.isEmpty()) {
                    ESP_LOGE(LogTag().c_str(), "Could not extract serial number from status.html");
                } else if (!htmlSerial.equalsIgnoreCase(_serialString)) {
                    // Serial mismatch
                    ESP_LOGE(LogTag().c_str(),
                             "Serial number mismatch! Expected: %s, Got: %s - NOT setting firmware version",
                             _serialString.c_str(), htmlSerial.c_str());
                } else {
                    // Serial matches, parse firmware version
                    String version = parseHtmlVariable(response.body, "cover_ver");
                    if (!version.isEmpty()) {
                        _devInfoParser->setHardwareVersion(version);
                        _devInfoParser->setLastUpdate(millis());
                        fetchSuccess = true;
                        ESP_LOGI(LogTag().c_str(), "Successfully fetched firmware version: %s (serial validated)",
                                 version.c_str());
                    } else {
                        ESP_LOGW(LogTag().c_str(), "Could not extract firmware version");
                    }
                }
            } else {
                ESP_LOGW(LogTag().c_str(), "Failed to fetch firmware version (code=%d): %s",
                         response.httpCode, response.body.c_str());
            }

            _firmwareVersionFuture.reset();

            // Set timer based on success/failure
            if (fetchSuccess) {
                _timerFirmwareVersionFetch.set(TIMER_FIRMWARE_VERSION_FETCH_SUCCESS); // 15 minutes
                ESP_LOGD(LogTag().c_str(), "Next firmware fetch in 15 minutes");
            } else {
                _timerFirmwareVersionFetch.set(TIMER_FIRMWARE_VERSION_FETCH_RETRY);   // 30 seconds
                ESP_LOGD(LogTag().c_str(), "Firmware fetch failed, retry in 30 seconds");
            }
        }

        return; // Don't queue new request while one is pending
    }

    // Check if timer has expired (should we fetch?)
    if (!_timerFirmwareVersionFetch.occured()) {
        return;
    }

    String host = _oringalIpOrHostname.c_str();
    if(_resolvedIpByMacAdress != nullptr){
        host = _resolvedIpByMacAdress->c_str();
    }

    // Build URL
    String url = "http://" + host;
    url += "/status.html";

    // Prepare headers with authentication if configured
    std::map<String, String> headers;
    if (!addBasicAuthHeader(headers)) {
        ESP_LOGW(LogTag().c_str(), "Failed to add auth header for firmware fetch");
        // Don't return - continue anyway, might work without auth
    }

    // Queue request
    auto future = RestRequestHandler.queueRequestWithHeaders(
        url,
        "GET",
        "",  // No body
        "",  // No content type
        headers,
        2,      // maxRetries
        10000   // 10s timeout
    );

    _firmwareVersionFuture = std::move(future);

    ESP_LOGD(LogTag().c_str(), "Queued firmware version fetch request");
}

void DeyeInverter::checkRestartCommandResult() {
    // Check if future is pending and ready
    if (_restartCommandFuture.has_value()) {
        auto& future = _restartCommandFuture.value();

        if (future.wait_for(0) == LightFuture<RestResponse>::Status::READY) {
            auto response = future.get();

            if (response.success && response.httpCode == 200) {
                ESP_LOGI(LogTag().c_str(), "Restart command succeeded for %s", serialString().c_str());
            } else {
                ESP_LOGE(LogTag().c_str(), "Restart command failed for %s (code=%d): %s",
                         serialString().c_str(), response.httpCode, response.body.c_str());

                // Add alarm for 5 minutes
                _alarmLogParser->addAlarm(8, 5 * 60, "Restart command failed");
            }

            _restartCommandFuture.reset();
        }
    }
}

void DeyeInverter::checkForNewWriteCommands() {
    if(_currentWritCommand == nullptr && getEnableCommands()) {
        if (_powerTargetStatus != nullptr) {
            ESP_LOGD(LogTag().c_str(),"Start writing register power status");
            _currentWritCommand = std::make_unique<WriteRegisterMapping>("002B", 2,*_powerTargetStatus ? "0001" : "0002");
            _powerTargetStatus = nullptr;
        } else if (_limitToSet != nullptr) {
            ESP_LOGD(LogTag().c_str(),"Start writing register limit");
            _currentWritCommand = std::make_unique<WriteRegisterMapping>("0028", 2, String(DeyeUtils::lengthToHexString(*_limitToSet,4).c_str()));
            _limitToSet = nullptr;
        }
    }
}

