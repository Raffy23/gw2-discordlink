#pragma once

#include <algorithm>

#define GW2_ENDPOINT "https://api.guildwars2.com"

#define API_MAP_URL (std::string(GW2_ENDPOINT"/v2/maps/"))
#define API_CHAR_URL(character) (std::string(GW2_ENDPOINT"/v2/characters/") + character + std::string("/specializations"))

// Todo: Replace with static names in /lang
#define API_SPECS_URL (std::string(GW2_ENDPOINT"/v2/specializations/"))

// Some magic constances
#define MAP_TYPE_PVE (std::string("pve"))
#define MAP_TYPE_PVP (std::string("pvp"))
#define MAP_TYPE_WVW (std::string("wvw"))

const std::vector<Specializations> specializations {
	Druid , Daredevil , Berserker , Dragonhunter , Reaper , Chronomancer ,
	Scrapper , Tempest , Herald , Soulbeast , Weaver , Holosmith , Deadeye ,
	Mirage , Scourge , Spellbreaker , Firebrand , Renegade
};

bool isEliteSpecialization(int id) {
	return std::binary_search(specializations.begin(), specializations.end(), id);
}