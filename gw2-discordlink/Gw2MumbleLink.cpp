#include "stdafx.h"
#include "Gw2MumbleLink.h"

Gw2MumbleLink::Gw2MumbleLink() noexcept(false) {
	// Try to open Mumble Link mapping, mumble must be running
	this->hMapObject = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
	if (this->hMapObject == nullptr) {
		// Create shared named memory for Mumbe Link
		this->hMapObject = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(Gw2MumbleLink::MumbleLinkMemory), L"MumbleLink");
		if (this->hMapObject == nullptr) {
			throw new std::runtime_error("Unable to create mumble link file mapping");
		}
	}

	this->lm = (Gw2MumbleLink::MumbleLinkMemory *)MapViewOfFile(this->hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Gw2MumbleLink::MumbleLinkMemory));
	if (this->lm == nullptr) {
		CloseHandle(this->hMapObject);
		throw new std::runtime_error("Unable to read MumbeLink File!");
	}

	// Zero out Mumbe Link memory, Gw2 does only write to it when fully loaded
	// and does not zero it out itfself (there might still be data from other games)
	memset(this->lm, 0, sizeof(Gw2MumbleLink::MumbleLinkMemory));
}

Gw2MumbleLink::~Gw2MumbleLink() noexcept {
	if (this->lm != nullptr)
		UnmapViewOfFile(this->lm);

	if (hMapObject != nullptr) 
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

void Gw2MumbleLink::cleanupMumbeLinkMemory() {
	memset(this->lm, 0, sizeof(Gw2MumbleLink::MumbleLinkMemory));
}
