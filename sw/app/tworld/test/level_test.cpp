/*
 * Copyright 2022 Nicolas Maltais
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <fstream>
#include <iterator>
#include <filesystem>

extern "C" {
#include <boot/init.h>
#include <sim/flash.h>
#include <tworld_dir.h>
#include <tworld_level.h>
#include <tworld_actor.h>
#include <game.h>
}

// Whether to export a list of actors state for each time to a file for failing tests.
constexpr bool EXPORT_ACTORS_FILE = true;
constexpr const char* EXPORT_ACTORS_DIR = "test/dev/";

// Path of TWS files for level packs declared in pack.py, in the same order.
// The working directory should be app/tworld when running tests.
constexpr const char* LEVEL_PACK_TWS[] = {
        "test/tws/cclp1.tws",
        "test/tws/cclp2.tws",
        "test/tws/cclp3.tws",
        "test/tws/cclp4.tws",
};

class Move {
public:
    uint32_t delta;
    direction_mask_t direction;

    Move(uint32_t delta, direction_mask_t direction)
            : delta(delta), direction(direction) {}
};

class Solution {
public:
    uint32_t total_time;
    uint8_t stepping;
    direction_t initial_random_slide_dir;
    uint32_t prng_seed;
    std::vector<Move> moves;

    Solution(uint32_t total_time, uint8_t stepping, direction_t initial_random_slide_dir,
             uint32_t prng_seed, std::vector<Move> moves)
            : total_time(total_time), stepping(stepping),
              initial_random_slide_dir(initial_random_slide_dir),
              prng_seed(prng_seed), moves(std::move(moves)) {}

    template<typename T>
    void iterate(T func) const {
        uint32_t index = 0;
        uint32_t time = 0;
        while (true) {
            if (time >= moves[index].delta) {
                time -= moves[index].delta;
                if (!func(moves[index].direction)) {
                    return;
                }
                ++index;
                if (index == moves.size()) {
                    return;
                }
            } else if (!func(DIR_MASK_NONE)) {
                return;
            }
            ++time;
        }
    }
};

/**
 * Used to load solutions from TWS files generated by Tile World. The file format is documented at:
 * https://github.com/Qalthos/Tile-World/blob/master/solution.c
 */
class SolutionLoader {
private:
    std::vector<uint8_t> data;
    size_t pos;

public:
    static constexpr direction_mask_t DIRECTIONS[] = {
            DIR_NORTH_MASK,
            DIR_WEST_MASK,
            DIR_SOUTH_MASK,
            DIR_EAST_MASK,
            DIR_NORTHWEST_MASK,
            DIR_SOUTHWEST_MASK,
            DIR_NORTHEAST_MASK,
            DIR_SOUTHEAST_MASK,
    };

    explicit SolutionLoader(std::ifstream& stream) : pos(0) {
        data.insert(data.begin(), std::istreambuf_iterator<char>(stream),
                    std::istreambuf_iterator<char>());

        const std::array<uint8_t, 4> signature{0x35, 0x33, 0x9b, 0x99};
        if (!std::equal(signature.begin(), signature.end(), data.begin())) {
            throw std::runtime_error("Bad TWS signature");
        }
        if (data[4] != 1) {
            throw std::runtime_error("Only Lynx ruleset supported");
        }
    }

    Solution read_solution(level_idx_t level_number) {
        pos = 8;
        size_t end_pos;
        bool found = false;
        while (pos < data.size()) {
            size_t offset = read(4);
            if (offset == 0) {
                continue;  // padding
            }
            end_pos = pos + offset;
            uint16_t number = read(2);
            if (number - 1 == level_number) {
                found = true;
                break;
            }
            pos = end_pos;
        }
        if (!found) {
            throw std::runtime_error("level not found in TWS file");
        }

        pos += 5;
        uint8_t initial_conditions = read(1);
        auto initial_random_slide_dir = (direction_t) (initial_conditions & 0x7);
        uint8_t stepping = (initial_conditions >> 3) & 0x7;

        uint32_t prng_seed = read(4);
        uint32_t total_time = read(4);
        std::vector<Move> all_moves;
        while (pos < end_pos) {
            uint8_t b = data[pos];
            std::vector<Move> moves;
            if ((b & 0x3) == 0b00) {
                auto new_moves = read_move_type3();
                moves.insert(moves.end(), new_moves.begin(), new_moves.end());
            } else if ((b & 0x3) == 0b01) {
                moves.push_back(read_move_type1(1));
            } else if ((b & 0x3) == 0b10) {
                moves.push_back(read_move_type1(2));
            } else if ((b & 0x10) == 0x10) {
                moves.push_back(read_move_type4(((b >> 2) & 0x3) + 2));
            } else if ((b & 0x10) == 0x00) {
                moves.push_back(read_move_type2());
            } else {
                throw std::runtime_error("Unknown move encoding");
            }

            if (all_moves.empty()) {
                // First move has -1 on delta
                --moves[0].delta;
            }
            all_moves.insert(all_moves.end(), moves.begin(), moves.end());
        }

        if (pos != end_pos) {
            throw std::runtime_error("Truncated move encoding");
        }

        return {total_time, stepping, initial_random_slide_dir, prng_seed,
                std::move(all_moves)};
    }

private:
    uint64_t read(size_t n) {
        uint64_t val = 0;
        for (size_t i = 0; i < n; ++i) {
            val |= data[pos++] << (i * 8);
        }
        return val;
    }

    Move read_move_type1(size_t length) {
        uint64_t move = read(length);
        direction_mask_t direction = DIRECTIONS[(move >> 2) & 0x7];
        uint32_t delta = (move >> 5) + 1;
        return {delta, direction};
    }

    Move read_move_type2() {
        uint64_t move = read(4);
        direction_mask_t direction = DIRECTIONS[(move >> 2) & 0x3];
        uint32_t delta = ((move >> 5) + 1) & 0x7fffff;
        return {delta, direction};
    }

    std::array<Move, 3> read_move_type3() {
        uint64_t move = read(1);
        direction_mask_t direction0 = DIRECTIONS[(move >> 2) & 0x3];
        direction_mask_t direction1 = DIRECTIONS[(move >> 4) & 0x3];
        direction_mask_t direction2 = DIRECTIONS[(move >> 6) & 0x3];
        return {Move(4, direction0), Move(4, direction1), Move(4, direction2)};
    }

    Move read_move_type4(size_t length) {
        uint64_t move = read(length);
        auto direction = (direction_mask_t) ((move >> 5) & 0x1ff);
        if (std::find(std::begin(DIRECTIONS), std::end(DIRECTIONS),
                      direction) == std::end(DIRECTIONS)) {
            throw std::runtime_error("unsupported type 4 move encoding (mouse)");
        }
        uint32_t delta = (move >> 14) + 1;
        return {delta, direction};
    }
};

class LevelTestParam {
public:
    level_pack_idx_t pack;
    level_idx_t level;
    std::string pack_name;
    Solution solution;

    LevelTestParam(level_pack_idx_t pack, level_idx_t level,
                   std::string pack_name, Solution solution)
            : pack(pack), level(level), pack_name(std::move(pack_name)),
              solution(std::move(solution)) {}

    friend std::ostream& operator<<(std::ostream& str, const LevelTestParam& param) {
        // Test name: "CCLPxLyyy"
        str << param.pack_name << "L" << std::setfill('0') << std::setw(3) << (param.level + 1);
        return str;
    }
};

std::vector<LevelTestParam> create_test_cases() {
    // Simulator initialization
    sys_init();
    sim_flash_load("assets.dat");

    level_read_packs();

    // Read solution for levels in all packs.
    std::cout << "Reading level solutions..." << std::endl;
    std::vector<LevelTestParam> params;
    for (level_pack_idx_t i = 0; i < LEVEL_PACK_COUNT; ++i) {
        std::ifstream solution_stream(LEVEL_PACK_TWS[i]);
        SolutionLoader loader(solution_stream);

        const level_pack_info_t& info = tworld_packs.packs[i];
        for (level_idx_t j = 0; j < info.total_levels; ++j) {
            Solution solution = loader.read_solution(j);
            params.emplace_back(i, j, info.name, std::move(solution));
        }

        solution_stream.close();
    }

    if (EXPORT_ACTORS_FILE) {
        std::filesystem::create_directories(EXPORT_ACTORS_DIR);
    }

    return params;
}

constexpr const char* END_CAUSE_NAMES[] = {
        "exit not reached",
        "burned",
        "monster collision",
        "block collision",
        "drowned",
        "bombed",
        "out of time",
        "complete",
        "ERROR",
};

constexpr const char* ENTITY_NAMES[] = {
        "none",
        "chip",
        "",
        "",
        "block_ghost",
        "block",
        "bug",
        "paramecium",
        "glider",
        "fireball",
        "ball",
        "blob",
        "tank",
        "tank_reversed",
        "walker",
        "teeth",
};

constexpr const char* DIRECTION_NAMES[] = {
        "north",
        "west",
        "south",
        "east",
};

class LevelTest : public testing::TestWithParam<LevelTestParam> {
};

template <class T>
static void do_state_update(std::optional<T>& out, direction_mask_t input) {
    if (tworld.error) {
        // assertion failed or other error.
        tworld.end_cause = END_CAUSE_ERROR;
    }

    tworld.input_state = input;
    tworld_update();

    if (out) {
        // Output the state of all actors to the file.
        std::ostream& stream = out.value();
        stream << "STEP TIME " << tworld.current_time << std::endl;
        for (actor_idx_t i = 0; i < tworld.actors_size; ++i) {
            const active_actor_t act = tworld.actors[i];
            const active_actor_t state = act_actor_get_state(act);
            const position_t pos = act_actor_get_pos(act);
            const actor_t tile = tworld_get_top_tile(pos);
            const step_t step = act_actor_get_step(act);
            const entity_t entity = actor_get_entity(tile);
            const direction_t direction = actor_get_direction(tile);

            if (state == ACTOR_STATE_HIDDEN && step <= 0) {
                // Actor is hidden and not in an animation state.
                continue;
            }

            stream << "[" << (int) i << "] (" << (int) pos.x << "," << (int) pos.y << ") "
                 << ENTITY_NAMES[entity >> 2] << " (" << DIRECTION_NAMES[direction] << "), "
                 << (step > 0 && state == ACTOR_STATE_HIDDEN ? "anim" : "alive")
                 << ", step=" << (int) step << std::endl;
        }
        stream << std::endl;
    }
}

TEST_P(LevelTest, level_test) {
    game.current_pack = GetParam().pack;
    game.current_level = GetParam().level;
    level_read_level();
    level_get_links();

    const Solution& solution = GetParam().solution;
    tworld.prng_value0 = solution.prng_seed;
    tworld.stepping = solution.stepping;
    tworld.random_slide_dir = solution.initial_random_slide_dir;

    std::optional<std::ostringstream> out;
    if (EXPORT_ACTORS_FILE) {
        out = std::ostringstream();
    }

    // Step through the solution until end of solution or game over.
    solution.iterate([&](direction_mask_t direction) -> bool {
        if (tworld.error) {
            // assertion failed or other error.
            tworld.end_cause = END_CAUSE_ERROR;
        } else if (!tworld_is_game_over()) {
            do_state_update(out, direction);
            return true;
        }
        return false;
    });

    if (!tworld_is_game_over()) {
        // Level is not done, step with no input until the end of solution.
        while (tworld.current_time < solution.total_time) {
            do_state_update(out, DIR_MASK_NONE);
        }
    }

    std::cout << "Level " << GetParam().pack_name << "/" << (GetParam().level + 1) << ": "
              << END_CAUSE_NAMES[tworld.end_cause] << std::endl;

    if (EXPORT_ACTORS_FILE && tworld.end_cause != END_CAUSE_COMPLETE) {
        // Export actors file if test failed.
        std::ostringstream name;
        name << GetParam() << ".txt";
        std::ofstream file(std::filesystem::path(EXPORT_ACTORS_DIR) / name.str());
        file << out.value().str();
        file.close();
    }

    if (tworld.end_cause != END_CAUSE_COMPLETE) {
        FAIL();
    }
}

// Instantiate one test per level, for all level packs.
INSTANTIATE_TEST_SUITE_P(level_test, LevelTest, testing::ValuesIn(create_test_cases()),
                         testing::PrintToStringParamName());
