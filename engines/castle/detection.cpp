/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "base/plugins.h"
#include "engines/advancedDetector.h"

#include "castle/detection.h"

namespace Castle {

const PlainGameDescriptor castleGames[] = {
	{ "castle", "Castle Explorer" },
	{ nullptr, nullptr }
};

const ADGameDescription gameDescriptions[] = {
	// Castle Explorer, UK version 1.0 (DK Multimedia, 1996)
	{
		"castle",
		"UK Version 1.0",
		AD_ENTRY2s("CASTLE.PNG", "676b3c8bcb91667231ed5ea49574a7fd", 1738548,
		           "CASTLE.EXE", "914344ea78f82b685782ec628a019df6", 4420096),
		Common::EN_GRB,
		Common::kPlatformWindows,
		ADGF_UNSTABLE,
		GUIO1(GUIO_NOMIDI)
	},

	AD_TABLE_END_MARKER
};

static const DebugChannelDef debugFlagList[] = {
	{ kDebugGeneral, "general", "General debug level" },
	{ kDebugDatabase, "database", "Page database loading" },
	{ kDebugScript, "script", "Actions and scripts" },
	{ kDebugGraphics, "graphics", "Graphics and rendering" },
	{ kDebugSound, "sound", "Sound and music" },
	DEBUG_CHANNEL_END
};

static const char *const directoryGlobs[] = {
	"dkcode",
	nullptr
};

} // End of namespace Castle

class CastleMetaEngineDetection : public AdvancedMetaEngineDetection<ADGameDescription> {
public:
	CastleMetaEngineDetection() : AdvancedMetaEngineDetection(Castle::gameDescriptions, Castle::castleGames) {
		_maxScanDepth = 2;
		_directoryGlobs = Castle::directoryGlobs;
	}

	const char *getName() const override {
		return "castle";
	}

	const char *getEngineName() const override {
		return "Castle Explorer";
	}

	const char *getOriginalCopyright() const override {
		return "Castle Explorer (C) 1996 Dorling Kindersley Multimedia";
	}

	const DebugChannelDef *getDebugChannels() const override {
		return Castle::debugFlagList;
	}
};

REGISTER_PLUGIN_STATIC(CASTLE_DETECTION, PLUGIN_TYPE_ENGINE_DETECTION, CastleMetaEngineDetection);
