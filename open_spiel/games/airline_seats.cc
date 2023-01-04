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
                                                    GameType::RewardModel::kTerminal,
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
            } else if (move < 5) {
                return absl::StrCat("Buy:", (move) * 5);
            } else {
                return absl::StrCat("SetPrice:", 50 + (move - 5) * 5);
            }
        }

        std::vector<Action> AirlineSeatsState::LegalActions() const {
            // implicit stochastic
            if (phase_ == GamePhase::InitialConditions || phase_ == GamePhase::DemandSimulation) return {0};
                // seat buying
            else if (phase_ == GamePhase::SeatBuying) {
                return {0, 1, 2, 3, 4};
            }
                // pricing
            else {
                return {5, 6, 7, 8, 9};
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
            boughtSeats_[currentPlayer_] = (int) (move) * 5;
            currentPlayer_++;

            // once everyone has bought their seats, start setting prices
            if (currentPlayer_ >= num_players_) {
                currentPlayer_ = kInitialPlayer;
                phase_ = GamePhase::PriceSetting;
            }
        }

        void AirlineSeatsState::DoApplyActionPriceSetting(Action move) {
            auto price = (int) (move - 5) * 5 + 50;
            prices_[currentPlayer_].push_back(price);
            currentPlayer_++;

            // move on to demand simulation
            if (currentPlayer_ >= num_players_) {
                phase_ = GamePhase::DemandSimulation;
                currentPlayer_ = kChancePlayerId;
            }
        }

        double my_pow(double a,double b) {
            if (b < 0) return 1.0 / pow(a,std::abs(b));
            else return pow(a,b);
        }

        void AirlineSeatsState::DoApplyActionDemandSimulation() {
            // calculate seats sold
            std::vector<double> powers;
            std::vector<double> randoms;
            std::vector<double> shares;
            std::vector<double> randomizedShares;
            // calculate powers
            for (Player i = kInitialPlayer; i < num_players_; i++) {
                int price = prices_[i].back();
                double power = pow(price, kDefaultPower);
                powers.push_back(power);
            }
            // generate randoms
            for (Player i = kInitialPlayer; i < num_players_; i++) {
                double random = ((RAND() - 0.5) * kDefaultRandom) / 100.0;
                randoms.push_back(random);
            }
            double powerSum = std::accumulate(powers.begin(), powers.end(), 0.0);
            double invertedSum = my_pow(powerSum, 1.0 / kDefaultPower);
            double totalDemand = kC0 + invertedSum * c1_;

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

            if(round_ >= kMaxRounds)
            {
                currentPlayer_ = kTerminalPlayerId;
            }

        }

        bool AirlineSeatsState::IsOutOfSeats(Player player) const {
            auto playerSold = std::accumulate(sold_[player].begin(), sold_[player].end(), 0);
            auto playerBought = boughtSeats_[player];
            return playerSold >= playerBought;
        }

        std::vector<double> AirlineSeatsState::Returns() const {
            std::vector<double> returns(num_players_, 0.0);
            if(IsTerminal()) {
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    double pnl = boughtSeats_[i] * -kInitialPurchasePrice;
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
                    returns[i] = pnl;
                }
            }

            return returns;
        }

        /*
        std::vector<double> AirlineSeatsState::Rewards() const {
            SPIEL_CHECK_FALSE(IsChanceNode());
            std::vector<double> rewards;
            for (Player i = kInitialPlayer; i < num_players_; i++) {
                if(i!=PreviousPlayer()) rewards.push_back(0);
                else if(round_==0) rewards.push_back(boughtSeats_[i] * -kInitialPurchasePrice);
                else{
                    int seatsLeft = boughtSeats_[i];
                    for (int round = kInitialRound; round < round_; round++) {
                        int sold = sold_[i][round];
                        double price = prices_[i][round];
                        double pnl = sold * price;
                        if (seatsLeft > 0) {
                            seatsLeft -= sold;
                            if (seatsLeft < 0) pnl -= -seatsLeft * kLatePurchasePrice;
                        } else {
                            pnl -= sold * kLatePurchasePrice;
                        }
                        if(round == round_-1)
                            rewards.push_back(pnl);
                    }
                }
            }

            return rewards;
        }
         */

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
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    absl::StrAppendFormat(&sold, "%d,", sold_[i][j]);
                }
            }

            std::string prices;
            for (int j = 0; j < round_; j++) {
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
            SPIEL_CHECK_GE(player, 0);
            SPIEL_CHECK_LT(player, num_players_);
            std::string boughtSeats;
            absl::StrAppendFormat(&boughtSeats, "%d", boughtSeats_[player]);

            std::string sold;
            for (int j = 0; j < round_; j++) {
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    absl::StrAppendFormat(&sold, "%d,", sold_[i][j]);
                }
            }

            std::string prices;
            for (int j = 0; j < round_; j++) {
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
            int offset = 0;
            values[offset+round_] = 1;
            offset+=kMaxRounds;
            values[offset+currentPlayer_] = 1;
            offset+=num_players_;
            values[offset] = (float)boughtSeats_[player];
            offset++;
            for (int j = 0; j < round_; j++) {
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    values[offset] = (float)sold_[i][j];
                }
            }
            offset+=kMaxRounds*num_players_;
            for (int j = 0; j < round_; j++) {
                for (Player i = kInitialPlayer; i < num_players_; i++) {
                    values[offset] = (float)prices_[i][j];
                }
            }
        }

        std::string AirlineSeatsState::Serialize() const {
            std::string result;
            auto rngState = game_->GetRNGState();
            auto stateString = this->ToString();
            result+=rngState;
            result+="|";
            result+=stateString;

            return result;
        }

        Player AirlineSeatsState::PreviousPlayer() const {
            if(currentPlayer_ ==0) return num_players_-1;
            else return currentPlayer_-1;
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
        }

        std::unique_ptr<State> AirlineSeatsGame::NewInitialState() const {
            return NewInitialAirlineSeatsState();
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
            // round (one hot) + player (one hot) + seats at beginning + (seats sold) previous rounds + prices set
            return {kMaxRounds + num_players_ + 1 + num_players_ * kMaxRounds + num_players_ * kMaxRounds};
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

        std::unique_ptr<State> AirlineSeatsGame::DeserializeState(const std::string &str) const {
            std::unique_ptr<AirlineSeatsState> state = NewInitialAirlineSeatsState();
            std::vector<std::string> lines = absl::StrSplit(str, '|');
            // set rng
            SetRNGState(lines.at(0));
            // set round
            state->round_ = std::stoi(lines.at(1));
            // set c1
            state->c1_ = std::stod(lines.at(2));
            // set player
            state->currentPlayer_ = std::stoi(lines.at(3));
            // set phase
            std::string phase = lines.at(4);
            if(phase == "IC")
                state->phase_ = GamePhase::InitialConditions;
            else if(phase == "SB")
                state->phase_ = GamePhase::SeatBuying;
            else if(phase == "PS")
                state->phase_ = GamePhase::PriceSetting;
            else
                state->phase_ = GamePhase::DemandSimulation;
            // seats
            std::vector<std::string> seats = absl::StrSplit(lines.at(5), ",");
            for(Player i = kInitialPlayer; i<num_players_; i++)
            {
                state->boughtSeats_[i] = std::stoi(seats[i]);
            }
            // sold
            std::vector<std::string> sold = absl::StrSplit(lines.at(6), ",");
            for(int j=kInitialRound; j<state->round_; j++)
            {
                for(Player i=kInitialPlayer; i<num_players_; i++)
                {
                    state->sold_[i].push_back(std::stoi(sold[i+j*num_players_]));
                }
            }

            // prices
            std::vector<std::string> prices = absl::StrSplit(lines.at(7), ",");
            for(int j=kInitialRound; j<state->round_; j++)
            {
                for(Player i=kInitialPlayer; i<num_players_; i++)
                {
                    state->prices_[i].push_back(std::stoi(prices[i+j*num_players_]));
                }
            }
            std::cout << "repr:" << state->ToString() << std::endl;

            return state;

        }

        std::unique_ptr<AirlineSeatsState> AirlineSeatsGame::NewInitialAirlineSeatsState() const {
            return std::make_unique<AirlineSeatsState>(shared_from_this());
        }


    }  // namespace airline_seats
}  // namespace open_spiel
