/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_OVERRIDE_VIEW
#define QGBA_OVERRIDE_VIEW

#include <QDialog>

#include <memory>

#ifdef M_CORE_GB
#include <mgba/gb/interface.h>
#endif

#include "Override.h"

#include "ui_OverrideView.h"

struct mCoreThread;

namespace QGBA {

class ConfigController;
class CoreController;
class Override;

class OverrideView : public QDialog {
Q_OBJECT

public:
	OverrideView(ConfigController* config, QWidget* parent = nullptr);

	void setController(std::shared_ptr<CoreController> controller);

public slots:
	void saveOverride();

private slots:
	void updateOverrides();
	void gameStarted();
	void gameStopped();

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	Ui::OverrideView m_ui;

	std::shared_ptr<CoreController> m_controller;
	std::unique_ptr<Override> m_override;
	ConfigController* m_config;

#ifdef M_CORE_GB
	uint32_t m_gbColors[4]{};

	static QList<enum GBModel> s_gbModelList;
	static QList<enum GBMemoryBankControllerType> s_mbcList;
#endif
};

}

#endif
