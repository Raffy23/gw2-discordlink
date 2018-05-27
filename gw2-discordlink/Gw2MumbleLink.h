#pragma once

#include <windows.h>

class Gw2MumbleLink {

private:
	// https://wiki.guildwars2.com/wiki/API:MumbleLink
	struct MumbleLinkMemory {
		UINT32	uiVersion;
		DWORD	uiTick;

		float	fAvatarPosition[3];
		float	fAvatarFront[3];
		float	fAvatarTop[3];
		wchar_t	name[256];
		float	fCameraPosition[3];
		float	fCameraFront[3];
		float	fCameraTop[3];
		wchar_t	identity[256];

		UINT32	context_len;

		unsigned char context[256];
		wchar_t description[2048];
	};

	struct MumbleContext {
		byte serverAddress[28]; // contains sockaddr_in or sockaddr_in6
		unsigned mapId;
		unsigned mapType;
		unsigned shardId;
		unsigned instance;
		unsigned buildId;
	};

	HANDLE hMapObject;
	MumbleLinkMemory *lm;
	
public:
	Gw2MumbleLink();
	~Gw2MumbleLink();

	enum Profession {
		Guardian = 1,
		Warrior,
		Engineer,
		Ranger,
		Thief,
		Elementalist,
		Mesmer,
		Necromancer,
		Revenant
	};

	enum Race {
		Asura,
		Charr,
		Human,
		Norn,
		Sylvari
	};

	enum MapType {
		PvP=2,
		Instances=4,
		PvE=5,
		EternalBattlegrounds = 9,
		BlueBorderlands = 10,
		GreenBorderlands = 11,
		RedBorderlands = 12,
		EotM = 15,
		DryTop = 16
	};

	DWORD getUITick();

	const float* getCharPosition();
	const char *getName();
	const MumbleContext *getMumbleContext();
	std::string getIdentity();

};

