#pragma once

#include "targetver.h"

#include <iostream>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <codecvt>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <fstream>

#include "json.hpp"

using json = nlohmann::json;

enum Specializations {
	Druid = 5,
	Daredevil = 7,
	Berserker = 18,
	Dragonhunter = 27,
	Reaper = 34,
	Chronomancer = 40,
	Scrapper = 43,
	Tempest = 48,
	Herald = 52,
	Soulbeast = 55,
	Weaver = 56,
	Holosmith = 57,
	Deadeye = 58,
	Mirage = 59,
	Scourge = 60,
	Spellbreaker = 61,
	Firebrand = 62,
	Renegade = 63
};