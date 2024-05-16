/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/sio/gbp.h>

mLOG_DEFINE_CATEGORY(GBA_SIO, "GBA Serial I/O", "gba.sio");

const int GBASIOCyclesPerTransfer[4][MAX_GBAS] = {
	{ 31976, 63427, 94884, 125829 },
	{ 8378, 16241, 24104, 31457 },
	{ 5750, 10998, 16241, 20972 },
	{ 3140, 5755, 8376, 10486 }
};

static struct GBASIODriver* _lookupDriver(struct GBASIO* sio, enum GBASIOMode mode) {
	switch (mode) {
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
		return sio->drivers.normal;
	case GBA_SIO_MULTI:
		return sio->drivers.multiplayer;
	case GBA_SIO_JOYBUS:
		return sio->drivers.joybus;
	default:
		return 0;
	}
}

static const char* _modeName(enum GBASIOMode mode) {
	switch (mode) {
	case GBA_SIO_NORMAL_8:
		return "NORMAL8";
	case GBA_SIO_NORMAL_32:
		return "NORMAL32";
	case GBA_SIO_MULTI:
		return "MULTI";
	case GBA_SIO_JOYBUS:
		return "JOYBUS";
	case GBA_SIO_GPIO:
		return "GPIO";
	default:
		return "(unknown)";
	}
}

static void _switchMode(struct GBASIO* sio) {
	unsigned mode = ((sio->rcnt & 0xC000) | (sio->siocnt & 0x3000)) >> 12;
	enum GBASIOMode newMode;
	if (mode < 8) {
		newMode = (enum GBASIOMode) (mode & 0x3);
	} else {
		newMode = (enum GBASIOMode) (mode & 0xC);
	}
	if (newMode != sio->mode) {
		if (sio->activeDriver && sio->activeDriver->unload) {
			sio->activeDriver->unload(sio->activeDriver);
		}
		if (sio->mode != (enum GBASIOMode) -1) {
			mLOG(GBA_SIO, DEBUG, "Switching mode from %s to %s", _modeName(sio->mode), _modeName(newMode));
		}
		sio->mode = newMode;
		sio->activeDriver = _lookupDriver(sio, sio->mode);
		if (sio->activeDriver && sio->activeDriver->load) {
			sio->activeDriver->load(sio->activeDriver);
		}
	}
}

void GBASIOInit(struct GBASIO* sio) {
	sio->drivers.normal = 0;
	sio->drivers.multiplayer = 0;
	sio->drivers.joybus = 0;
	sio->activeDriver = 0;

	sio->gbp.p = sio->p;
	GBASIOPlayerInit(&sio->gbp);

	GBASIOReset(sio);
}

void GBASIODeinit(struct GBASIO* sio) {
	if (sio->activeDriver && sio->activeDriver->unload) {
		sio->activeDriver->unload(sio->activeDriver);
	}
	if (sio->drivers.multiplayer && sio->drivers.multiplayer->deinit) {
		sio->drivers.multiplayer->deinit(sio->drivers.multiplayer);
	}
	if (sio->drivers.joybus && sio->drivers.joybus->deinit) {
		sio->drivers.joybus->deinit(sio->drivers.joybus);
	}
	if (sio->drivers.normal && sio->drivers.normal->deinit) {
		sio->drivers.normal->deinit(sio->drivers.normal);
	}
}

void GBASIOReset(struct GBASIO* sio) {
	if (sio->activeDriver && sio->activeDriver->unload) {
		sio->activeDriver->unload(sio->activeDriver);
	}
	if (sio->drivers.multiplayer && sio->drivers.multiplayer->reset) {
		sio->drivers.multiplayer->reset(sio->drivers.multiplayer);
	}
	if (sio->drivers.joybus && sio->drivers.joybus->reset) {
		sio->drivers.joybus->reset(sio->drivers.joybus);
	}
	if (sio->drivers.normal && sio->drivers.normal->reset) {
		sio->drivers.normal->reset(sio->drivers.normal);
	}
	sio->rcnt = RCNT_INITIAL;
	sio->siocnt = 0;
	sio->mode = -1;
	sio->activeDriver = NULL;
	_switchMode(sio);

	GBASIOPlayerReset(&sio->gbp);
}

void GBASIOSetDriverSet(struct GBASIO* sio, struct GBASIODriverSet* drivers) {
	GBASIOSetDriver(sio, drivers->normal, GBA_SIO_NORMAL_8);
	GBASIOSetDriver(sio, drivers->multiplayer, GBA_SIO_MULTI);
	GBASIOSetDriver(sio, drivers->joybus, GBA_SIO_JOYBUS);
}

void GBASIOSetDriver(struct GBASIO* sio, struct GBASIODriver* driver, enum GBASIOMode mode) {
	struct GBASIODriver** driverLoc;
	switch (mode) {
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
		driverLoc = &sio->drivers.normal;
		break;
	case GBA_SIO_MULTI:
		driverLoc = &sio->drivers.multiplayer;
		break;
	case GBA_SIO_JOYBUS:
		driverLoc = &sio->drivers.joybus;
		break;
	default:
		mLOG(GBA_SIO, ERROR, "Setting an unsupported SIO driver: %x", mode);
		return;
	}
	if (*driverLoc) {
		if ((*driverLoc)->unload) {
			(*driverLoc)->unload(*driverLoc);
		}
		if ((*driverLoc)->deinit) {
			(*driverLoc)->deinit(*driverLoc);
		}
	}
	if (driver) {
		driver->p = sio;

		if (driver->init) {
			if (!driver->init(driver)) {
				driver->deinit(driver);
				mLOG(GBA_SIO, ERROR, "Could not initialize SIO driver");
				return;
			}
		}
	}
	if (sio->activeDriver == *driverLoc) {
		sio->activeDriver = driver;
		if (driver && driver->load) {
			driver->load(driver);
		}
	}
	*driverLoc = driver;
}

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value) {
	sio->rcnt &= 0xF;
	sio->rcnt |= value & ~0xF;
	_switchMode(sio);
	if (sio->activeDriver && sio->activeDriver->writeRegister) {
		sio->activeDriver->writeRegister(sio->activeDriver, GBA_REG_RCNT, value);
	}
}

void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value) {
	if ((value ^ sio->siocnt) & 0x3000) {
		sio->siocnt = value & 0x3000;
		_switchMode(sio);
	}
	if (sio->activeDriver && sio->activeDriver->writeRegister) {
		value = sio->activeDriver->writeRegister(sio->activeDriver, GBA_REG_SIOCNT, value);
	} else {
		// Dummy drivers
		switch (sio->mode) {
		case GBA_SIO_NORMAL_8:
		case GBA_SIO_NORMAL_32:
			value = GBASIONormalFillSi(value);
			if ((value & 0x0081) == 0x0081) {
				if (GBASIONormalIsIrq(value)) {
					// TODO: Test this on hardware to see if this is correct
					GBARaiseIRQ(sio->p, GBA_IRQ_SIO, 0);
				}
				value = GBASIONormalClearStart(value);
			}
			break;
		case GBA_SIO_MULTI:
			value &= 0xFF83;
			value |= 0xC;
			break;
		default:
			// TODO
			break;
		}
	}
	sio->siocnt = value;
}

uint16_t GBASIOWriteRegister(struct GBASIO* sio, uint32_t address, uint16_t value) {
	if (sio->activeDriver && sio->activeDriver->writeRegister) {
		return sio->activeDriver->writeRegister(sio->activeDriver, address, value);
	}
	// Dummy drivers
	switch (sio->mode) {
	case GBA_SIO_JOYBUS:
		switch (address) {
		case GBA_REG_JOYCNT:
			return (value & 0x0040) | (sio->p->memory.io[GBA_REG(JOYCNT)] & ~(value & 0x7) & ~0x0040);
		case GBA_REG_JOYSTAT:
			return (value & 0x0030) | (sio->p->memory.io[GBA_REG(JOYSTAT)] & ~0x30);
		}
		break;
	default:
		// TODO
		break;
	}
	return value;
}
