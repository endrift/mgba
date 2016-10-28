/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DebuggerConsoleController.h"

#include "GameController.h"

#include <QMutexLocker>

extern "C" {
#include "debugger/cli-debugger.h"
}

using namespace QGBA;

DebuggerConsoleController::DebuggerConsoleController(GameController* controller, QObject* parent)
	: DebuggerController(controller, &m_cliDebugger.d, parent)
{
	m_backend.d.printf = printf;
	m_backend.d.init = init;
	m_backend.d.deinit = deinit;
	m_backend.d.readline = readLine;
	m_backend.d.lineAppend = lineAppend;
	m_backend.d.historyLast = historyLast;
	m_backend.d.historyAppend = historyAppend;
	m_backend.self = this;

	CLIDebuggerCreate(&m_cliDebugger);
	CLIDebuggerAttachBackend(&m_cliDebugger, &m_backend.d);
}

void DebuggerConsoleController::enterLine(const QString& line) {
	QMutexLocker lock(&m_mutex);
	m_lines.append(line);
	if (m_cliDebugger.d.state == DEBUGGER_RUNNING) {
		mDebuggerEnter(&m_cliDebugger.d, DEBUGGER_ENTER_MANUAL, nullptr);
	}
	m_cond.wakeOne();
}

void DebuggerConsoleController::attachInternal() {
	mCore* core = m_gameController->thread()->core;
	CLIDebuggerAttachSystem(&m_cliDebugger, core->cliDebuggerSystem(core));
}

void DebuggerConsoleController::printf(struct CLIDebuggerBackend* be, const char* fmt, ...) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerConsoleController* self = consoleBe->self;
	va_list args;
	va_start(args, fmt);
	self->log(QString().vsprintf(fmt, args));
	va_end(args);
}

void DebuggerConsoleController::init(struct CLIDebuggerBackend* be) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerConsoleController* self = consoleBe->self;
}

void DebuggerConsoleController::deinit(struct CLIDebuggerBackend* be) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerConsoleController* self = consoleBe->self;
}

const char* DebuggerConsoleController::readLine(struct CLIDebuggerBackend* be, size_t* len) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerConsoleController* self = consoleBe->self;
	GameController::Interrupter interrupter(self->m_gameController, true);
	QMutexLocker lock(&self->m_mutex);
	while (self->m_lines.isEmpty()) {
		self->m_cond.wait(&self->m_mutex);
	}
	self->m_last = self->m_lines.takeFirst().toUtf8();
	*len = self->m_last.size();
	return self->m_last.constData();

}

void DebuggerConsoleController::lineAppend(struct CLIDebuggerBackend* be, const char* line) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerConsoleController* self = consoleBe->self;
	self->lineAppend(QString::fromUtf8(line));
}

const char* DebuggerConsoleController::historyLast(struct CLIDebuggerBackend* be, size_t* len) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerConsoleController* self = consoleBe->self;
	GameController::Interrupter interrupter(self->m_gameController, true);
	QMutexLocker lock(&self->m_mutex);
	self->m_last = self->m_history.last().toUtf8();
	return self->m_last.constData();
}

void DebuggerConsoleController::historyAppend(struct CLIDebuggerBackend* be, const char* line) {
	Backend* consoleBe = reinterpret_cast<Backend*>(be);
	DebuggerConsoleController* self = consoleBe->self;
	GameController::Interrupter interrupter(self->m_gameController, true);
	QMutexLocker lock(&self->m_mutex);
	self->m_history.append(QString::fromUtf8(line));
}
