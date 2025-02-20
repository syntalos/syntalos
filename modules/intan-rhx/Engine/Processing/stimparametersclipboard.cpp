//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.4.0
//
//  Copyright (c) 2020-2025 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include "stimparametersclipboard.h"

StimParametersClipboard::StimParametersClipboard(SystemState* state_, ControllerInterface* controllerInterface_) :
    state(state_),
    controllerInterface(controllerInterface_),
    full(false),
    stimParameters(nullptr)
{

}

void StimParametersClipboard::copy(Channel* selectedChannel)
{
    stimParameters = selectedChannel->stimParameters;

    signalType = selectedChannel->getSignalType();
    full = true;
}

void StimParametersClipboard::paste(QList<Channel*> selectedChannels) const
{
    if (state->running) {
        qDebug() << "Error: pasting shouldn't be possible while board is running. Aborting paste";
        return;
    }

    for (QList<Channel*>::iterator i = selectedChannels.begin(); i != selectedChannels.end(); ++i) {
        (*i)->stimParameters->populateParametersFrom(stimParameters);
        controllerInterface->uploadStimParameters(*i);
    }

    state->forceUpdate();
}

bool StimParametersClipboard::isFull() const
{
    return full;
}

SignalType StimParametersClipboard::getSignalType() const
{
    return signalType;
}
