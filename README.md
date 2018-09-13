GW2-DiscordLink
===============

GW2-DiscordLink is a [Discord Rich Presence](https://discordapp.com/rich-presence) utility program for Guild Wars 2.

## How does it work
GW2-DiscordLink does utilize the [MumbleLink API](https://wiki.guildwars2.com/wiki/API:MumbleLink) and the official
[GW2-API](https://wiki.guildwars2.com/wiki/API:2) to gather information about the current game state (Playername, Mapname, Profession, Specialization).

*Note:* Due the limitation of MumbleLink API the specializations can only be queried over the GW2-API which does
require a API-Key with at least the `builds` right to function properly. 
(It's still possible to use this GW2-DiscordLink with no API-Key, but only core professions are shown when you play the game)

## Features
* Display character name and current map
* Display how long the Character stayed on the map
* Display profession / specialization icon besides Guild Wars 2 Logo

## Configure
Create or modify the `settings.json` like below and place it right along the .exe file
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

*(Currently a language must also be supported by the GW2-API if specializations are used.)*

## Dependencies
* [libcurl](https://curl.haxx.se/libcurl/)
* [discord-rpc](https://github.com/discordapp/discord-rpc)

## How to build
1. Build [libcurl](https://curl.haxx.se/download.html) into `./libcurl-x64-release-static`
2. Build discord-rpc or download the [pre-compiled binaries](https://github.com/discordapp/discord-rpc/releases) into `./discord-rpc-win64-dynamic`
3. Build Visual Studio Project

### Use your own Discord Account
1. Open the [Discord Developer Portal](https://discordapp.com/developers/applications/)
2. Create a new Application, the `CLIENT ID` is the `discord-api-key` in the `settings.json` file.
3. Activate Discord Rich Presence for the application
4. Under `Rich Presence > Art Assets` add the logos for the professions: 

| Asset Name    | Usage               | Resource (Link)								|
| ------------- | ------------------- | ----------------------------------------------------------------------  |
| gw2-logo 		| Guild Wars 2 Logo   | |
| profession_1	| Guardian			  | https://wiki.guildwars2.com/wiki/File:Guardian_tango_icon_200px.png |
| profession_2	| Warrior			  | https://wiki.guildwars2.com/wiki/File:Warrior_tango_icon_200px.png |
| profession_3  | Engineer			  | https://wiki.guildwars2.com/wiki/File:Engineer_tango_icon_200px.png |
| profession_4  | Ranger			  | https://wiki.guildwars2.com/wiki/File:Ranger_tango_icon_200px.png |
| profession_5  | Thief			      | https://wiki.guildwars2.com/wiki/File:Thief_tango_icon_200px.png |
| profession_6  | Elementalist		  | https://wiki.guildwars2.com/wiki/File:Elementalist_tango_icon_200px.png |
| profession_7  | Mesmer			  | https://wiki.guildwars2.com/wiki/File:Mesmer_tango_icon_200px.png
| profession_8  | Necromancer		  | https://wiki.guildwars2.com/wiki/File:Necromancer_tango_icon_200px.png
| profession_9  | Revenant			  | https://wiki.guildwars2.com/wiki/File:Revenant_tango_icon_200px.png
| spec_18  		| Berserker			  | https://wiki.guildwars2.com/wiki/File:Berserker_tango_icon_200px.png
| spec_27  		| Dragonhunter		  | https://wiki.guildwars2.com/wiki/File:Dragonhunter_tango_icon_200px.png
| spec_34  		| Reaper			  | https://wiki.guildwars2.com/wiki/File:Reaper_tango_icon_200px.png
| spec_40  		| Chronomancer		  | https://wiki.guildwars2.com/wiki/File:Chronomancer_tango_icon_200px.png
| spec_43  		| Scrapper			  | https://wiki.guildwars2.com/wiki/File:Scrapper_tango_icon_200px.png
| spec_48 		| Tempest			  | https://wiki.guildwars2.com/images/9/90/Tempest_tango_icon_200px.png
| spec_5  		| Druid			  	  | https://wiki.guildwars2.com/wiki/File:Druid_tango_icon_200px.png
| spec_52  		| Herald			  | https://wiki.guildwars2.com/wiki/File:Herald_tango_icon_200px.png
| spec_55  		| Soulbeast			  | https://wiki.guildwars2.com/wiki/File:Soulbeast_tango_icon_200px.png
| spec_56  		| Weaver			  | https://wiki.guildwars2.com/wiki/File:Weaver_tango_icon_200px.png
| spec_57  		| Holosmith			  | https://wiki.guildwars2.com/wiki/File:Holosmith_tango_icon_200px.png
| spec_58  		| Deadeye			  | https://wiki.guildwars2.com/wiki/File:Deadeye_tango_icon_200px.png
| spec_59  		| Mirage			  | https://wiki.guildwars2.com/wiki/File:Mirage_tango_icon_200px.png
| spec_60  		| Scourge			  | https://wiki.guildwars2.com/wiki/File:Scourge_tango_icon_200px.png
| spec_61  		| Spellbreaker	      | https://wiki.guildwars2.com/wiki/File:Spellbreaker_tango_icon_200px.png
| spec_62  		| Firebrand			  | https://wiki.guildwars2.com/wiki/File:Firebrand_tango_icon_200px.png
| spec_63  		| Renegade			  | https://wiki.guildwars2.com/wiki/File:Renegade_tango_icon_200px.png
| spec_7  		| Daredevil			  | https://wiki.guildwars2.com/wiki/File:Daredevil_tango_icon_200px.png
	

