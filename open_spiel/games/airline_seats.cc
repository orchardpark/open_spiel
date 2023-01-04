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
            constexpr int kDefaultPower = -50;
            constexpr double kC0 = 36.0;
            constexpr int kMaxRounds = 10;
            constexpr float kC11 = -0.24;
            constexpr float kC12 = -0.293;
            constexpr int kInitialRound = 0;
            constexpr int kInitialPurchasePrice = 50;
            constexpr int kLatePurchasePrice = 80;
            constexpr int kInitialPlayer = 0;
            constexpr int kDefaultSeed = 2139;

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
                                                    {{"players", GameParameter(kDefaultPlayers)},
                                                     {"rng_seed", GameParameter(kDefaultSeed)}},
                    /*default_loadable=*/true,
                    /*provides_factored_observation_string=*/true,
            };

            std::shared_ptr<const Game> Factory(const GameParameters &params) {
                return std::shared_ptr<const Game>(new AirlineSeatsGame(params));
            }

            REGISTER_SPIEL_GAME(kGameType, Factory);
        }  // namespace

        AirlineSeatsState::AirlineSeatsState(std::shared_ptr<const Game> game)
                : State(game),
                  round_(kInitialRound),
                  boughtSeats_(num_players_),
                  sold_(num_players_),
                  prices_(num_players_),
                  currentPlayer_(kChancePlayerId),
                  phase_(GamePhase::InitialConditions) {}

        bool AirlineSeatsState::IsTerminal() const {
            return round_ >= kMaxRounds;
        }

        std::unique_ptr<State> AirlineSeatsState::Clone() const {
            return std::unique_ptr<State>(new AirlineSeatsState(*this));
        }

        std::vector<std::pair<Action, double>> AirlineSeatsState::ChanceOutcomes() const {
            SPIEL_CHECK_TRUE(IsChanceNode());
            // return a dummy action with probability 1
            return {{0, 1.0}};
        }

        std::string AirlineSeatsState::ActionToString(Player player, Action move) const {
            if (phase_ == GamePhase::InitialConditions) {
                return "InitialConditions";
            } else if (phase_ == GamePhase::DemandSimulation) {
                return "DemandSimulation";
            } else if (move <= 5) {
                return absl::StrCat("Buy:", (move - 1) * 5);
            } else {
                return absl::StrCat("SetPrice:", 50 + (move - 6) * 5);
            }
        }

        std::vector<Action> AirlineSeatsState::LegalActions() const {
            // implicit stochastic
            if (phase_ == GamePhase::InitialConditions) return {0};
                // seat buying
            else if (phase_ == GamePhase::SeatBuying) {
                return {1, 2, 3, 4, 5};
            }
                // pricing
            else {
                return {6, 7, 8, 9, 10};
            }
        }

        void AirlineSeatsState::DoApplyAction(Action move) {
            if (!ActionInActions(move)) {
                SpielFatalError(absl::StrCat("Action ", move,
                                             " is not valid in the current state."));
            }
            switch (phase_) {
                case GamePhase::InitialConditions:
                    DoApplyActionInitialConditions();
                    break;
                case GamePhase::SeatBuying:
                    DoApplyActionSeatBuying(move);
                    break;
                case GamePhase::PriceSetting:
                    DoApplyActionPriceSetting(move);
                    break;
                case GamePhase::DemandSimulation:
                    DoApplyActionDemandSimulation();
                    break;
            }
        }

        bool AirlineSeatsState::ActionInActions(Action move) const {
            auto actions = LegalActions();
            return std::find(actions.begin(), actions.end(), move) != actions.end();
        }

        void AirlineSeatsState::DoApplyActionInitialConditions() {
            // Stochastic sampling
            c1_ = RAND() * (kC12 - kC11) + kC11;

            // Set first player
            currentPlayer_ = kInitialPlayer;
            // Update phase
            phase_ = GamePhase::SeatBuying;
        }

        Player AirlineSeatsState::CurrentPlayer() const {
            return currentPlayer_;
        }

        double AirlineSeatsState::RAND() {
            auto rng = std::static_pointer_cast<const AirlineSeatsGame>(game_)->RNG();
            auto rngMax = std::static_pointer_cast<const AirlineSeatsGame>(game_)->RNGMax();
            return (double) rng / (double) rngMax;
        }

        void AirlineSeatsState::DoApplyActionSeatBuying(Action move) {
            // update the state by how much the player bought
            boughtSeats_[currentPlayer_] = (int) (move - 1) * 5;
            currentPlayer_++;

            // once everyone has bought their seats, start setting prices
            if (currentPlayer_ > num_players_) {
                currentPlayer_ = kInitialPlayer;
                phase_ = GamePhase::PriceSetting;
            }
        }

        void AirlineSeatsState::DoApplyActionPriceSetting(Action move) {
            auto price = (int) (move - 6) * 5 + 50;
            prices_[currentPlayer_].push_back(price);
            currentPlayer_++;

            // move on to demand simulation
            if (currentPlayer_ > num_players_) {
                phase_ = GamePhase::DemandSimulation;
                currentPlayer_ = kChancePlayerId;
            }
        }

        void AirlineSeatsState::DoApplyActionDemandSimulation() {
            // calculate seats sold
            std::vector<double> powers;
            std::vector<double> randoms;
            std::vector<double> shares;
            std::vector<double> randomizedShares;
            // calculate powers
            for (Player i = kInitialPlayer; i < num_players_; i++) {
                int price = prices_[currentPlayer_].back();
                double power = pow(price, kDefaultPower);
                powers.push_back(power);
            }
            // generate randoms
            for (Player i = kInitialPlayer; i < num_players_; i++) {
                double random = ((RAND() - 0.5) * kDefaultRandom) / 100.0;
                randoms.push_back(random);
            }
            double powerSum = std::accumulate(powers.begin(), powers.end(), 0.0);
            double invertedSum = pow(powerSum, 1.0 / kDefaultRandom);
            double totalDemand = invertedSum * kC0 * c1_;

            for (Player i = kInitialPlayer; i < num_players_; i++) {
                double share = powers[i] / powerSum;
                shares.push_back(share);
            }

            for (Player i = kInitialPlayer; i < num_players_; i++) {
                double randomizedShare = (1 + randoms[i]) * shares[i];
                randomizedShares.push_back(randomizedShare);
            }

            for (Player i = kInitialPlayer; i < num_players_; i++) {
                int seatsSold = (int) round(totalDemand * randomizedShares[i]);
                sold_[i].push_back(seatsSold);
            }

            // move on to the next round
            phase_ = GamePhase::PriceSetting;
            round_++;
            currentPlayer_ = kInitialPlayer;

        }

        bool AirlineSeatsState::IsOutOfSeats(Player player) const {
            auto playerSold = std::accumulate(sold_[player].begin(), sold_[player].end(), 0);
            auto playerBought = boughtSeats_[player];
            return playerSold >= playerBought;
        }

        std::vector<double> AirlineSeatsState::Returns() const {
            std::vector<double> returns;
            for (Player i = kInitialPlayer; i < num_players_; i++) {
                double pnl = boughtSeats_[i] * kInitialPurchasePrice;
                int seatsLeft = boughtSeats_[i];
                for (int round = kInitialRound; round < kMaxRounds; round++) {
                    int sold = sold_[i][round];
                    double price = prices_[i][round];
                    pnl += sold * price;
                    if (seatsLeft > 0) {
                        seatsLeft -= sold;
                        if (seatsLeft < 0) pnl -= -seatsLeft * kLatePurchasePrice;
                    } else {
                        pnl -= sold * kLatePurchasePrice;
                    }
                }
                returns.push_back(pnl);
            }

            return returns;
        }

        std::vector<double> AirlineSeatsState::Rewards() const {
            std::vector<double> rewards;
            for (Player i = kInitialPlayer; i < num_players_; i++) {
                double pnl = boughtSeats_[i] * kInitialPurchasePrice;
                int seatsLeft = boughtSeats_[i];
                for (int round = kInitialRound; round < round_; round++) {
                    int sold = sold_[i][round];
                    double price = prices_[i][round];
                    pnl += sold * price;
                    if (seatsLeft > 0) {
                        seatsLeft -= sold;
                        if (seatsLeft < 0) pnl -= -seatsLeft * kLatePurchasePrice;
                    } else {
                        pnl -= sold * kLatePurchasePrice;
                    }
                }
                rewards.push_back(pnl);
            }

            return rewards;
        }

        void AirlineSeatsState::ObservationTensor(Player player, absl::Span<float> values) const {
            return InformationStateTensor(player, values);
        }

        std::string AirlineSeatsState::ObservationString(Player player) const {
            return InformationStateString(player);
        }

        std::string AirlineSeatsState::ToString() const {
            std::string phase;
            switch (phase_) {
                case GamePhase::InitialConditions:
                    phase = "IC";
                    break;
                case GamePhase::SeatBuying:
                    phase = "SB";
                    break;
                case GamePhase::PriceSetting:
                    phase = "PS";
                    break;
                case GamePhase::DemandSimulation:
                    phase = "DS";
                    break;
            }
            std::string boughtSeats;
            for (Player i = kInitialPlayer; i < num_players_; i++) {
                absl::StrAppendFormat(&boughtSeats, "%d,", boughtSeats_[i]);
            }

            std::string sold;
            for (int j = 0; j < round_; j++) {
                absl::StrAppendFormat(&sold, "%d:", j);
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    absl::StrAppendFormat(&sold, "%d,", sold_[i][j]);
                }
            }

            std::string prices;
            for (int j = 0; j < round_; j++) {
                absl::StrAppendFormat(&prices, "%d:", j);
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    absl::StrAppendFormat(&prices, "%d,", prices_[i][j]);
                }
            }


            std::string output = absl::StrFormat("%d|%f|%d|%s|%s|%s|%s",
                                                 round_,
                                                 c1_,
                                                 currentPlayer_,
                                                 phase.c_str(),
                                                 boughtSeats.c_str(),
                                                 sold.c_str(),
                                                 prices.c_str()
            );

            return output;
        }

        std::string AirlineSeatsState::InformationStateString(Player player) const {
            std::string boughtSeats;
            absl::StrAppendFormat(&boughtSeats, "%d", boughtSeats_[player]);

            std::string sold;
            for (int j = 0; j < kMaxRounds; j++) {
                absl::StrAppendFormat(&sold, "%d:", j);
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    absl::StrAppendFormat(&sold, "%d,", sold_[i][j]);
                }
            }

            std::string prices;
            for (int j = 0; j < kMaxRounds; j++) {
                absl::StrAppendFormat(&prices, "%d:", j);
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    absl::StrAppendFormat(&prices, "%d,", prices_[i][j]);
                }
            }


            std::string output = absl::StrFormat("%d|%d|%s|%s|%s",
                                                 round_,
                                                 currentPlayer_,
                                                 boughtSeats.c_str(),
                                                 sold.c_str(),
                                                 prices.c_str()
            );

            return output;
        }

        void AirlineSeatsState::InformationStateTensor(Player player, absl::Span<float> values) const {
            SPIEL_CHECK_GE(player, 0);
            SPIEL_CHECK_LT(player, num_players_);
            SPIEL_CHECK_EQ(values.size(), game_->InformationStateTensorSize());
            std::fill(values.begin(), values.end(), 0);
        }

        AirlineSeatsGame::AirlineSeatsGame(const GameParameters &params)
                : Game(kGameType, params),
                  num_players_(ParameterValue<int>("players")),
                  rng_(std::mt19937(ParameterValue<int>("rng_seed") == -1
                                    ? std::time(0)
                                    : ParameterValue<int>("rng_seed")))
                  {
            SPIEL_CHECK_GE(num_players_, kGameType.min_num_players);
            SPIEL_CHECK_LE(num_players_, kGameType.max_num_players);
            std::cout << "Number of players: " << num_players_ << std::endl;
        }

        std::unique_ptr<State> AirlineSeatsGame::NewInitialState() const {
            return std::unique_ptr<State>(new AirlineSeatsState(shared_from_this()));
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
            // hardcoded for now, 5 buy qty and 6 price setting (including setting no price)
            return 10;
        }

        int AirlineSeatsGame::MaxChanceNodesInHistory() const {
            return kMaxRounds;
        }

        std::vector<int> AirlineSeatsGame::InformationStateTensorShape() const {
            // round + player + seats at beginning + (seats sold) previous rounds + prices set
            return {1 + 1 + 1 + num_players_ * kMaxRounds + num_players_ * kMaxRounds};
        }

        std::vector<int> AirlineSeatsGame::ObservationTensorShape() const {
            return InformationStateTensorShape();
        }

        double AirlineSeatsGame::MinUtility() const {
            return -1000;
        }

        double AirlineSeatsGame::MaxUtility() const {
            return 5000;
        }

        std::string AirlineSeatsGame::GetRNGState() const {
            std::ostringstream rng_stream;
            rng_stream << rng_;
            return rng_stream.str();
        }

        void AirlineSeatsGame::SetRNGState(const std::string &rng_state) const {
            if (rng_state.empty()) return;
            std::istringstream rng_stream(rng_state);
            rng_stream >> rng_;
        }

        unsigned long AirlineSeatsGame::RNG() const { return rng_(); }

        unsigned long AirlineSeatsGame::RNGMax() const {
            return rng_.max();
        }


    }  // namespace airline_seats
}  // namespace open_spiel
