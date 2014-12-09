/*
 * This file is part of the NUbots Codebase.
 *
 * The NUbots Codebase is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The NUbots Codebase is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the NUbots Codebase.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2013 NUBots <nubots@nubots.net>
 */

#include "NUbugger.h"

#include <yaml-cpp/yaml.h>
#include "utility/file/fileutil.h"

namespace modules {
namespace support {
    using utility::file::listFiles;

    void NUbugger::sendConfigurationState() {
        std::vector<std::string> paths = listFiles("config", true);
        for (auto&& path : paths) {
            log(path);
            YAML::Node node = YAML::LoadFile(path);
            log(node);
        }
        // TICK: search through config directory -> config/*.yaml accounting for sub-directories
        // parse yaml (load using yaml.cpp)
        // make protocol buffer tree and send over network to nubugger
    }
}
}