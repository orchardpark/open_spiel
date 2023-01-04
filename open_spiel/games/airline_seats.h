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

#ifndef OPEN_SPIEL_GAMES_AIRLINE_SEATS_H_
#define OPEN_SPIEL_GAMES_AIRLINE_SEATS_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "open_spiel/policy.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

// Airline seats game, where players compete to sell airline seats.
// Parameters:
//     "players"       int    number of players               (default = 2)

namespace open_spiel {
    namespace airline_seats {

        enum class GamePhase {
            InitialConditions,
            SeatBuying,
            PriceSetting,
            DemandSimulation
        };

        enum ActionType {
            Buy0 = 0,
            Buy5 = 1,
            Buy10 = 2,
            Buy15 = 3,
            Buy20 = 4,
            SetPrice50 = 5,
            SetPrice55 = 6,
            SetPrice60 = 7,
            SetPrice65 = 8,
            SetPrice70 = 9,
        };

        class AirlineSeatsGame;

        class AirlineSeatsState : public State {
        public:
            explicit AirlineSeatsState(std::shared_ptr<const Game> game);

            AirlineSeatsState(const AirlineSeatsState &) = default;

            [[nodiscard]] Player CurrentPlayer() const override;

            [[nodiscard]] std::string ActionToString(Player player, Action move) const override;

            [[nodiscard]] std::string ToString() const override;

            [[nodiscard]] bool IsTerminal() const override;

            [[nodiscard]] std::vector<double> Returns() const override;

            [[nodiscard]] std::vector<double> Rewards() const override;

            [[nodiscard]] std::string InformationStateString(Player player) const override;

            [[nodiscard]] std::string ObservationString(Player player) const override;

            void InformationStateTensor(Player player,
                                        absl::Span<float> values) const override;

            void ObservationTensor(Player player,
                                   absl::Span<float> values) const override;

            [[nodiscard]] std::unique_ptr<State> Clone() const override;

            [[nodiscard]] std::vector<std::pair<Action, double>> ChanceOutcomes() const override;

            [[nodiscard]] std::vector<Action> LegalActions() const override;
            [[nodiscard]] std::string Serialize() const override;

        protected:
            void DoApplyAction(Action move) override;

        private:
            // rng
            double RAND();

            // helper function
            [[nodiscard]] bool IsOutOfSeats(Player player) const;

            // action functions
            void DoApplyActionInitialConditions();

            void DoApplyActionSeatBuying(Action move);

            void DoApplyActionPriceSetting(Action move);

            void DoApplyActionDemandSimulation();
            Player PreviousPlayer() const;

            // variables that maintain the state (history) of the game
            std::vector<int> boughtSeats_; // how many seats were bought initially per player
            std::vector<std::vector<int>> sold_; // sold seats per player at each round
            std::vector<std::vector<int>> prices_; // prices set per player at each round
            double c1_;
            int round_;
            GamePhase phase_;
            Player currentPlayer_;
            friend class AirlineSeatsGame;
            [[nodiscard]] bool ActionInActions(Action move) const;
        };

        class AirlineSeatsGame : public Game {
        public:
            explicit AirlineSeatsGame(const GameParameters &params);

            int NumDistinctActions() const override;

            std::unique_ptr<State> NewInitialState() const override;
            std::unique_ptr<AirlineSeatsState> NewInitialAirlineSeatsState() const;

            int MaxChanceOutcomes() const override;

            int NumPlayers() const override;

            std::vector<int> InformationStateTensorShape() const override;

            std::vector<int> ObservationTensorShape() const override;

            int MaxGameLength() const override;

            int MaxChanceNodesInHistory() const override;

            double MinUtility() const override;

            double MaxUtility() const override;

            std::string GetRNGState() const override;

            void SetRNGState(const std::string &rng_state) const override;
            std::unique_ptr<State> DeserializeState(
                    const std::string& str) const override;


        private:
            friend class AirlineSeatsState;
            mutable std::mt19937 rng_;
            unsigned long RNG() const;
            unsigned long RNGMax() const;
            int num_players_;
        };

    }  // namespace arline_seats
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_AIRLINE_SEATS_H_
