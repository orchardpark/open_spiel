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

#include "open_spiel/games/kuhn_poker.h"

#include "open_spiel/algorithms/get_all_states.h"
#include "open_spiel/policy.h"
#include "open_spiel/spiel_utils.h"
#include "open_spiel/tests/basic_tests.h"

namespace open_spiel {
namespace airline_seats {
namespace {

namespace testing = open_spiel::testing;

void BasicAirlineSeatsTest() {
  testing::LoadGameTest("airline_seats");
  testing::RandomSimTest(*LoadGame("airline_seats"), 100);
  testing::RandomSimTestWithUndo(*LoadGame("airline_seats"), 1);
  for (Player players = 2; players <= 4; players++) {
    testing::RandomSimTest(
        *LoadGame("airline_seats", {{"players", GameParameter(players)}}), 100);
  }
}

}  // namespace
}  // namespace kuhn_poker
}  // namespace open_spiel

int main(int argc, char **argv) {
  open_spiel::airline_seats::BasicAirlineSeatsTest();
  open_spiel::testing::CheckChanceOutcomes(*open_spiel::LoadGame(
      "kuhn_poker", {{"players", open_spiel::GameParameter(3)}}));
  open_spiel::testing::RandomSimTest(*open_spiel::LoadGame("kuhn_poker"),
                                     /*num_sims=*/10);
  open_spiel::testing::ResampleInfostateTest(
      *open_spiel::LoadGame("kuhn_poker"),
      /*num_sims=*/10);
}
