#include "stdafx.h"

#include <atomic>
#include <ctime>
#include <signal.h>
#include <thread>

#include "discord_rpc.h"

#include "Gw2MumbleLink.h"
#include "RpcClient.h"
#include "Gw2Definitions.h"

#include "resource.h"

// Name of the file which will be loaded at startup
#define SETTINGS_FILE ("./settings.json")

// Given in ms: (Discord API is rate limited to 15 sec, but the discord-rpc does handle this for us)
#define ERROR_SLEEP_TIME (5000)
#define SLEEP_TIME (1000)

// Global variables for state
RpcClient *rpc;
Gw2MumbleLink *mLink;
std::atomic<bool> running;

// Cache for requests, so API must not be queried so often
std::map<std::string, RpcClient::Response> apiCache;

// Background thread function, so UI / Thread does no networking 
void worker(bool querySpecs, json &professions);

// Signal handler for SIGINT, SIGBREAK
void signalHandler(int n_signal);

// Discord Handlers
DiscordEventHandlers discordHandlers;
void discordCallback_Ready(const DiscordUser *);
void discordCallback_Error(int, const char *);
void discordCallback_Disconnected(int, const char *);

// Loads the file content into str
void readFromFile(std::string &str, std::ifstream &file);

// Fetches the reponse if cache miss
RpcClient::Response fetchResponse(const std::string &url);

// WinAPI defintions
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#define WM_TRAYICON (WM_USER + 0x100)
#define ID_TRAY_EXIT_CONTEXT_MENU_ITEM 0x1238
NOTIFYICONDATA systemTrayData;
HMENU contextMenu;

// WinMain Entry Point
int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow) {
	// Setup signal handler
	signal(SIGINT, &signalHandler);
	signal(SIGBREAK, &signalHandler);
	running = false;

	// Setup Window stuff:
	LPCWSTR lpClassName = L"gw2-discordlink";
	WNDCLASSEX wndClass;
	memset(&wndClass, 0, sizeof(WNDCLASSEX));
	wndClass.lpfnWndProc   = WndProc;
	wndClass.hInstance     = hInstance;
	wndClass.lpszClassName = lpClassName;
	wndClass.cbSize        = sizeof(WNDCLASSEX);
	RegisterClassEx(&wndClass);
	
	// Create a invisible Window just to have a handle & a message loop
	HWND hWnd = CreateWindowEx(0, lpClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance, nullptr);
	if (hWnd == nullptr) {
		std::cerr << "Error: Unable to create Window!" << std::endl;
		return FALSE;
	}

	
	// Init Systemtray icon & stuff:
	contextMenu = CreatePopupMenu();
	AppendMenu(contextMenu, MF_STRING, ID_TRAY_EXIT_CONTEXT_MENU_ITEM, L"Exit");
	
	memset(&systemTrayData, 0, sizeof(NOTIFYICONDATA));
	systemTrayData.cbSize = sizeof(NOTIFYICONDATA);
	systemTrayData.uID         = 0x0923;
	systemTrayData.hWnd        = hWnd;
	systemTrayData.uFlags      = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	systemTrayData.dwInfoFlags = NIIF_INFO;
	systemTrayData.uVersion    = NOTIFYICON_VERSION_4;
	wcsncpy_s(systemTrayData.szInfoTitle, 64, L"GW2-DiscordLink", 64);
	systemTrayData.uCallbackMessage = WM_TRAYICON;
	systemTrayData.hIcon = (HICON)LoadImage(hInstance,
						 MAKEINTRESOURCE(IDI_ICON1),
						 IMAGE_ICON,
						 GetSystemMetrics(SM_CXSMICON),
						 GetSystemMetrics(SM_CYSMICON),
						 LR_DEFAULTCOLOR);
	
	Shell_NotifyIcon(NIM_ADD, &systemTrayData);

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

		if (config["gw2-api-key"].get<std::string>().empty()) {
			MessageBox(hWnd, L"Without a API-Key only the core professions are shown!", L"GW2-DiscordLink Warning", MB_ICONWARNING | MB_OK);
			std::cout << "Warning: Without a API-Key only the core professions are shown!" << std::endl;
			querySpecs = false;
		}

		professions = json::parse(str);
	}

	// Init RPC & Mumble link
	// Guard with try to give the user an error and not crash silently 
	try {
		rpc = new RpcClient(config["lang"].get<std::string>(), config["gw2-api-key"].get<std::string>());
		mLink = new Gw2MumbleLink();
	} catch (const std::runtime_error *error) {
		MessageBoxA(hWnd, error->what(), "GW2-DiscordLink Error", MB_ICONERROR | MB_OK);
		return EXIT_FAILURE;
	}

	std::cout << "Connecting to Discord ..." << std::endl;
	const std::string DISCORD_API_KEY = config["discord-api-key"];

	// Initalize Discord API:
	memset(&discordHandlers, 0, sizeof(discordHandlers));
	discordHandlers.ready = discordCallback_Ready;
	discordHandlers.errored = discordCallback_Error;
	discordHandlers.disconnected = discordCallback_Disconnected;
	Discord_Initialize(DISCORD_API_KEY.c_str(), &discordHandlers, false, nullptr);

	// run Discord Update in background thread
	std::thread workerThread(&worker, querySpecs, std::ref(professions));

	// Enter Message loop for UI
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Wait for worker to exit program
	running = false;
	workerThread.join();

	std::cout << "Cleaning up ..." << std::endl;

	// Cleanup 
	Discord_Shutdown();
	delete rpc;
	delete mLink;
	apiCache.clear();

	return (int)msg.wParam;
}

// Callback for Window Events
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_TRAYICON:
		switch (lParam) {
		case WM_LBUTTONDBLCLK: break;
		case WM_LBUTTONUP:break;
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			// Show the context menu:
			POINT cursor;
			GetCursorPos(&cursor);

			// Don't ask why: 
			// https://docs.microsoft.com/de-at/windows/desktop/api/winuser/nf-winuser-trackpopupmenu 
			SetForegroundWindow(hWnd);

			UINT clicked = TrackPopupMenu(
				contextMenu,
				TPM_RIGHTALIGN,
				cursor.x,
				cursor.y,
				0,
				hWnd,
				NULL
			);
			
			PostMessage(hWnd, WM_NULL, 0, 0);

			break;
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_TRAY_EXIT_CONTEXT_MENU_ITEM:
			DestroyWindow(hWnd);
		}
		break;

	case WM_CLOSE:
		if (systemTrayData.hIcon != 0) {
			DeleteObject(systemTrayData.hIcon);
			systemTrayData.hIcon = 0;
		}
		break;

	case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &systemTrayData);
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return FALSE;
}

void worker(bool querySpecs, json &professions) {
	running = true;
	
	// Init memory for Discord Rich Presence 
	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));
	discordPresence.largeImageKey = "gw2-logo";

	// Somehow the Thread does not copy these values so they must be
	// readable until we update it again
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
				std::cout << "Disabling querying of character specializations, could not fetch with given API_KEY" << std::endl;
				querySpecs = false;
			} else {
				std::string specType = MAP_TYPE_PVE;

				// Choose right specialization from the list according to maptype
				switch ((Gw2MumbleLink::MapType)mLink->getMumbleContext()->mapType) {
				case Gw2MumbleLink::PvE:
					break;

				case Gw2MumbleLink::PvP: 
					specType = MAP_TYPE_PVP;
					break;

				case Gw2MumbleLink::BlueBorderlands:
				case Gw2MumbleLink::RedBorderlands:
				case Gw2MumbleLink::GreenBorderlands:
				case Gw2MumbleLink::EternalBattlegrounds:
				case Gw2MumbleLink::EotM:
					specType = MAP_TYPE_WVW; 
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

