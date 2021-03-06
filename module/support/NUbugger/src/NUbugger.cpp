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

#include "message/vision/LookUpTable.h"
#include "message/support/Configuration.h"
#include "message/support/nubugger/proto/Ping.pb.h"
#include "message/support/nubugger/proto/ReactionHandles.pb.h"
#include "message/support/nubugger/proto/Command.pb.h"
#include "message/support/nubugger/proto/ConfigurationState.pb.h"
#include "message/vision/proto/LookUpTable.pb.h"

#include "utility/nubugger/NUhelpers.h"
#include "utility/time/time.h"
#include "utility/math/angle.h"
#include "utility/math/coordinates.h"

namespace module {
namespace support {

    using utility::nubugger::graph;

    using message::support::Configuration;
    using message::support::nubugger::proto::Ping;
    using message::support::nubugger::proto::ReactionHandles;
    using message::support::nubugger::proto::Command;
    using message::support::nubugger::proto::ConfigurationState;

    using LookUpTableProto = message::vision::proto::LookUpTable;

    using message::vision::LookUpTable;
    using message::vision::SaveLookUpTable;
    using message::vision::Colour;

    using message::support::SaveConfiguration;

    using utility::time::getUtcTimestamp;
    using utility::time::durationFromSeconds;

    // Flag struct to upload a lut
    struct UploadLUT {};

    NUbugger::NUbugger(std::unique_ptr<NUClear::Environment> environment)
        : Reactor(std::move(environment))
        , max_image_duration()
        , max_classified_image_duration()
        , handles()
        , dataPointFilterIds()
        , overview()
        , actionRegisters()
        , outputFile()
        , networkMutex()
        , fileMutex() {

        // These go first so the config can do things with them
        provideOverview();
        provideDataPoints();
        provideDrawObjects();
        provideSubsumption();
        provideGameController();
        provideLocalisation();
        provideReactionStatistics();
        provideSensors();
        provideVision();

        on<Configuration>("NUbugger.yaml").then([this] (const Configuration& config) {

            max_image_duration = durationFromSeconds(1.0 / config["output"]["network"]["max_image_fps"].as<double>());
            max_classified_image_duration = durationFromSeconds(1.0 / config["output"]["network"]["max_classified_image_fps"].as<double>());

            // If we are using the network
            networkEnabled = config["output"]["network"]["enabled"].as<bool>();

            // If we are using files and haven't set one up yet
            if (!fileEnabled && config["output"]["file"]["enabled"].as<bool>()) {

                // Lock the file
                std::lock_guard<std::mutex> lock(fileMutex);

                // Get our timestamp
                std::string timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(NUClear::clock::now().time_since_epoch()).count());

                // Open a file using the file name and timestamp
                outputFile.close();
                outputFile.clear();
                outputFile.open(config["output"]["file"]["path"].as<std::string>()
                                + "/"
                                + timestamp
                                + ".nbs", std::ios::binary);

                fileEnabled = true;
            }
            else if (fileEnabled && !config["output"]["file"]["enabled"].as<bool>()) {

                // Lock the file
                std::lock_guard<std::mutex> lock(fileMutex);

                // Close the file
                outputFile.close();
                fileEnabled = false;
            }

            for (auto& setting : config["reaction_handles"]) {
                // Lowercase the name
                std::string name = setting.first.as<std::string>();
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                bool enabled = setting.second.as<bool>();

                bool found = false;
                for (auto& handle : handles[name]) {
                    if (enabled && !handle.enabled()) {
                        handle.enable();
                        found = true;
                    }

                    else if (!enabled && handle.enabled()) {
                        handle.disable();
                        found = true;
                    }
                }

                if (found) {
                    if (enabled) {
                        log<NUClear::INFO>("Enabled:", name);
                    } else {
                        log<NUClear::INFO>("Disabled:", name);
                    }
                }
            }
        });

        on<Every<1, Per<std::chrono::seconds>>, Single, Priority::LOW>().then([this] {
            // Send a ping message
            send(Ping(), 0, false, NUClear::clock::time_point());
        });

        on<Trigger<UploadLUT>, With<LookUpTable>>().then([this](const LookUpTable& lut) {

            LookUpTableProto lutMessage;

            lutMessage.set_table(lut.getData());
            lutMessage.set_bits_y(lut.BITS_Y);
            lutMessage.set_bits_cb(lut.BITS_CB);
            lutMessage.set_bits_cr(lut.BITS_CR);

            send(lutMessage);
        });

        on<Network<Command>>().then("Network Command", [this](const Command& message) {
            std::string command = message.command();
            log<NUClear::INFO>("Received command:", command);
            if (command == "download_lut") {

                // Emit something to make it upload the lut
                emit<Scope::DIRECT>(std::make_unique<UploadLUT>());
            } else if (command == "get_configuration_state") {
                sendConfigurationState();
            } else if (command == "get_reaction_handles") {
                sendReactionHandles();
            } else if (command == "get_subsumption") {
                sendSubsumption();
            }
        });

        on<Network<LookUpTableProto>>().then([this](const LookUpTableProto& lookuptable) {

            const std::string& lutData = lookuptable.table();

            log<NUClear::INFO>("Loading LUT");
            std::vector<message::vision::Colour> data;
            data.reserve(lutData.size());
            for (auto& s : lutData) {
                data.push_back(message::vision::Colour(s));
            }
            auto lut = std::make_unique<LookUpTable>(lookuptable.bits_y(), lookuptable.bits_cb(), lookuptable.bits_cr(), std::move(data));
            emit<Scope::DIRECT>(std::move(lut));

            if (lookuptable.save()) {
                log<NUClear::INFO>("Saving LUT to file");
                emit<Scope::DIRECT>(std::make_unique<SaveLookUpTable>());
            }
        });

        on<Network<ReactionHandles>>().then([this](const NetworkSource& /*source*/, const ReactionHandles& /*command*/) {
            // auto config = std::make_unique<SaveConfiguration>();
            // config->config = currentConfig->config;

            // for (const auto& command : message.reaction_handles().handles()) {

            //     Message::Type type = command.type();
            //     bool enabled = command.enabled();
            //     std::string key = getStringFromMessageType(type);

            //     std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            //     config->path = CONFIGURATION_PATH;
            //     config->config["reaction_handles"][key] = enabled;
            //     for (auto& handle : handles[type]) {
            //         handle.enable(enabled);
            //     }
            // }

            // emit(std::move(config));
        });

        on<Network<ConfigurationState>>().then([this](const ConfigurationState& state) {
            recvConfigurationState(state);
        });

        sendReactionHandles();

        // When we shutdown, close our publisher and our file if we have one
        on<Shutdown>().then([this] {

            // Close the file if it exists
            fileEnabled = false;
            outputFile.close();
        });

        on<Trigger<SaveConfiguration>>().then("Save Config",[this](const SaveConfiguration& config){
            saveConfigurationFile(config.path,config.config);
        });
    }

    void NUbugger::sendReactionHandles() {

        ReactionHandles reactionHandles;

        for (auto& handle : handles) {
            auto* objHandles = reactionHandles.add_handles();
            auto& value = handle.second;
            objHandles->set_type(handle.first);
            objHandles->set_enabled(value.empty() ? true : value.front().enabled());
        }

        send(reactionHandles, 0, true, NUClear::clock::now());
    }

} // support
} // modules
