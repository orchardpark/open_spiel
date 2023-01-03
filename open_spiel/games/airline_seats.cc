// Copyright 2019 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/games/airline_seats.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/observer.h"
#include "open_spiel/policy.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
    namespace airline_seats {
        namespace {

// Default parameters.
            constexpr int kDefaultPlayers = 2;
            constexpr int kDefaultRandom = 20;
            constexpr int kDefaultPower = 50;
            constexpr int kC0 = 36;
            constexpr int kMaxRounds = 10;

// Facts about the game
            const GameType kGameType{/*short_name=*/"airline_seats",
                    /*long_name=*/"Airline Seats",
                                                    GameType::Dynamics::kSequential,
                                                    GameType::ChanceMode::kSampledStochastic,
                                                    GameType::Information::kImperfectInformation,
                                                    GameType::Utility::kGeneralSum,
                                                    GameType::RewardModel::kRewards,
                    /*max_num_players=*/4,
                    /*min_num_players=*/2,
                    /*provides_information_state_string=*/true,
                    /*provides_information_state_tensor=*/true,
                    /*provides_observation_string=*/true,
                    /*provides_observation_tensor=*/true,
                    /*parameter_specification=*/
                                                    {{"players", GameParameter(kDefaultPlayers)}},
                    /*default_loadable=*/true,
                    /*provides_factored_observation_string=*/true,
            };

            std::shared_ptr<const Game> Factory(const GameParameters &params) {
                return std::shared_ptr<const Game>(new AirlineSeatsGame(params));
            }

            REGISTER_SPIEL_GAME(kGameType, Factory);
        }  // namespace

        class AirlineSeatsObserver : public Observer {
        public:
            AirlineSeatsObserver(IIGObservationType iig_obs_type)
                    : Observer(/*has_string=*/true, /*has_tensor=*/true),
                      iig_obs_type_(iig_obs_type) {}

            void WriteTensor(const State &observed_state, int player,
                             Allocator *allocator) const override {
                const AirlineSeatsState &state =
                        open_spiel::down_cast<const AirlineSeatsState &>(observed_state);
                SPIEL_CHECK_GE(player, 0);
                SPIEL_CHECK_LT(player, state.num_players_);
                const int num_players = state.num_players_;
                const int num_cards = num_players + 1;

                if (iig_obs_type_.private_info == PrivateInfoType::kSinglePlayer) {
                    {  // Observing player.
                        auto out = allocator->Get("player", {num_players});
                        out.at(player) = 1;
                    }
                    {  // The player's card, if one has been dealt.
                        auto out = allocator->Get("private_card", {num_cards});
                        if (state.history_.size() > player)
                            out.at(state.history_[player].action) = 1;
                    }
                }

                // Betting sequence.
                if (iig_obs_type_.public_info) {
                    if (iig_obs_type_.perfect_recall) {
                        auto out = allocator->Get("betting", {2 * num_players - 1, 2});
                        for (int i = num_players; i < state.history_.size(); ++i) {
                            out.at(i - num_players, state.history_[i].action) = 1;
                        }
                    } else {
                        auto out = allocator->Get("pot_contribution", {num_players});
                        for (auto p = Player{0}; p < state.num_players_; p++) {
                            out.at(p) = state.ante_[p];
                        }
                    }
                }
            }

            std::string StringFrom(const State &observed_state,
                                   int player) const override {
                const AirlineSeatsState &state =
                        open_spiel::down_cast<const AirlineSeatsState &>(observed_state);
                SPIEL_CHECK_GE(player, 0);
                SPIEL_CHECK_LT(player, state.num_players_);
                std::string result;

                // Private card
                if (iig_obs_type_.private_info == PrivateInfoType::kSinglePlayer) {
                    if (iig_obs_type_.perfect_recall || iig_obs_type_.public_info) {
                        if (state.history_.size() > player) {
                            absl::StrAppend(&result, state.history_[player].action);
                        }
                    } else {
                        if (state.history_.size() == 1 + player) {
                            absl::StrAppend(&result, "Received card ",
                                            state.history_[player].action);
                        }
                    }
                }

                // Betting.
                // TODO(author11) Make this more self-consistent.
                if (iig_obs_type_.public_info) {
                    if (iig_obs_type_.perfect_recall) {
                        // Perfect recall public info.
                        for (int i = state.num_players_; i < state.history_.size(); ++i)
                            result.push_back(state.history_[i].action ? 'b' : 'p');
                    } else {
                        // Imperfect recall public info - two different formats.
                        if (iig_obs_type_.private_info == PrivateInfoType::kNone) {
                            if (state.history_.empty()) {
                                absl::StrAppend(&result, "start game");
                            } else if (state.history_.size() > state.num_players_) {
                                absl::StrAppend(&result,
                                                state.history_.back().action ? "Bet" : "Pass");
                            }
                        } else {
                            if (state.history_.size() > player) {
                                for (auto p = Player{0}; p < state.num_players_; p++) {
                                    absl::StrAppend(&result, state.ante_[p]);
                                }
                            }
                        }
                    }
                }

                // Fact that we're dealing a card.
                if (iig_obs_type_.public_info &&
                    iig_obs_type_.private_info == PrivateInfoType::kNone &&
                    !state.history_.empty() &&
                    state.history_.size() <= state.num_players_) {
                    int currently_dealing_to_player = state.history_.size() - 1;
                    absl::StrAppend(&result, "Deal to player ", currently_dealing_to_player);
                }
                return result;
            }

        private:
            IIGObservationType iig_obs_type_;
        };

        AirlineSeatsState::AirlineSeatsState(std::shared_ptr<const Game> game)
                : State(game),
                  winner_(kInvalidPlayer),
                  round_(1),
                  pnl_(num_players_),
                  seats_(num_players_),
                  phase_(GamePhase::DemandSimulation)
                  {}

        bool AirlineSeatsState::IsTerminal() const { return round_ > kMaxRounds || std::accumulate(seats_.begin(), seats_.end(), 0) == 0; }

        std::unique_ptr<State> AirlineSeatsState::Clone() const {
            return std::unique_ptr<State>(new AirlineSeatsState(*this));
        }

        std::vector<std::pair<Action, double>> AirlineSeatsState::ChanceOutcomes() const {
            SPIEL_CHECK_TRUE(IsChanceNode());
            // return a dummy action with probability 1
            return {{0, 1.0}};
        }

        std::string AirlineSeatsState::ActionToString(Player player, Action move) const {
            
        }

        AirlineSeatsGame::AirlineSeatsGame(const GameParameters &params)
                : Game(kGameType, params), num_players_(ParameterValue<int>("players")) {
            SPIEL_CHECK_GE(num_players_, kGameType.min_num_players);
            SPIEL_CHECK_LE(num_players_, kGameType.max_num_players);
            default_observer_ = std::make_shared<AirlineSeatsObserver>(kDefaultObsType);
            info_state_observer_ = std::make_shared<AirlineSeatsObserver>(kInfoStateObsType);
            private_observer_ = std::make_shared<AirlineSeatsObserver>(
                    IIGObservationType{/*public_info*/false,
                            /*perfect_recall*/false,
                            /*private_info*/PrivateInfoType::kSinglePlayer});
            public_observer_ = std::make_shared<AirlineSeatsObserver>(
                    IIGObservationType{/*public_info*/true,
                            /*perfect_recall*/false,
                            /*private_info*/PrivateInfoType::kNone});
        }

        std::unique_ptr<State> AirlineSeatsGame::NewInitialState() const {
            return std::unique_ptr<State>(new AirlineSeatsState(shared_from_this()));
        }

        std::shared_ptr<Observer> AirlineSeatsGame::MakeObserver(
                absl::optional<IIGObservationType> iig_obs_type,
                const GameParameters &params) const {
            if (!params.empty()) SpielFatalError("Observation params not supported");
            return std::make_shared<AirlineSeatsObserver>(iig_obs_type.value_or(kDefaultObsType));
        }

        int AirlineSeatsGame::MaxChanceOutcomes() const {
            // Implicit stochastic
            return 1;
        }

        int AirlineSeatsGame::NumPlayers() const {
            return num_players_;
        }

        int AirlineSeatsGame::MaxGameLength() const {
            return num_players_ * kMaxRounds + num_players_;
        }

        int AirlineSeatsGame::NumDistinctActions() const {
            // hardcoded for now, 5 buy qty and 5 price setting
            return 10;
        }

        int AirlineSeatsGame::MaxChanceNodesInHistory() const {
            return kMaxRounds;
        }

        std::vector<int> AirlineSeatsGame::InformationStateTensorShape() const {
            // player turn + seats at beginning + c_11 + c_12 + c1 + seats sold (or bought) at each round + prices set + round number
            return {num_players_ + 1 + 1 + 1 + 1 + num_players_*kMaxRounds + num_players_*kMaxRounds + 1};
        }

        std::vector<int> AirlineSeatsGame::ObservationTensorShape() const {
            // player turn + seats left + round number + seats sold (or bought)
            return {num_players_ + 1 + 1 + num_players_};
        }

        double AirlineSeatsGame::MinUtility() const {
            return -1000;
        }

        double AirlineSeatsGame::MaxUtility() const {
            return 1000;
        }

    }  // namespace kuhn_poker
}  // namespace open_spiel
