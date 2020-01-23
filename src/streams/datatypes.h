/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <memory>
#include <QMetaType>
#include "stream.h"
#include "hrclock.h"

Q_DECLARE_SMART_POINTER_METATYPE(std::shared_ptr)

/**
 * @brief The ModuleState enum
 *
 * Describes the state a module can be in. The state is usually displayed
 * to the user via a module indicator widget.
 */
enum class ModuleState {
    UNKNOWN,      /// Module is in an unknown state
    INITIALIZING, /// Module is initializing after being added
    IDLE,         /// Module is inactive and not started
    PREPARING,    /// Module is preparing a run
    READY,        /// Everything is prepared, we are ready to start
    RUNNING,      /// Module is running
    ERROR         /// Module failed to run / is in an error state
};
Q_DECLARE_METATYPE(ModuleState)

/**
 * @brief The ModuleMessageSeverity enum
 *
 * Sets the severity of a message displayed to the user and added to the
 * experiment log. These messages are emitted by modules and usually received
 * by the MazeAmaze engine.
 */
enum class ModuleMessageSeverity
{
    UNKNOWN,
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief Status message of a module
 *
 * This structure contains information about a module state change or
 * a message sent to the engine by a module.
 * The engine may react to messages (e.g. by terminating the experiment)
 * or simply display a message to the user or log it to a file.
 */
typedef struct
{
    ModuleMessageSeverity severity;
    ModuleState state;
    QString text;
} ModuleMessage;
Q_DECLARE_METATYPE(ModuleMessage)

/**
 * @brief The ControlCommandKind enum
 *
 * Basic operations to control a module from another module.
 */
enum class ControlCommandKind {
    UNKNOWN,
    START,
    STOP,
    STEP,
    CUSTOM
};

/**
 * @brief A control command to a module.
 *
 * Generic data type to stream commands to other modules.
 */
typedef struct
{
    ControlCommandKind kind;
    QString command;
} ControlCommand;
Q_DECLARE_METATYPE(ControlCommand)

/**
 * @brief A new row  for a table
 *
 * Generic type emitted for adding a table row.
 */
using TableRow = QList<QString>;
Q_DECLARE_METATYPE(TableRow)

/**
 * @brief The FirmataCommandKind enum
 *
 * Set which type of change should be made on a Firmata interface.
 */
enum class FirmataCommandKind {
    UNKNOWN,
    IO_MODE,
    WRITE_ANALOG,
    WRITE_DIGITAL,
    SYSEX
};

/**
 * @brief Commands to control Firmata output.
 */
typedef struct
{
    FirmataCommandKind command;
    int mode;
    uint8_t pin;
    bool digitalValue;
    uint16_t analogValue;
} FirmataControl;
Q_DECLARE_METATYPE(FirmataControl)

/**
 * @brief Output data returned from a Firmata device.
 */
typedef struct
{
    uint8_t pin;
    bool digitalValue;
    uint16_t analogValue;
} FirmataData;
Q_DECLARE_METATYPE(FirmataData)
