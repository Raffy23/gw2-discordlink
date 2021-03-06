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
#define SLEEP_TIME       (1000)
#define LOADING_TICKS    (10 /* Number of sleep ticks until "Loading ..." is shown in discord */)
#define CLEAN_LINK_TICKS (60 /* Number of sleep ticks until MumbeLink memory is zeroed out    */)
#define ASSUME_DEAD_GAME (30 /* Number of error ticks until the game is assumed to be dead    */)

// Global variables for state
RpcClient *rpc;
Gw2MumbleLink *mLink;
std::atomic<bool> running;

// Cache for requests, so API must not be queried so often
std::map<std::string, RpcClient::Response> apiCache;

// Background thread function, so UI / Thread does no networking 
void worker_discordpresence(bool querySpecs, json &professions, HWND hWnd);
void worker_processwatcher(std::string path, std::string cmd, HWND hWnd);

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

#define WM_TRAYICON      (WM_USER + 0x100)
#define WM_MSG_DEAD_GAME (WM_USER + 0x101)
#define WM_MSG_PROC_EXIT (WM_USER + 0x102)
#define WM_MSG_EXE_404   (WM_USER + 0x103)
#define ID_TRAY_EXIT_CONTEXT_MENU_ITEM 0x1238
NOTIFYICONDATA systemTrayData;
HMENU contextMenu;

#define DIALOG_HEADER L"Gw2 - DiscordLink"

// WinMain Entry Point
int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow) {
	// Setup signal handler (only needed for console subsystem?)
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
	wcsncpy_s(systemTrayData.szTip, 64, L"GW2-DiscordLink", 64);
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
	bool startGw2 = false;
	{
		std::ifstream configFile(SETTINGS_FILE);
		std::string str;

		readFromFile(str, configFile);
		config = json::parse(str);

		std::ifstream profFile(std::string("./lang/") + config["lang"].get<std::string>() + "/profession.json");
		readFromFile(str, profFile);

		professions = json::parse(str);
	}

	// Process config
	if (config["gw2-api-key"].get<std::string>().empty()) {
		MessageBox(hWnd, L"Without a API-Key only the core professions are shown!", DIALOG_HEADER, MB_ICONWARNING | MB_OK);
		std::cout << "Warning: Without a API-Key only the core professions are shown!" << std::endl;
		querySpecs = false;
	}

	if (!config["gw2-path"].get<std::string>().empty()) {
		startGw2 = true; 
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
	std::thread *gw2ProcessWaiterThread = nullptr;
	if (startGw2) { 
		gw2ProcessWaiterThread = new std::thread(&worker_processwatcher, config["gw2-path"].get<std::string>(), config["gw2-cmd"].get<std::string>(), hWnd);
	}

	std::thread workerThread(&worker_discordpresence, querySpecs, std::ref(professions), hWnd);
	
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

	case WM_MSG_DEAD_GAME:
		switch (MessageBox(hWnd, L"Could not detect any activity!\n\nDo you still play Guild Wars 2?", DIALOG_HEADER, MB_ICONQUESTION | MB_YESNO)) {
		case IDNO:
			running = false;
			DestroyWindow(hWnd);
		}
		break;

	case WM_MSG_PROC_EXIT:
		DestroyWindow(hWnd);
		break;

	case WM_MSG_EXE_404:
		MessageBox(hWnd, L"The given path to the Gw2-64.exe is not valid!\nGw2-DiscordLink will now exit ...", DIALOG_HEADER, MB_ICONERROR | MB_OK);
		DestroyWindow(hWnd);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return FALSE;
}

void worker_discordpresence(bool querySpecs, json &professions, HWND hWnd) {
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

	unsigned short sameTicks = 0;
	bool zeroedMemory = true;

	while (running) {
		// Check if Mumble link is connected and wait for the game ...
		if (mLink->getIdentity().empty()) {
			sameTicks += 1;

			// waited ERROR_SLEEP_TIME * ASSUME_DEAD_GAME ticks until notifying 
			// the user
			if (sameTicks >= ASSUME_DEAD_GAME && zeroedMemory) {
				PostMessage(hWnd, WM_MSG_DEAD_GAME, 0, 0);
				sameTicks = 0;
			}

			Sleep(ERROR_SLEEP_TIME);
			continue;
		}
		
		// Check the UI tick, if tick is the same then Gw2 might not be updating
		DWORD tick = mLink->getUITick();
		if (tick == oldTick) {
			sameTicks += 1;

			// If ~10 sec the same UI, Gw2 might not be running or still loading
			// just zero out stuff, might need to search for process to determine 
			// if Gw2 is still running ...
			if (sameTicks == LOADING_TICKS) {
				oldMap = 0;

				discordPresence.details = "Loading ...";
				discordPresence.state = nullptr;
				discordPresence.smallImageKey = nullptr;
				discordPresence.smallImageText = nullptr;
				discordPresence.startTimestamp = std::time(nullptr);

				Discord_UpdatePresence(&discordPresence);

				Sleep(SLEEP_TIME);
				continue;
			} else if (sameTicks > LOADING_TICKS && sameTicks < CLEAN_LINK_TICKS) {
				Sleep(SLEEP_TIME);
				continue;
			} else if (sameTicks == CLEAN_LINK_TICKS) {
				Discord_ClearPresence();
				mLink->cleanupMumbeLinkMemory();

				zeroedMemory = true;
				sameTicks = 0;
				
				Sleep(SLEEP_TIME);
				continue;
			} else if (sameTicks > CLEAN_LINK_TICKS) {
				Sleep(SLEEP_TIME / 2);
				continue;
			}
			
		} else {
			sameTicks = 0;
			zeroedMemory = false;
		}

		oldTick = tick;

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

	// Clear Presence & wait so discord doesn't jump back 
	// to the normal Playing Guild Wars 2 label
	Discord_ClearPresence();
	Sleep(SLEEP_TIME);

}

void worker_processwatcher(std::string path, std::string cmd, HWND hWnd) {
	// Check if Gw2 executable exists in given path
	DWORD attrb = GetFileAttributesA(path.c_str());
	if (attrb == INVALID_FILE_ATTRIBUTES && (attrb & FILE_ATTRIBUTE_DIRECTORY)) {
		PostMessage(hWnd, WM_MSG_EXE_404, 0, 0);
		running = false;
		return;
	}

	// copy cmd to writeable buffer, cuz child process is allowed to 
	// write to this buffer
	char *cmdBuffer = new char[cmd.length() + 1];
	cmd.copy(cmdBuffer, cmd.length());
	cmdBuffer[cmd.length()] = '\0';
	
	STARTUPINFOA info = { sizeof(info) };
	PROCESS_INFORMATION processInfo;

	// Start Gw2.exe and wait until it is closed
	if (CreateProcessA(path.c_str(), cmdBuffer, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &info, &processInfo)) {
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}

	running = false;
	PostMessage(hWnd, WM_MSG_PROC_EXIT, 0, 0);
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

