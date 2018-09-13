GW2-DiscordLink
===============

GW2-DiscordLink is a [Discord Rich Presence](https://discordapp.com/rich-presence) utility program for Guild Wars 2.

## How does it work
GW2-DiscordLink does utilize the [MumbleLink API](https://wiki.guildwars2.com/wiki/API:MumbleLink) and the official
[GW2-API](https://wiki.guildwars2.com/wiki/API:2) to gather Information about the current Game state (Playername, Mapname, Profession, Specialization).

*Note:* Due the limitation of MumbleLink the Specializations can only be queried over the GW2-API which does
require a API-Key with at least the `builds` right to function properly. 
(It's still possible to use this GW2-DiscordLink with no API-Key)

## Features
* Display character name and current map
* Display how long the Character stayed on the map
* Display profession / specialization icon besides GW2-Logo

## Configure
Create the `settings.json` like below and place it right along the .exe file
```json
{
	"discord-api-key": "450300626241454082",
	"lang": "en",

	"gw2-path": "C:\\Program Files (x86)\\Guild Wars 2\\Gw2-64.exe",
	"gw2-api-key": "00000000-0000-0000-0000-00000000000000000000-0000-0000-0000-000000000000",
	"gw2-cmd": "-maploadinfo"
}
```

* `discord-api-key`: This is the key for Discord Rich Presence
* `lang`: For language, see below which languages are supported
* `gw2-path`: Can either be empty (`""`) or the path to your Gw2-64.exe which only needed if Guild Wars 2 should be started with this program and Gw2-DiscordLink should exit immediately after Guild Wars 2 was exited.
* `gw2-cmd`: Additional [program arguments](https://wiki.guildwars2.com/wiki/Command_line_arguments) for Guild Wars 2
* `gw2-api-key`: Can either be empty (`""`) or a valid API Key for Guild Wars 2

### Supported Languages:
* **English** (en)
* **Deutsch** (de)
* **Français** (fr)
* **Español** (es)

## Dependencies
* [libcurl](https://curl.haxx.se/libcurl/)
* [discord-rpc](https://github.com/discordapp/discord-rpc)

## How to build
1. Build [libcurl](https://curl.haxx.se/download.html) into `./libcurl-x64-release-static`
2. Build discord-rpc or download the [pre-compiled binaries](https://github.com/discordapp/discord-rpc/releases) into `./discord-rpc-win64-dynamic`
3. Build Visual Studio Project
