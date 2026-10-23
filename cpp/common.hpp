#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace l6 {

// The largest supplied board has 397 interior cells (radius 12).
// Reject larger inputs instead of silently dropping cells above bit 511.
struct Bits {
    std::array<std::uint64_t, 8> w{};
    bool operator==(const Bits&) const = default;
    bool any() const {
        std::uint64_t combined = 0;
        for (auto word : w) combined |= word;
        return combined != 0;
    }
    int count() const {
        int result = 0;
        for (auto word : w) result += std::popcount(word);
        return result;
    }
    bool test(int index) const { return (w[index / 64] >> (index % 64)) & 1U; }
    void set(int index) { w[index / 64] |= std::uint64_t{1} << (index % 64); }
    Bits& operator&=(const Bits& rhs) {
        for (int i = 0; i < 8; ++i) w[i] &= rhs.w[i];
        return *this;
    }
    Bits& operator|=(const Bits& rhs) {
        for (int i = 0; i < 8; ++i) w[i] |= rhs.w[i];
        return *this;
    }
    Bits& operator^=(const Bits& rhs) {
        for (int i = 0; i < 8; ++i) w[i] ^= rhs.w[i];
        return *this;
    }
    friend Bits operator&(Bits lhs, const Bits& rhs) { return lhs &= rhs; }
    friend Bits operator|(Bits lhs, const Bits& rhs) { return lhs |= rhs; }
    friend Bits operator^(Bits lhs, const Bits& rhs) { return lhs ^= rhs; }
    friend Bits operator~(Bits value) {
        for (auto& word : value.w) word = ~word;
        return value;
    }
};

struct BitsHash {
    std::size_t operator()(const Bits& bits) const noexcept {
        std::uint64_t value = 0x9e3779b97f4a7c15ULL;
        for (auto word : bits.w)
            value ^= word + 0x9e3779b97f4a7c15ULL + (value << 6) + (value >> 2);
        return static_cast<std::size_t>(value);
    }
};

using Config = std::array<int, 6>;
using Tick = std::array<char, 6>;
using Program = std::vector<Tick>;

struct Case {
    int radius = 0;
    int required = 0;
    std::vector<std::string> board;
};

struct Geometry {
    int radius, base, cells = 0;
    Bits target, halo;
    std::array<std::vector<Bits>, 3> rays;
    std::vector<std::pair<int, int>> coordinates;
    std::vector<std::array<int, 3>> sources;

    explicit Geometry(const Case& input) : radius(input.radius), base(2 * radius + 1) {
        if (radius < 1 || radius > 13)
            throw std::runtime_error("Supported board radius: 1..13 (512-cell bitsets)");
        const int height = 2 * radius + 1, width = 4 * radius + 1;
        if (static_cast<int>(input.board.size()) != height)
            throw std::runtime_error("Invalid board height");
        std::vector<std::vector<int>> indices(height, std::vector<int>(width, -1));
        for (int row = 0; row < height; ++row) {
            if (static_cast<int>(input.board[row].size()) != width)
                throw std::runtime_error("Invalid board width");
            for (int col = 0; col < width; ++col) {
                const char symbol = input.board[row][col];
                if (symbol != 'O' && symbol != 'M') continue;
                if (cells >= 512) throw std::runtime_error("Board exceeds 512 interior cells");
                indices[row][col] = cells;
                coordinates.emplace_back(row, col);
                if (symbol == 'M') target.set(cells);
                ++cells;
            }
        }
        if (!target.any()) throw std::runtime_error("Empty target pattern");
        constexpr std::array<std::pair<int, int>, 6> directions = {
            std::pair{0, 2}, {1, 1}, {1, -1}, {0, -2}, {-1, -1}, {-1, 1}};
        for (int i = 0; i < cells; ++i) {
            if (!target.test(i)) continue;
            const auto [row, col] = coordinates[i];
            for (const auto [dr, dc] : directions) {
                const int r = row + dr, c = col + dc;
                if (r >= 0 && r < height && c >= 0 && c < width && indices[r][c] >= 0)
                    halo.set(indices[r][c]);
            }
        }
        halo &= ~target;
        std::vector<std::vector<int>> rail(height, std::vector<int>(width, -1));
        constexpr std::array<std::pair<int, int>, 6> borderDirections = {
            std::pair{-1, 1}, {0, 2}, {1, 1}, {1, -1}, {0, -2}, {-1, -1}};
        int row = radius, col = 0, index = 0;
        for (const auto [dr, dc] : borderDirections) {
            for (int step = 0; step < radius; ++step) {
                rail[row][col] = index++;
                row += dr;
                col += dc;
            }
        }
        for (auto& axis : rays) axis.resize(base);
        for (int cell = 0; cell < cells; ++cell) {
            std::array<int, 3> source{};
            for (int axis = 0; axis < 3; ++axis) {
                auto [r, c] = coordinates[cell];
                const auto [dr, dc] = directions[axis];
                while (rail[r][c] < 0) {
                    r -= dr;
                    c -= dc;
                    if (r < 0 || r >= height || c < 0 || c >= width)
                        throw std::runtime_error("Nonhexagonal interior cell");
                }
                int offset = (rail[r][c] - axis * radius + 6 * radius) % (6 * radius);
                if (offset > 3 * radius) offset -= 6 * radius;
                if (offset <= -radius || offset >= radius)
                    throw std::runtime_error("Interior cell does not have an inward source");
                source[axis] = offset;
                rays[axis][offset + radius].set(cell);
            }
            sources.push_back(source);
        }
    }
};

inline int setupCost(const Config& state) {
    int result = 0;
    for (int value : state) result = std::max(result, std::abs(value));
    return result;
}

} // namespace l6
