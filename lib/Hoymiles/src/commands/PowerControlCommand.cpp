// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2024 Thomas Basler and others
 */

/*
This command is used to power cycle the inverter.

Derives from DevControlCommand.

Command structure:
SCmd: Sub-Command ID
  00 --> Turn On
  01 --> Turn Off
  02 --> Restart

00   01 02 03 04   05 06 07 08   09   10   11   12 13   14   15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
---------------------------------------------------------------------------------------------------------------
                                      |<--->| CRC16
51   71 60 35 46   80 12 23 04   81   00   00   00 00   00   -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
^^   ^^^^^^^^^^^   ^^^^^^^^^^^   ^^   ^^   ^^   ^^^^^   ^^
ID   Target Addr   Source Addr   Cmd  SCmd ?    CRC16   CRC8
*/
#include "PowerControlCommand.h"
#include "inverters/InverterAbstract.h"

#define CRC_SIZE 2

PowerControlCommand::PowerControlCommand(InverterAbstract* inv, const uint64_t router_address)
    : DevControlCommand(inv, router_address)
{
    _payload[10] = 0x00; // TurnOn
    _payload[11] = 0x00;

    udpateCRC(CRC_SIZE); // 2 byte crc

    _payload_size = 14;

    setTimeout(2000);
}

String PowerControlCommand::getCommandName() const
{
    return "PowerControl";
}

namespace {
PowerCommandType getPowerCommandType(const uint8_t payloadType)
{
    if (payloadType == 0x01) {
        return PowerCommandType::Off;
    }
    if (payloadType == 0x02) {
        return PowerCommandType::Restart;
    }

    return PowerCommandType::On;
}
}

bool PowerControlCommand::handleResponse(const fragment_t fragment[], const uint8_t max_fragment_id)
{
    if (!DevControlCommand::handleResponse(fragment, max_fragment_id)) {
        return false;
    }

    _inv->PowerCommand()->setLastUpdateCommand(millis());
    _inv->PowerCommand()->setLastPowerCommandSuccess(CMD_OK);
    _inv->PowerCommand()->setLastCompletedPowerCommandSuccess(CMD_OK);
    _inv->PowerCommand()->setLastCompletedPowerCommand(getPowerCommandType(_payload[10]));
    if (_payload[10] == 0x01) {
        _inv->PowerCommand()->setLastStateCommand(LastStateCommand::Off);
    } else {
        _inv->PowerCommand()->setLastStateCommand(LastStateCommand::On);
    }
    return true;
}

void PowerControlCommand::gotTimeout()
{
    _inv->PowerCommand()->setLastPowerCommandSuccess(CMD_NOK);
    _inv->PowerCommand()->setLastCompletedPowerCommandSuccess(CMD_NOK);
    _inv->PowerCommand()->setLastCompletedPowerCommand(getPowerCommandType(_payload[10]));
}

void PowerControlCommand::setPowerOn(const bool state)
{
    if (state) {
        _payload[10] = 0x00; // TurnOn
    } else {
        _payload[10] = 0x01; // TurnOff
    }

    udpateCRC(CRC_SIZE); // 2 byte crc
}

void PowerControlCommand::setRestart()
{
    _payload[10] = 0x02; // Restart

    udpateCRC(CRC_SIZE); // 2 byte crc
}
