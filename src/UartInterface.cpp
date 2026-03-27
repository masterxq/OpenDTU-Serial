// SPDX-License-Identifier: GPL-2.0-or-later
#include "UartInterface.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Configuration.h>
#include <HardwareSerial.h>
#include <Hoymiles.h>
#include <MqttHandleHass.h>
#include <defaults.h>

namespace {
#if defined(UART_DATA_TX)
HardwareSerial DataSerial(1);

HardwareSerial& getDataSerialPort()
{
    return DataSerial;
}

Stream& getDataSerial()
{
    return getDataSerialPort();
}
#else
Stream& getDataSerial()
{
    return Serial;
}
#endif

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

    return count == 0 ? 0 : total / count;
}

void printJsonString(Print& out, const char* value)
{
    out.write('"');
    while (*value != '\0') {
        if (*value == '"' || *value == '\\') {
            out.write('\\');
        }
        out.write(*value++);
    }
    out.write('"');
}

void printJsonFloat(Print& out, float value)
{
    char buffer[24];
    int len = snprintf(buffer, sizeof(buffer), "%.3f", value);
    while (len > 0 && buffer[len - 1] == '0') {
        --len;
    }
    if (len > 0 && buffer[len - 1] == '.') {
        --len;
    }
    out.write(reinterpret_cast<const uint8_t*>(buffer), len);
}

void printJsonBool(Print& out, bool value)
{
    out.print(value ? "true" : "false");
}

const char* getStateString(LastStateCommand state)
{
    switch (state) {
    case LastStateCommand::On:
        return "on";
    case LastStateCommand::Off:
        return "off";
    case LastStateCommand::Unknown:
        return "";
    }

    return "";
}

const char* getPowerCommandString(PowerCommandType command)
{
    switch (command) {
    case PowerCommandType::On:
        return "on";
    case PowerCommandType::Off:
        return "off";
    case PowerCommandType::Restart:
        return "restart";
    case PowerCommandType::Unknown:
        return "";
    }

    return "";
}

bool isPollingEnabled(std::shared_ptr<InverterAbstract> inv)
{
    return inv->getEnablePolling();
}

void writeLiveObject(Print& out, std::shared_ptr<InverterAbstract> inv, bool includeAge)
{
    auto stats = inv->Statistics();
    out.print("\"live\":{");

    bool needsComma = false;
    if (includeAge) {
        const uint32_t lastUpdate = stats->getLastUpdate();
        out.print("\"live_age_ms\":");
        out.print(lastUpdate > 0 ? millis() - lastUpdate : 0);
        needsComma = true;
    }

    if (needsComma) {
        out.write(',');
    }
    out.print("\"ac_power\":");
    printJsonFloat(out, sumFieldValues(inv, TYPE_AC, FLD_PAC));
    out.print(",\"ac_voltage\":");
    printJsonFloat(out, averageFieldValues(inv, TYPE_AC, FLD_UAC));
    out.print(",\"ac_current\":");
    printJsonFloat(out, sumFieldValues(inv, TYPE_AC, FLD_IAC));

    float dcPower = sumFieldValues(inv, TYPE_DC, FLD_PDC);
    if (dcPower == 0 && stats->hasChannelFieldValue(TYPE_INV, CH0, FLD_PDC)) {
        dcPower = stats->getChannelFieldValue(TYPE_INV, CH0, FLD_PDC);
    }
    out.print(",\"dc_power\":");
    printJsonFloat(out, dcPower);
    out.print(",\"dc_voltage\":");
    printJsonFloat(out, averageFieldValues(inv, TYPE_DC, FLD_UDC));
    out.print(",\"dc_current\":");
    printJsonFloat(out, sumFieldValues(inv, TYPE_DC, FLD_IDC));

    float temperature = 0;
    if (!tryGetFirstValue(inv, TYPE_INV, FLD_T, temperature)) {
        tryGetFirstValue(inv, TYPE_AC, FLD_T, temperature);
    }
    out.print(",\"temperature\":");
    printJsonFloat(out, temperature);
    out.print(",\"reachable\":");
    printJsonBool(out, inv->isReachable());
    out.write('}');
}

void writeStatusObject(Print& out, std::shared_ptr<InverterAbstract> inv)
{
    auto limitParser = inv->SystemConfigPara();
    auto powerParser = inv->PowerCommand();
    const bool pollEnabled = isPollingEnabled(inv);

    out.write('{');
    out.print("\"limit_cmd\":{");

    bool needsComma = false;
    if (limitParser->hasLastAppliedLimitWatts()) {
        out.print("\"watts_applied\":");
        printJsonFloat(out, limitParser->getLastAppliedLimitWatts());
        needsComma = true;
    }
    if (limitParser->getLastUpdateCommand() > 0) {
        if (needsComma) {
            out.write(',');
        }
        out.print("\"last_applied_age_ms\":");
        out.print(millis() - limitParser->getLastUpdateCommand());
        needsComma = true;
    }
    if (needsComma) {
        out.write(',');
    }
    out.print("\"last_result\":");
    printJsonString(out, getLimitResultString(limitParser).c_str());
    out.print(",\"pending_limit\":");
    printJsonBool(out, limitParser->getPendingLimitCommand());
    if (limitParser->getPendingLimitCommand() && limitParser->hasPendingLimitWatts()) {
        out.print(",\"watts_pending\":");
        printJsonFloat(out, limitParser->getPendingLimitWatts());
    }
    if (powerParser->hasLastStateCommand()) {
        out.print(",\"last_state_cmd\":");
        printJsonString(out, getStateString(powerParser->getLastStateCommand()));
    }
    out.print(",\"poll_enabled\":");
    printJsonBool(out, pollEnabled);
    out.write('}');

    out.print(",\"serial\":");
    printJsonString(out, inv->serialString().c_str());
    if (pollEnabled) {
        out.write(',');
        writeLiveObject(out, inv, true);
    }
    out.write('}');
}

void writeDataUpdate(Print& out, std::shared_ptr<InverterAbstract> inv)
{
    out.print("{\"type\":\"data\",\"serial\":");
    printJsonString(out, inv->serialString().c_str());
    out.write(',');
    writeLiveObject(out, inv, false);
    out.println('}');
}

void writeLimitUpdate(Print& out, std::shared_ptr<InverterAbstract> inv)
{
    auto parser = inv->SystemConfigPara();
    const bool success = parser->getLastCompletedLimitCommandSuccess() == CMD_OK;

    out.print("{\"type\":\"limit\",\"serial\":");
    printJsonString(out, inv->serialString().c_str());
    out.print(",\"limit_cmd\":{\"success\":");
    printJsonBool(out, success);
    if (success && parser->hasLastAppliedLimitWatts()) {
        out.print(",\"watts_applied\":");
        printJsonFloat(out, parser->getLastAppliedLimitWatts());
    }
    out.println("}}");
}

void writeStateUpdate(Print& out, std::shared_ptr<InverterAbstract> inv)
{
    auto parser = inv->PowerCommand();
    const bool success = parser->getLastCompletedPowerCommandSuccess() == CMD_OK;
    PowerCommandType command = parser->getLastCompletedPowerCommand();

    out.print("{\"type\":\"state\",\"serial\":");
    printJsonString(out, inv->serialString().c_str());
    out.print(",\"state_cmd\":{\"success\":");
    printJsonBool(out, success);
    if (command != PowerCommandType::Unknown) {
        out.print(",\"command\":");
        printJsonString(out, getPowerCommandString(command));
    }
    if (success && parser->hasLastStateCommand()) {
        out.print(",\"state\":");
        printJsonString(out, getStateString(parser->getLastStateCommand()));
    }
    out.println("}}");
}

void writePollingUpdate(Print& out, std::shared_ptr<InverterAbstract> inv, bool success)
{
    out.print("{\"type\":\"polling\",\"serial\":");
    printJsonString(out, inv->serialString().c_str());
    out.print(",\"polling_cmd\":{\"success\":");
    printJsonBool(out, success);
    out.print(",\"enabled\":");
    printJsonBool(out, isPollingEnabled(inv));
    out.println("}}");
}

void writeAck(Print& out, const char* command, const char* serial, bool success, bool includePolling = false, bool pollingEnabled = true)
{
    out.print("{\"type\":\"ack\",\"command\":");
    printJsonString(out, command);
    if (serial != nullptr && serial[0] != '\0') {
        out.print(",\"serial\":");
        printJsonString(out, serial);
    }
    out.print(",\"success\":");
    printJsonBool(out, success);
    if (includePolling) {
        out.print(",\"polling\":");
        printJsonBool(out, pollingEnabled);
    }
    out.println('}');
}

void writeRequestedData(Print& out, std::shared_ptr<InverterAbstract> singleInv)
{
    out.print("{\"type\":\"requested_data\",\"data\":[");

    if (singleInv != nullptr) {
        writeStatusObject(out, singleInv);
    } else {
        bool first = true;
        for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
            auto inv = Hoymiles.getInverterByPos(i);
            if (inv == nullptr) {
                continue;
            }

            if (!first) {
                out.write(',');
            }
            first = false;
            writeStatusObject(out, inv);
        }
    }

    out.println("]}");
}

bool parseSerialString(JsonVariantConst value, uint64_t& serial)
{
    if (value.isNull() || !value.is<const char*>()) {
        return false;
    }

    serial = strtoull(value.as<const char*>(), nullptr, 16);
    return serial != 0;
}
}

UartInterfaceClass UartInterface;

UartInterfaceClass::UartInterfaceClass()
    : _loopTask(25 * TASK_MILLISECOND, TASK_FOREVER, std::bind(&UartInterfaceClass::loop, this))
{
}

void UartInterfaceClass::init(Scheduler& scheduler)
{
#if defined(UART_DATA_TX)
    getDataSerialPort().begin(SERIAL_BAUDRATE, SERIAL_8N1,
#if defined(UART_DATA_RX)
        UART_DATA_RX,
#else
        -1,
#endif
        UART_DATA_TX);

    _enabled = true;
    scheduler.addTask(_loopTask);
    _loopTask.enable();
    ensureTrackerSize();
#else
    (void)scheduler;
#endif
}

bool UartInterfaceClass::isEnabled() const
{
    return _enabled;
}

void UartInterfaceClass::ensureTrackerSize()
{
    const size_t inverterCount = Hoymiles.getNumInverters();
    if (_trackers.size() == inverterCount) {
        return;
    }

    _trackers.resize(inverterCount);
    for (size_t i = 0; i < inverterCount; i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }

        _trackers[i].lastDataUpdate = inv->Statistics()->getLastUpdate();
        _trackers[i].lastLimitDecisionUpdate = inv->SystemConfigPara()->getLastCompletedLimitCommandUpdate();
        _trackers[i].lastStateDecisionUpdate = inv->PowerCommand()->getLastCompletedPowerCommandUpdate();
        _trackers[i].lastPollEnabled = isPollingEnabled(inv);
    }
}

void UartInterfaceClass::loop()
{
    ensureTrackerSize();
    processIncomingData();
    emitPendingUpdates();
}

void UartInterfaceClass::processIncomingData()
{
    if (!_enabled) {
        return;
    }

    auto& dataSerial = getDataSerial();
    while (dataSerial.available() > 0) {
        int input = dataSerial.read();
        if (input < 0) {
            return;
        }

        char c = static_cast<char>(input);
        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            if (_inputOverflow) {
                writeAck(dataSerial, "invalid_json", nullptr, false);
            } else if (_inputLength > 0) {
                _inputBuffer[_inputLength] = '\0';
                handleCommandLine(_inputBuffer);
            }

            _inputLength = 0;
            _inputOverflow = false;
            continue;
        }

        if (_inputOverflow) {
            continue;
        }

        if (_inputLength + 1 >= INPUT_BUFFER_SIZE) {
            _inputOverflow = true;
            continue;
        }

        _inputBuffer[_inputLength++] = c;
    }
}

void UartInterfaceClass::handleCommandLine(const char* line)
{
    if (!_enabled) {
        return;
    }

    auto& dataSerial = getDataSerial();
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) {
        writeAck(dataSerial, "invalid_json", nullptr, false);
        return;
    }

    const char* type = doc["type"] | "";
    if (strcmp(type, "set_limit") == 0) {
        uint64_t serialNumber = 0;
        if (!parseSerialString(doc["serial"], serialNumber) || doc["watts"].isNull()) {
            writeAck(dataSerial, "set_limit", doc["serial"] | "", false);
            return;
        }

        auto inv = Hoymiles.getInverterBySerial(serialNumber);
        if (inv == nullptr) {
            writeAck(dataSerial, "set_limit", doc["serial"], false);
            return;
        }

        if (!isPollingEnabled(inv)) {
            writeAck(dataSerial, "set_limit", doc["serial"], false, true, false);
            return;
        }

        bool success = inv->sendActivePowerControlRequest(doc["watts"].as<float>(), PowerLimitControlType::AbsolutNonPersistent);
        writeAck(dataSerial, "set_limit", doc["serial"], success);
        return;
    }

    if (strcmp(type, "set_state") == 0) {
        uint64_t serialNumber = 0;
        const char* state = doc["state"] | "";
        if (!parseSerialString(doc["serial"], serialNumber) || state[0] == '\0') {
            writeAck(dataSerial, "set_state", doc["serial"] | "", false);
            return;
        }

        auto inv = Hoymiles.getInverterBySerial(serialNumber);
        if (inv == nullptr) {
            writeAck(dataSerial, "set_state", doc["serial"], false);
            return;
        }

        if (!isPollingEnabled(inv)) {
            writeAck(dataSerial, "set_state", doc["serial"], false, true, false);
            return;
        }

        bool success = false;
        if (strcmp(state, "on") == 0) {
            success = inv->sendPowerControlRequest(true);
        } else if (strcmp(state, "off") == 0) {
            success = inv->sendPowerControlRequest(false);
        } else if (strcmp(state, "restart") == 0) {
            success = inv->sendRestartControlRequest();
        }

        writeAck(dataSerial, "set_state", doc["serial"], success);
        return;
    }

    if (strcmp(type, "get_data") == 0) {
        std::shared_ptr<InverterAbstract> inv = nullptr;
        const char* serialStr = doc["serial"] | "";
        if (serialStr[0] != '\0') {
            uint64_t serialNumber = 0;
            if (!parseSerialString(doc["serial"], serialNumber)) {
                writeAck(dataSerial, "get_data", serialStr, false);
                return;
            }

            inv = Hoymiles.getInverterBySerial(serialNumber);
            if (inv == nullptr) {
                writeAck(dataSerial, "get_data", serialStr, false);
                return;
            }
        }

        writeAck(dataSerial, "get_data", serialStr, true);
        writeRequestedData(dataSerial, inv);
        return;
    }

    if (strcmp(type, "set_polling") == 0) {
        uint64_t serialNumber = 0;
        if (!parseSerialString(doc["serial"], serialNumber) || doc["enabled"].isNull()) {
            writeAck(dataSerial, "set_polling", doc["serial"] | "", false);
            return;
        }

        auto inv = Hoymiles.getInverterBySerial(serialNumber);
        if (inv == nullptr) {
            writeAck(dataSerial, "set_polling", doc["serial"], false);
            return;
        }

        const bool enabled = doc["enabled"].as<bool>();
        bool success = false;
        {
            auto guard = Configuration.getWriteGuard();
            auto& config = guard.getConfig();

            for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
                auto& inverter = config.Inverter[i];
                if (inverter.Serial != serialNumber) {
                    continue;
                }

                inverter.Poll_Enable = enabled;
                inverter.Command_Enable = enabled;
                success = true;
                break;
            }
        }

        if (success) {
            success = Configuration.write();
        }

        if (success) {
            inv->setEnablePolling(enabled);
            inv->setEnableCommands(enabled);
            MqttHandleHass.forceUpdate();
        }

        writeAck(dataSerial, "set_polling", doc["serial"], success);
        return;
    }

    writeAck(dataSerial, type[0] == '\0' ? "unknown" : type, doc["serial"] | "", false);
}

void UartInterfaceClass::emitPendingUpdates()
{
    if (!_enabled) {
        return;
    }

    auto& serial = getDataSerial();
    for (size_t i = 0; i < _trackers.size(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }

        const uint32_t dataUpdate = inv->Statistics()->getLastUpdate();
        if (dataUpdate > 0 && dataUpdate != _trackers[i].lastDataUpdate) {
            _trackers[i].lastDataUpdate = dataUpdate;
            if (isPollingEnabled(inv)) {
                writeDataUpdate(serial, inv);
            }
        }

        const uint32_t limitUpdate = inv->SystemConfigPara()->getLastCompletedLimitCommandUpdate();
        if (limitUpdate > 0 && limitUpdate != _trackers[i].lastLimitDecisionUpdate) {
            _trackers[i].lastLimitDecisionUpdate = limitUpdate;
            writeLimitUpdate(serial, inv);
        }

        const uint32_t stateUpdate = inv->PowerCommand()->getLastCompletedPowerCommandUpdate();
        if (stateUpdate > 0 && stateUpdate != _trackers[i].lastStateDecisionUpdate) {
            _trackers[i].lastStateDecisionUpdate = stateUpdate;
            writeStateUpdate(serial, inv);
        }

        const bool pollEnabled = isPollingEnabled(inv);
        if (pollEnabled != _trackers[i].lastPollEnabled) {
            _trackers[i].lastPollEnabled = pollEnabled;
            writePollingUpdate(serial, inv, true);
        }
    }
}
