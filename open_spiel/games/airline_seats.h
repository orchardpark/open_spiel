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

enum class GamePhase
{
    DemandSimulation,
    SeatBuying,
    PriceSetting,
};

enum ActionType
{
    Buy0=1,
    Buy5=2,
    Buy10=3,
    Buy15=4,
    Buy20=5,
    SetPrice50=6,
    SetPrice55=7,
    SetPrice60=8,
    SetPrice70=9,
    SetPrice80=10
};

class AirlineSeatsGame;
class AirlineSeatsObserver;

class AirlineSeatsState : public State {
 public:
  explicit AirlineSeatsState(std::shared_ptr<const Game> game);
  AirlineSeatsState(const AirlineSeatsState&) = default;

  [[nodiscard]] Player CurrentPlayer() const override;

  [[nodiscard]] std::string ActionToString(Player player, Action move) const override;
  [[nodiscard]] std::string ToString() const override;
  [[nodiscard]] bool IsTerminal() const override;
  [[nodiscard]] std::vector<double> Returns() const override;
  [[nodiscard]] std::string InformationStateString(Player player) const override;
  [[nodiscard]] std::string ObservationString(Player player) const override;
  void InformationStateTensor(Player player,
                              absl::Span<float> values) const override;
  void ObservationTensor(Player player,
                         absl::Span<float> values) const override;
  [[nodiscard]] std::unique_ptr<State> Clone() const override;
  [[nodiscard]] std::vector<std::pair<Action, double>> ChanceOutcomes() const override;
  [[nodiscard]] std::vector<Action> LegalActions() const override;
  std::unique_ptr<State> ResampleFromInfostate(
      int player_id, std::function<double()> rng) const override;

 protected:
  void DoApplyAction(Action move) override;

 private:
  friend class AirlineSeatsObserver;
  std::vector<int> seats_;
  std::vector<float> pnl_;
  int round_;
  GamePhase phase_;
  int winner_;
};

class AirlineSeatsGame : public Game {
 public:
  explicit AirlineSeatsGame(const GameParameters& params);
  int NumDistinctActions() const override;
  std::unique_ptr<State> NewInitialState() const override;
  int MaxChanceOutcomes() const override;
  int NumPlayers() const override;
  std::vector<int> InformationStateTensorShape() const override;
  std::vector<int> ObservationTensorShape() const override;
  int MaxGameLength() const override;
  int MaxChanceNodesInHistory() const override;
  std::shared_ptr<Observer> MakeObserver(
      absl::optional<IIGObservationType> iig_obs_type,
      const GameParameters& params) const override;
  double MinUtility() const override;
  double MaxUtility() const override;

  // Used to implement the old observation API.
  std::shared_ptr<AirlineSeatsObserver> default_observer_;
  std::shared_ptr<AirlineSeatsObserver> info_state_observer_;
  std::shared_ptr<AirlineSeatsObserver> public_observer_;
  std::shared_ptr<AirlineSeatsObserver> private_observer_;

 private:
  // Number of players.
  int num_players_;
};

}  // namespace arline_seats
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_AIRLINE_SEATS_H_
