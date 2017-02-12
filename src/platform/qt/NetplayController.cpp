/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "NetplayController.h"

#include <mgba/internal/netplay/server.h>

#include "GameController.h"

#include <QUuid>

using namespace QGBA;

const uint16_t NetplayController::DEFAULT_PORT = 4267;

const mNPCallbacks NetplayController::s_callbacks = {
	NetplayController::cbServerConnected,
	NetplayController::cbServerShutdown,
	NetplayController::cbCoreRegistered,
	NetplayController::cbRoomJoined,
	NetplayController::cbListRooms,
	NetplayController::cbListCores,
	NetplayController::cbRollbackStart,
	NetplayController::cbRollbackEnd,
};

NetplayController::NetplayController(MultiplayerController* mp, QObject* parent)
	: QObject(parent)
	, m_multiplayer(mp)
	, m_np(nullptr)
	, m_server(nullptr)
	, m_connected(false)
{
	qRegisterMetaType<QList<mNPRoomInfo>>("QList<mNPRoomInfo>");
	qRegisterMetaType<QList<mNPCoreInfo>>("QList<mNPCoreInfo>");
	qRegisterMetaType<QList<quint32>>("QList<quint32>");

	connect(this, &NetplayController::connected, [this]() {
		for (auto iter = m_pendingCores.begin(); iter != m_pendingCores.end(); ++iter) {
			mNPContextRegisterCore(m_np, iter.value()->thread(), iter.key());
		}
	});
	connect(this, SIGNAL(connected()), this, SLOT(updateRooms()));
	connect(this, SIGNAL(connected()), this, SLOT(updateCores()));
	connect(this, SIGNAL(roomJoined(quint32, quint32)), this, SLOT(updateRooms()));
	connect(this, SIGNAL(roomJoined(quint32, quint32)), this, SLOT(updateCores()));
}

NetplayController::~NetplayController() {
	disconnectFromServer();
	stopServer();
}

bool NetplayController::startServer(const QString& address, uint16_t port) {
	if (m_server) {
		return false;
	}
	mNPServerOptions opts;
	opts.port = port;
	if (!SocketResolveHostname(address.toUtf8().constData(), &opts.address)) {
		return false;
	}
	m_server = mNPServerStart(&opts);
	if (!m_server) {
		return false;
	}
	if (!connectToServer(address, port)) {
		stopServer();
		return false;
	}
	return true;
}

void NetplayController::stopServer() {
	if (!m_server) {
		return;
	}
	mNPServerStop(m_server);
	m_server = nullptr;
}

bool NetplayController::connectToServer(const QString& address, uint16_t port) {
	if (m_np) {
		return false;
	}
	mNPServerOptions opts;
	opts.port = port;
	if (!SocketResolveHostname(address.toUtf8().constData(), &opts.address)) {
		return false;
	}
	m_connectedHost = address;
	m_connectedPort = port;
	m_np = mNPContextCreate();
	mNPContextAttachCallbacks(m_np, &s_callbacks, this);
	if (!mNPContextConnect(m_np, &opts)) {
		mNPContextDestroy(m_np);
		m_np = nullptr;
		return false;
	}
	return true;
}

void NetplayController::disconnectFromServer() {
	if (!m_np) {
		return;
	}
	mNPContextDisconnect(m_np);
	mNPContextDestroy(m_np);
	m_cores.clear();
	m_pendingCores.clear();
	m_np = nullptr;
	emit disconnected();
}

void NetplayController::listRooms(std::function<void (const QList<mNPRoomInfo>&)> callback) {
	if (!m_np) {
		return;
	}
	m_listRoomsCallbacks.append(callback);
	mNPContextListRooms(m_np);
}

void NetplayController::listCores(std::function<void (const QList<mNPCoreInfo>&)> callback, uint32_t roomId) {
	if (!m_np) {
		return;
	}
	m_listCoresCallbacks[roomId].append(callback);
	mNPContextListCores(m_np, roomId);
}

QString NetplayController::connectedHost() const {
	if (m_np) {
		return m_connectedHost;
	}
	return QString();
}

void NetplayController::addGameController(GameController* controller) {
	if (!controller->isLoaded()) {
		return;
	}
	QList<uint32_t> keys = m_cores.keys(controller);
	if (!keys.empty()) {
		return;
	}
	uint32_t nonce = qHash(QUuid::createUuid());
	while (m_pendingCores.contains(nonce)) {
		nonce = qHash(QUuid::createUuid());
	}
	m_pendingCores[nonce] = controller;
	if (m_np) {
		mNPContextRegisterCore(m_np, controller->thread(), nonce);
	}
}

void NetplayController::clone(GameController* controller, quint32 coreId, quint32 flags) {
	if (!m_np) {
		return;
	}
	if (!controller->isLoaded()) {
		// TODO
		return;
	}
	uint32_t nonce = qHash(QUuid::createUuid());
	while (m_pendingCores.contains(nonce)) {
		nonce = qHash(QUuid::createUuid());
	}
	m_pendingCores[nonce] = controller;
	mNPContextCloneCore(m_np, coreId, flags, nonce);
}

void NetplayController::addGameController(uint32_t nonce, uint32_t id) {
	if (!m_np) {
		return;
	}
	GameController* controller = m_pendingCores.take(nonce);
	if (m_cores.key(controller)) {
		mNPContextDeleteCore(m_np, m_cores.key(controller));
		mNPContextAttachCore(m_np, controller->thread(), nonce);
		m_cores[id] = controller;
	} else {
		mNPContextAttachCore(m_np, controller->thread(), nonce);
		m_cores[id] = controller;
		joinFirstRoom(controller);
	}
	auto connection = connect(controller, &GameController::keysUpdated, [this, id](quint32 keys) {
		mNPContextPushInput(m_np, id, keys);
	});
	connect(this, &NetplayController::disconnected, [this, connection]() {
		disconnect(connection);
	});
	emit coreRegistered(id);
}

void NetplayController::joinRoom(GameController* controller, quint32 roomId) {
	if (!m_np) {
		return;
	}
	// TODO: Add reverse mapping?
	QList<uint32_t> keys = m_cores.keys(controller);
	if (keys.empty()) {
		return;
	}
	mNPContextJoinRoom(m_np, roomId, keys[0]);
}

void NetplayController::joinFirstRoom(GameController* controller) {
	if (!m_np) {
		return;
	}
	// TODO: Add reverse mapping?
	QList<uint32_t> keys = m_cores.keys(controller);
	if (keys.empty()) {
		return;
	}
	uint32_t coreId = keys[0];
	listRooms([this, coreId](const QList<mNPRoomInfo>& rooms) {
		for (const auto& room : rooms) {
			if (room.nCores < room.capacity) {
				mNPContextJoinRoom(m_np, room.roomId, coreId);
				return;
			}
		}
		mNPContextJoinRoom(m_np, 0, coreId);
	});
}

void NetplayController::cbListRooms(QList<mNPRoomInfo> list) {
	if (m_listRoomsCallbacks.empty()) {
		return;
	}
	auto cb = m_listRoomsCallbacks.takeFirst();
	cb(list);
}

void NetplayController::cbRollbackStart(QList<quint32> list) {
	for (const auto& coreId : list) {
		GameController* controller = m_cores[coreId];
		if (controller) {
			controller->setKeyInputBlocked(true);
			controller->setOutputBlocked(true);
		}
	}
}

void NetplayController::cbRollbackEnd(QList<quint32> list) {
	for (const auto& coreId : list) {
		GameController* controller = m_cores[coreId];
		if (controller) {
			controller->setKeyInputBlocked(false);
			controller->setOutputBlocked(false);
		}
	}
}

void NetplayController::cbListCores(QList<mNPCoreInfo> list, quint32 roomId) {
	QList<std::function<void (const QList<mNPCoreInfo>&)>>& cbList = m_listCoresCallbacks[roomId];
	if (cbList.empty()) {
		return;
	}
	auto cb = cbList.takeFirst();
	cb(list);
	if (cbList.empty()) {
		m_listCoresCallbacks.take(roomId);
	}
}

void NetplayController::updateRooms() {
	listRooms([this](const QList<mNPRoomInfo>& rooms) {
		m_roomInfo = rooms;
	});
}

void NetplayController::updateCores() {
	listCores([this](const QList<mNPCoreInfo>& cores) {
		m_coreInfo = cores;
	});
}

void NetplayController::cbServerConnected(mNPContext* context, void* user) {
	NetplayController* controller = static_cast<NetplayController*>(user);
	controller->m_connected = true;
	controller->connected();
}

void NetplayController::cbServerShutdown(mNPContext* context, void* user) {
	NetplayController* controller = static_cast<NetplayController*>(user);
	controller->m_connected = true;
	QMetaObject::invokeMethod(controller, "disconnectFromServer");
	QMetaObject::invokeMethod(controller, "stopServer");
}

void NetplayController::cbCoreRegistered(mNPContext* context, const mNPCoreInfo* info, uint32_t nonce, void* user) {
	QMetaObject::invokeMethod(static_cast<NetplayController*>(user), "addGameController", Q_ARG(quint32, nonce), Q_ARG(quint32, info->coreId));
}

void NetplayController::cbRoomJoined(mNPContext* context, uint32_t roomId, uint32_t coreId, void* user) {
	NetplayController* controller = static_cast<NetplayController*>(user);
	QMetaObject::invokeMethod(controller, "roomJoined", Q_ARG(quint32, roomId), Q_ARG(quint32, coreId));
}

void NetplayController::cbListRooms(mNPContext* context, const struct mNPRoomInfo* rooms, uint32_t nRooms, void* user) {
	QList<mNPRoomInfo> list;
	if (nRooms) {
		list.reserve(nRooms);
		std::copy(&rooms[0], &rooms[nRooms], std::back_inserter(list));
	}
	QMetaObject::invokeMethod(static_cast<NetplayController*>(user), "cbListRooms", Q_ARG(QList<mNPRoomInfo>, list));
}

void NetplayController::cbListCores(mNPContext* context, const struct mNPCoreInfo* cores, uint32_t nCores, uint32_t roomId, void* user) {
	QList<mNPCoreInfo> list;
	if (nCores) {
		list.reserve(nCores);
		std::copy(&cores[0], &cores[nCores], std::back_inserter(list));
	}
	QMetaObject::invokeMethod(static_cast<NetplayController*>(user), "cbListCores", Q_ARG(QList<mNPCoreInfo>, list), Q_ARG(quint32, roomId));
}

void NetplayController::cbRollbackStart(mNPContext* context, const uint32_t* cores, uint32_t nCores, void* user) {
	QList<quint32> list;
	if (nCores) {
		list.reserve(nCores);
		std::copy(&cores[0], &cores[nCores], std::back_inserter(list));
	}
	QMetaObject::invokeMethod(static_cast<NetplayController*>(user), "cbRollbackStart", Q_ARG(QList<quint32>, list));
}

void NetplayController::cbRollbackEnd(mNPContext* context, const uint32_t* cores, uint32_t nCores, void* user) {
	QList<quint32> list;
	if (nCores) {
		list.reserve(nCores);
		std::copy(&cores[0], &cores[nCores], std::back_inserter(list));
	}
	QMetaObject::invokeMethod(static_cast<NetplayController*>(user), "cbRollbackEnd", Q_ARG(QList<quint32>, list));
}
