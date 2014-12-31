/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GamePakView.h"

#include "GameController.h"

extern "C" {
#include "gba-thread.h"
}

using namespace QGBA;

GamePakView::GamePakView(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	connect(controller, SIGNAL(gameStarted(GBAThread*)), this, SLOT(gameStarted(GBAThread*)));
	connect(controller, SIGNAL(gameStopped(GBAThread*)), this, SLOT(gameStopped()));

	if (controller->isLoaded()) {
		gameStarted(controller->thread());
	}
}

void GamePakView::gameStarted(GBAThread* thread) {
	if (!thread->gba) {
		gameStopped();
		return;
	}
	SavedataType savetype = thread->gba->memory.savedata.type;
	if (m_ui.savetype->currentIndex() > 0) {
		if (savetype > SAVEDATA_NONE) {
			VFile* vf = thread->gba->memory.savedata.vf;
			GBASavedataDeinit(&thread->gba->memory.savedata);
			GBASavedataInit(&thread->gba->memory.savedata, vf);
		}
		savetype = static_cast<SavedataType>(m_ui.savetype->currentIndex() - 1);
		GBASavedataForceType(&thread->gba->memory.savedata, savetype);
	}

	if (savetype > SAVEDATA_NONE) {
		m_ui.savetype->setCurrentIndex(savetype + 1);
	}
	m_ui.savetype->setEnabled(false);

	m_ui.sensorRTC->setChecked(thread->gba->memory.gpio.gpioDevices & GPIO_RTC);
	m_ui.sensorGyro->setChecked(thread->gba->memory.gpio.gpioDevices & GPIO_GYRO);
	m_ui.sensorLight->setChecked(thread->gba->memory.gpio.gpioDevices & GPIO_LIGHT_SENSOR);
}

void GamePakView::gameStopped() {	
	m_ui.savetype->setCurrentIndex(0);
	m_ui.savetype->setEnabled(true);

	m_ui.sensorRTC->setChecked(false);
	m_ui.sensorGyro->setChecked(false);
	m_ui.sensorLight->setChecked(false);
}
