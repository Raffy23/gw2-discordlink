#include "stdafx.h"
#include "Gw2MumbleLink.h"

Gw2MumbleLink::Gw2MumbleLink() {
	/* TODO: Mumble must be running otherwise file is not present! */
	this->hMapObject = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
	if (this->hMapObject == NULL) {
		throw new std::runtime_error("Unable to open MumbeLink File!");
	}

	this->lm = (Gw2MumbleLink::MumbleLinkMemory *)MapViewOfFile(this->hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Gw2MumbleLink::MumbleLinkMemory));
	if (this->lm == NULL) {
		CloseHandle(this->hMapObject);
		throw new std::runtime_error("Unable to read MumbeLink File!");
	}
}

Gw2MumbleLink::~Gw2MumbleLink() {
	CloseHandle(hMapObject);
}

DWORD Gw2MumbleLink::getUITick() {
	return this->lm->uiTick;
}

const float * Gw2MumbleLink::getCharPosition() {
	return this->lm->fAvatarPosition;
}

const char * Gw2MumbleLink::getName() {
	return (const char *)this->lm->name;
}

const Gw2MumbleLink::MumbleContext * Gw2MumbleLink::getMumbleContext() {
	return (MumbleContext *)this->lm->context;
}

std::string Gw2MumbleLink::getIdentity() {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	auto idy = std::wstring(this->lm->identity);

	return converter.to_bytes(idy);
}
