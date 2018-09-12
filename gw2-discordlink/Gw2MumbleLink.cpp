#include "stdafx.h"
#include "Gw2MumbleLink.h"

Gw2MumbleLink::Gw2MumbleLink() noexcept(false) {
	
	// Try to open Mumble Link mapping, mumble must be running
	this->hMapObject = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink");
	if (this->hMapObject == nullptr) {
		
		// Make new memory mapped file and register it as Mumbe Link file
		this->mumbeFile = CreateFile(L"./mumble_link_file.memory", GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
		if (this->mumbeFile == INVALID_HANDLE_VALUE)
			throw new std::runtime_error("Unable to create mumble link file to disk");
	
		this->hMapObject = CreateFileMapping(this->mumbeFile, NULL, PAGE_READWRITE, 0, sizeof(Gw2MumbleLink::MumbleLinkMemory), L"MumbleLink");
		if (this->hMapObject == nullptr) {
			CloseHandle(this->mumbeFile);
			throw new std::runtime_error("Unable to create mumble link file mapping");
		}

	}

	this->lm = (Gw2MumbleLink::MumbleLinkMemory *)MapViewOfFile(this->hMapObject, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Gw2MumbleLink::MumbleLinkMemory));
	if (this->lm == nullptr) {
		CloseHandle(this->hMapObject);
		throw new std::runtime_error("Unable to read MumbeLink File!");
	}
}

Gw2MumbleLink::~Gw2MumbleLink() noexcept {
	if (hMapObject != nullptr) 
		CloseHandle(hMapObject);

	if (this->mumbeFile != INVALID_HANDLE_VALUE)
		CloseHandle(this->mumbeFile);
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
