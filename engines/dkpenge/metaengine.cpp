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

#include "dkpenge/dkpenge.h"
#include "dkpenge/detection.h"

class DKPengeMetaEngine : public AdvancedMetaEngine<ADGameDescription> {
public:
	const char *getName() const override {
		return "dkpenge";
	}

	Common::Error createInstance(OSystem *syst, Engine **engine, const ADGameDescription *desc) const override {
		*engine = new DKPenge::DKPengeEngine(syst, desc);
		return Common::kNoError;
	}

	bool hasFeature(MetaEngineFeature f) const override {
		return f == kSavesUseExtendedFormat || f == kSimpleSavesNames || f == kSupportsListSaves ||
		       f == kSupportsDeleteSave || f == kSavesSupportMetaInfo || f == kSavesSupportCreationDate ||
		       f == kSavesSupportPlayTime || f == kSupportsLoadingDuringStartup;
	}
};

#if PLUGIN_ENABLED_DYNAMIC(DKPENGE)
	REGISTER_PLUGIN_DYNAMIC(DKPENGE, PLUGIN_TYPE_ENGINE, DKPengeMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(DKPENGE, PLUGIN_TYPE_ENGINE, DKPengeMetaEngine);
#endif
