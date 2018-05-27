#include "stdafx.h"

#include <algorithm>
#include <atomic>
#include <ctime>
#include <signal.h>

#include "discord_rpc.h"

#include "Gw2MumbleLink.h"
#include "RpcClient.h"

#define API_MAP_URL (std::string("https://api.guildwars2.com/v2/maps/"))
#define API_CHAR_URL(c) (std::string("https://api.guildwars2.com/v2/characters/") + c + std::string("/specializations"))
#define SETTINGS_FILE ("./settings.json")

// Todo: Replace with static names in /lang
#define API_SPECS_URL (std::string("https://api.guildwars2.com/v2/specializations/"))

// Given in ms: (Discord API is rate limited to 15 sec, but the discord-rpc does handle this for us)
#define ERROR_SLEEP_TIME (5000)
#define SLEEP_TIME (1000)

RpcClient *rpc;
Gw2MumbleLink *mLink;

// Cache for requests, so API must not be queried so often
std::map<std::string, RpcClient::Response> apiCache;

DiscordEventHandlers discordHandlers;

std::atomic<bool> running;

// Signal handler for SIGINT, SIGBREAK
void signalHandler(int n_signal);

// Discord Handlers
void discordCallback_Ready(const DiscordUser *);
void discordCallback_Error(int, const char *);
void discordCallback_Disconnected(int, const char *);

// Loads the file content into str
void readFromFile(std::string &str, std::ifstream &file);

// Fetches the reponse if cache miss
RpcClient::Response fetchResponse(const std::string &url);

bool isEliteSpecialization(int id);
std::vector<Specializations> specializations {
	Druid , Daredevil , Berserker , Dragonhunter , Reaper , Chronomancer ,
	Scrapper , Tempest , Herald , Soulbeast , Weaver , Holosmith , Deadeye ,
	Mirage , Scourge , Spellbreaker , Firebrand , Renegade
};

int main() {
	// Setup signal handler
	running = true;
	signal(SIGINT, &signalHandler);
	signal(SIGBREAK, &signalHandler);

	std::cout << "Loading config files ..." << std::endl;

	// Read local configuration:
	json config, professions;
	bool querySpecs = true;
	{
		std::ifstream configFile(SETTINGS_FILE);
		std::string str;

		readFromFile(str, configFile);
		config = json::parse(str);

		std::ifstream profFile(std::string("./lang/") + config["lang"].get<std::string>() + "/profession.json");
		readFromFile(str, profFile);

		if (config["gw2-api-key"].empty()) {
			std::cout << "Warning: Without a API-Key only the core professions are shown!" << std::endl;
			querySpecs = false;
		}

		professions = json::parse(str);
	}
	
	// Init RPC & Mumble link
	rpc = new RpcClient(config["lang"].get<std::string>(), config["gw2-api-key"].get<std::string>());
	mLink = new Gw2MumbleLink();

	std::cout << "Connecting to Discord ..." << std::endl;
	const std::string DISCORD_API_KEY = config["discord-api-key"];

	// Initalize Discord API:
	memset(&discordHandlers, 0, sizeof(discordHandlers));
	discordHandlers.ready        = discordCallback_Ready;
	discordHandlers.errored      = discordCallback_Error;
	discordHandlers.disconnected = discordCallback_Disconnected;
	Discord_Initialize(DISCORD_API_KEY.c_str(), &discordHandlers, false, nullptr);

	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	int oldMap = 0;
	std::string name;
	std::string mapname;
	std::string smallImageKey;
	std::string smallImageDesc;
	DWORD oldTick = 0;

	while (running) {
		// Check if Mumble link is connected
		if (mLink->getIdentity().empty()) {
			std::cout << "Could not fetch Name, is Gw2 running?" << std::endl;
			Sleep(ERROR_SLEEP_TIME);
			continue;
		}
		
		// Process Data in Identity JSON
		json identity = json::parse(mLink->getIdentity());
		std::string url = API_MAP_URL + std::to_string(identity["map_id"].get<int>());

		// Query Map name from the API / Cached responses
		RpcClient::Response response = fetchResponse(url);
		if (response.code != CURLE_OK) {
			Sleep(ERROR_SLEEP_TIME);
			continue;
		}

		// Parse JSON and collect data:
		json apiContent = json::parse(response.responseBody);
		int newMapID = identity["map_id"];
		name = identity["name"].get<std::string>();
		mapname = apiContent["name"].get<std::string>();
		smallImageKey = std::string("profession_") + std::to_string(identity["profession"].get<int>());
		smallImageDesc = professions[std::to_string(identity["profession"].get<int>())].get<std::string>();

		// Query specialization if allowed
		if (querySpecs) {
			RpcClient::Response sepcResonse = fetchResponse(API_CHAR_URL(name));
			if (sepcResonse.code != CURLE_OK) {
				std::cout << "Disabling querying of character specializations" << std::endl;
				querySpecs = false;
			} else {
				std::string specType = "pve";

				// Choose right specialization from the list according to maptype
				switch ((Gw2MumbleLink::MapType)mLink->getMumbleContext()->mapType) {
				case Gw2MumbleLink::PvP: 
					specType = "pvp"; 
					break;

				case Gw2MumbleLink::BlueBorderlands:
				case Gw2MumbleLink::RedBorderlands:
				case Gw2MumbleLink::GreenBorderlands:
				case Gw2MumbleLink::EternalBattlegrounds:
				case Gw2MumbleLink::EotM:
					specType = "wvw"; 
					break;

				case Gw2MumbleLink::PvE: 
					break;

				default:
					std::cout << "Error: This map type is unknown ... (" << mLink->getMumbleContext()->mapType << ")" << std::endl;
				}

				// Parse the response to get the specialization ID
				json specs = json::parse(sepcResonse.responseBody);
				int specID = specs["specializations"][specType][2]["id"].get<int>();

				// If it's a elite epsec change the small icon and text
				if (isEliteSpecialization(specID)) {
					//TODO: Replace name query with local file
					auto sName = fetchResponse(API_SPECS_URL + std::to_string(specID));
					json jName = json::parse(sName.responseBody);
					
					smallImageKey = std::string("spec_") + std::to_string(specID);
					smallImageDesc = jName["name"].get<std::string>();
				}
			}
		}
		
		// Debug stuff: 
		//std::cout << "[Debug]: mapType:" << mLink->getMumbleContext()->mapType << std::endl;
		//std::cout << "[Debug]: uiTick: " << mLink->getUITick() << std::endl;

		// Set Discord API stuff (needs data until send off ...):
		discordPresence.details = name.c_str();  
		discordPresence.state = mapname.c_str();  
		discordPresence.largeImageKey = "gw2-logo";
		discordPresence.smallImageKey = smallImageKey.c_str();

		// TODO: Translate / or use characters API
		discordPresence.smallImageText = smallImageDesc.c_str();

		// Update timestamp only after Mapchange
		if (oldMap != newMapID) {
			discordPresence.startTimestamp = std::time(nullptr);
			oldMap = newMapID;
		}

		// Update Discord
		Discord_UpdatePresence(&discordPresence);
		
		// Update every second for accurate timestamp
		Sleep(SLEEP_TIME);
	}

	std::cout << "Cleaning up ..." << std::endl;

	// Cleanup 
	Discord_Shutdown();
	delete rpc;
	delete mLink;
	apiCache.clear();

    return EXIT_SUCCESS;
}

void signalHandler(int n_signal) {
	std::cout << "Shutting down ..." << std::endl;
	running = false;
}

void discordCallback_Ready(const DiscordUser *user) {
	std::cout << "Discord API is now ready for " << user->username << std::endl;
}

void discordCallback_Error(int errorCode, const char *message) {
	std::cout << "Discord API Error (" << errorCode << ") : " << message << std::endl;
}

void discordCallback_Disconnected(int errorCode, const char *message) {
	std::cout << "Disconnected from Discord (" << message << ")" << std::endl;
}

void readFromFile(std::string &str, std::ifstream &file) {
	file.seekg(0, std::ios::end);
	str.reserve(file.tellg());
	file.seekg(0, std::ios::beg);

	str.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

RpcClient::Response fetchResponse(const std::string &url) {
	RpcClient::Response response;
	auto cachedEntry = apiCache.find(url);
	
	if (cachedEntry == apiCache.end()) {
		std::cout << "Fetching: " << url << std::endl;
		response = rpc->fetch(url);
		if (response.code != CURLE_OK) {
			std::cerr << "Unable to reach API Endpoint!" << std::endl;
		} else {
			apiCache[url] = response;
		}
	} else {
		response = cachedEntry->second;
	}

	return response;
}

bool isEliteSpecialization(int id) {
	return std::binary_search(specializations.begin(), specializations.end(), id);
}