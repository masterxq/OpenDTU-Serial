// SPDX-License-Identifier: GPL-2.0-or-later
#include "WebApi_v1.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include <AsyncJson.h>
#include <Hoymiles.h>

namespace {
String getLimitResultString(SystemConfigParaParser* parser)
{
    if (!parser->hasCompletedLimitCommandResult()) {
        return "never";
    }

    return parser->getLastCompletedLimitCommandSuccess() == CMD_OK ? "ok" : "failure";
}

bool tryGetFirstValue(std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const FieldId_t fieldId, float& value)
{
    for (auto& channel : inv->Statistics()->getChannelsByType(type)) {
        if (inv->Statistics()->hasChannelFieldValue(type, channel, fieldId)) {
            value = inv->Statistics()->getChannelFieldValue(type, channel, fieldId);
            return true;
        }
    }

    return false;
}

float sumFieldValues(std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const FieldId_t fieldId)
{
    float total = 0;
    for (auto& channel : inv->Statistics()->getChannelsByType(type)) {
        if (inv->Statistics()->hasChannelFieldValue(type, channel, fieldId)) {
            total += inv->Statistics()->getChannelFieldValue(type, channel, fieldId);
        }
    }

    return total;
}

float averageFieldValues(std::shared_ptr<InverterAbstract> inv, const ChannelType_t type, const FieldId_t fieldId)
{
    float total = 0;
    uint8_t count = 0;

    for (auto& channel : inv->Statistics()->getChannelsByType(type)) {
        if (inv->Statistics()->hasChannelFieldValue(type, channel, fieldId)) {
            total += inv->Statistics()->getChannelFieldValue(type, channel, fieldId);
            ++count;
        }
    }

    if (count == 0) {
        return 0;
    }

    return total / count;
}

void addLiveData(JsonObject& root, std::shared_ptr<InverterAbstract> inv)
{
    auto stats = inv->Statistics();
    auto live = root["live"].to<JsonObject>();

    const uint32_t lastUpdate = stats->getLastUpdate();
    live["live_age_ms"] = lastUpdate > 0 ? millis() - lastUpdate : 0;
    live["ac_power"] = sumFieldValues(inv, TYPE_AC, FLD_PAC);
    live["ac_voltage"] = averageFieldValues(inv, TYPE_AC, FLD_UAC);
    live["ac_current"] = sumFieldValues(inv, TYPE_AC, FLD_IAC);

    float dcPower = sumFieldValues(inv, TYPE_DC, FLD_PDC);
    if (dcPower == 0 && stats->hasChannelFieldValue(TYPE_INV, CH0, FLD_PDC)) {
        dcPower = stats->getChannelFieldValue(TYPE_INV, CH0, FLD_PDC);
    }
    live["dc_power"] = dcPower;
    live["dc_voltage"] = averageFieldValues(inv, TYPE_DC, FLD_UDC);
    live["dc_current"] = sumFieldValues(inv, TYPE_DC, FLD_IDC);

    float temperature = 0;
    if (!tryGetFirstValue(inv, TYPE_INV, FLD_T, temperature)) {
        tryGetFirstValue(inv, TYPE_AC, FLD_T, temperature);
    }
    live["temperature"] = temperature;
    live["reachable"] = inv->isReachable();
}

void addInverterStatus(JsonObject& root, std::shared_ptr<InverterAbstract> inv)
{
    auto limitParser = inv->SystemConfigPara();
    auto powerParser = inv->PowerCommand();
    auto limit = root["limit_cmd"].to<JsonObject>();

    root["serial"] = inv->serialString();

    if (limitParser->hasLastAppliedLimitWatts()) {
        limit["watts_applied"] = limitParser->getLastAppliedLimitWatts();
    }
    if (limitParser->getLastUpdateCommand() > 0) {
        limit["last_applied_age_ms"] = millis() - limitParser->getLastUpdateCommand();
    }
    limit["last_result"] = getLimitResultString(limitParser);
    limit["pending_limit"] = limitParser->getPendingLimitCommand();
    if (limitParser->getPendingLimitCommand() && limitParser->hasPendingLimitWatts()) {
        limit["watts_pending"] = limitParser->getPendingLimitWatts();
    }

    if (powerParser->hasLastStateCommand()) {
        switch (powerParser->getLastStateCommand()) {
        case LastStateCommand::On:
            limit["last_state_cmd"] = "on";
            break;
        case LastStateCommand::Off:
            limit["last_state_cmd"] = "off";
            break;
        case LastStateCommand::Unknown:
            break;
        }
    }

    addLiveData(root, inv);
}
}

void WebApiV1Class::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    (void)scheduler;
    server.on("/api/v1/status", HTTP_GET, std::bind(&WebApiV1Class::onStatus, this, _1));
}

void WebApiV1Class::onStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    const uint64_t serial = WebApi.parseSerialFromRequest(request, "serial");
    AsyncJsonResponse* response = new AsyncJsonResponse();

    if (serial != 0) {
        auto inv = Hoymiles.getInverterBySerial(serial);
        if (inv == nullptr) {
            auto& root = response->getRoot();
            root["message"] = "Invalid inverter specified!";
            root["code"] = WebApiError::InverterInvalidId;
            root["type"] = "warning";
            response->setCode(404);
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        auto root = response->getRoot().to<JsonObject>();
        addInverterStatus(root, inv);
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    auto root = response->getRoot().to<JsonArray>();
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }

        auto invRoot = root.add<JsonObject>();
        addInverterStatus(invRoot, inv);
    }

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}
