/*
 * This file is part of NUbots Codebase.
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
 * Copyright 2015 NUbots <nubots@nubots.net>
 */

#include "DOpE.h"

#include <armadillo>
#include <format.h>

#include "utility/support/proto_armadillo.h"
#include "messages/support/Configuration.h"
#include "messages/support/optimisation/DOpE.h"
#include "messages/support/optimisation/Episode.pb.h"
#include "messages/support/optimisation/Estimate.pb.h"

namespace modules {
namespace support {
namespace optimisation {

    using NUClear::message::NetworkJoin;
    using NUClear::message::NetworkLeave;
    using utility::math::optimisation::Optimiser;
    using utility::math::optimisation::PGAOptimiser;
    using messages::support::Configuration;
    using messages::support::optimisation::Episode;
    using messages::support::optimisation::Estimate;
    using messages::support::optimisation::RequestParameters;
    using messages::support::optimisation::Parameters;
    using messages::support::optimisation::RegisterOptimisation;

    void DOpE::sendEstimateUpdate(const Optimisation& opt, const std::string& target) {

        // Get the current state for this
        const auto& current = opt.optimiser->estimate();
        auto e = std::make_unique<Estimate>();

        // Set our group
        e->set_group(opt.group);

        // Add our generation
        e->set_generation(current.generation);

        // Add our values and covariance
        *e->mutable_values() << current.estimate;
        *e->mutable_covariance() << current.covariance;

        // Add our episodes
        for (auto& episode : opt.episodes) {
            *e->add_episode() = episode;
        }

        // Send it to the remote
        emit<Scope::NETWORK>(e, target, true);
    }

    void DOpE::saveOptimisationState(const Optimisation& opt) {
        // TODO save the optimisation
        // TODO merge the current yaml file with our new yaml file
        // TODO save the yaml file
        // TODO this will trigger the configuration which we don't wanna do
    }

    DOpE::DOpE(std::unique_ptr<NUClear::Environment> environment)
    : Reactor(std::move(environment)) {

        on<Configuration>("DOpE.yaml").then([this] (const Configuration& config) {
            // Use configuration here from file DOpE.yaml
            log("TODO load the configuration from any in progress optimisations here");

            // TODO load any optimisations that are currently in the config file (saved/in progress)
        });

        on<Trigger<NetworkJoin>>().then("Distrubute Initial Optimisation", [this] (const NetworkJoin& joiner) {

            log<NUClear::INFO>(fmt::format("{} ({}) joined the optimisation network", joiner.name, joiner.udpPort));

            for (auto& op : optimisations) {
                // If this is a network optimisation
                if(op.second.network) {
                    // Send it to our joiner
                    sendEstimateUpdate(op.second, joiner.name);
                }
            }
        });

        on<Network<Estimate>>().then("Network Estimate", [this] (const NetworkSource& src, const Estimate& estimate) {

            log<NUClear::INFO>(fmt::format("Estimate {} gen {} received from {}", estimate.group(), estimate.generation(), src.name));

            auto el = optimisations.find(estimate.group());
            if (el != optimisations.end()) {
                auto& opt = el->second;

                // If we are doing a network optimisation
                if (opt.network) {

                    bool updated = false;
                    if(opt.optimiser->estimate().generation < estimate.generation()) {
                        bool updated = true;
                        // TODO This is a new estimate, use this
                        // TODO also check if we have any episodes that are valid and not in this episodes data
                        // TODO also save this best estimate in the config
                    }
                    else if (opt.optimiser->estimate().generation == estimate.generation()) {
                        // TODO This is the same as our existing generation
                        // TODO Find some arbritrary way to work out which is better
                        // TODO work out if any of our episodes
                    }

                    // If we updated, save our estimate in the config
                    if(updated) {
                        // Save our optimisation state to the config file
                        saveOptimisationState(opt);
                    }
                }
            }
            else {
                // TODO should we add this? or ignore it?
            }
        });

        on<Network<Episode>>().then("Network Episode", [this] (const NetworkSource& src, const Episode& episode) {

            log<NUClear::INFO>(fmt::format("Episode for {} gen {} received from {}", episode.group(), episode.generation(), src.name));

            // If we have this optimisation
            auto el = optimisations.find(episode.group());
            if (el != optimisations.end()) {
                auto& opt = el->second;

                // If this optimiser works on the network
                if (opt.network) {

                    // If we don't already have this episode and it is valid for our optimiser
                    if(//std::find(opt.episodes.begin(), opt.episodes.end(), episode) == opt.episodes.end()
                       opt.optimiser->validSample(episode)) {

                        opt.episodes.push_back(episode);
                        if (opt.episodes.size() == opt.batchSize) {

                            arma::vec fitnesses(opt.episodes.size());
                            arma::mat samples(opt.episodes.size(), opt.optimiser->estimate().estimate.n_rows);

                            for (uint i = 0; i < opt.episodes.size(); ++i) {

                                // Make our combined fitness
                                fitnesses[i] = 0;
                                for(auto& f : opt.episodes[i].fitness()) {
                                    fitnesses[i] += f.fitness() * f.weight();
                                }

                                for (uint j = 0; j < samples.n_cols; ++j) {
                                    samples(i, j) = opt.episodes[i].values().v(j);
                                }
                            }

                            // Update our optimiser
                            opt.optimiser->updateEstimate(samples, fitnesses);

                            // Clear our episodes list
                            opt.episodes.clear();

                            // Emit our new best estimate over the network
                            sendEstimateUpdate(opt);
                        }

                        // Save our optimisation state to the config file
                        saveOptimisationState(opt);
                    }
                }
            }
        });

        on<Trigger<Episode>>().then("Local Episode", [this] (const Episode& episode) {

            // If we have this optimisation
            auto el = optimisations.find(episode.group());
            if (el != optimisations.end()) {
                auto& opt = el->second;

                // If this episode is valid for our optimiser
                if(opt.optimiser->validSample(episode)) {

                    opt.episodes.push_back(episode);

                    if (opt.episodes.size() == opt.batchSize) {
                        arma::vec fitnesses(opt.episodes.size());
                        arma::mat samples(opt.episodes.size(), opt.optimiser->estimate().estimate.n_rows);

                        for (uint i = 0; i < opt.episodes.size(); ++i) {

                            // Make our combined fitness
                            fitnesses[i] = 0;
                            for(auto& f : opt.episodes[i].fitness()) {
                                fitnesses[i] += f.fitness() * f.weight();
                            }

                            for (uint j = 0; j < samples.n_cols; ++j) {
                                samples(i, j) = opt.episodes[i].values().v(j);
                            }
                        }

                        // Update our optimiser
                        opt.optimiser->updateEstimate(samples, fitnesses);

                        // Clear our episodes list
                        opt.episodes.clear();

                        if (opt.network) {
                            // Emit our new best estimate over the network
                            sendEstimateUpdate(opt);
                        }
                    }
                    // If we are networked send out this episode
                    else if (opt.network) {
                        auto e = std::make_unique<Episode>(episode);
                        emit<Scope::NETWORK>(e, "", true);
                    }

                    // Save our optimisation state to the config file
                    saveOptimisationState(opt);
                }
            }
            else {
                // If we don't have an optimiser for this, this is an error
                log<NUClear::ERROR>(fmt::format("Episode for {} gen {} generated for unregistered optimisation", episode.group(), episode.generation()));
            }
        });

        on<Trigger<RegisterOptimisation>>().then("Register Optimisation", [this] (const RegisterOptimisation& optimisation) {
            // Add this optimisation to the list
            auto item = optimisations.find(optimisation.group);
            if (item == optimisations.end()) {
                // Add this new optimisation
                log("Adding a new optimisation for", optimisation.group);

                optimisations[optimisation.group] = Optimisation {
                    optimisation.group,
                    optimisation.network,
                    optimisation.parameters.batchSize,
                    std::make_unique<PGAOptimiser>(optimisation.parameters),
                    std::vector<Episode>()
                };
            }
            else {
                // TODO this may already have been loaded via either the config file, or the network

                // Check if the size of the vectors are the same
                // If they are not, log an error here saying that another
                // Optimisation with the same name was already registered
                log<NUClear::ERROR>("The optimisation,", optimisation.group, "was already registered as a different type");
            }
        });

        on<Trigger<RequestParameters>>().then("Request Optimisation Parameters", [this] (const RequestParameters& request) {

            auto el = optimisations.find(request.group);
            if (el != optimisations.end()) {
                auto& opt = el->second;

                auto p = std::make_unique<Parameters>();

                p->group = request.group;
                p->generation = opt.optimiser->estimate().generation;
                p->samples = opt.optimiser->getSamples(request.nSamples);
                p->covariance = opt.optimiser->estimate().covariance;

                emit(p);
            }
            else {
                log<NUClear::ERROR>("The optimisation,", request.group, "was requested but does not exist");
            }
        });
    }
}
}
}
