#pragma once

#include <array>
#include <memory>
#include <functional>
#include <numeric>
#include <deque>
#include <cstdint>
#include <vector>

namespace asp {

    struct sample_tile {
        std::uint32_t x;
        std::uint32_t y;
        std::uint32_t width;
        std::uint32_t height;
        float noise;
    };

    struct analyzed_noise {
        float max_noise = 0;
        std::vector<sample_tile> tiles{};
    };

    struct noise_buffer {
        float *noise;
        float max_noise;
        std::uint32_t width;
        std::uint32_t height;
    };

    struct tree_tile {

    };

    namespace detail {
        struct tile_node {
        private:
            [[nodiscard]] std::array<std::unique_ptr<tile_node>, 4> _split(const float *noise, int img_width) const {
                const auto min_x = x;
                const auto min_y = y;
                const auto mid_x = x + width / 2;
                const auto mid_y = y + height / 2;
                const auto node_width = width / 2;
                const auto node_height = height / 2;

                auto split_nodes = std::array<std::unique_ptr<tile_node>, 4>();
                for (auto &node : split_nodes) {
                    node = std::make_unique<tile_node>();
                    node->width = node_width;
                    node->height = node_height;
                }
                split_nodes[0]->x = min_x;
                split_nodes[0]->y = min_y;
                split_nodes[1]->x = mid_x;
                split_nodes[1]->y = min_y;
                split_nodes[2]->x = min_x;
                split_nodes[2]->y = mid_y;
                split_nodes[3]->x = mid_x;
                split_nodes[3]->y = mid_y;

                for (auto &node : split_nodes)
                    node->_calc_cost(noise, img_width);

                return split_nodes;
            }

            void _calc_cost(const float *noise, int img_width) {
                float sum = 0.0f;
                for (auto i = y; i < y + height; i++)
                    for (auto j = x; j < x + width; j++)
                        sum += noise[j + i * img_width];
                sum /= float(width * height);
                noise_cost = sum;
            }

        public:
            std::uint32_t x;
            std::uint32_t y;
            std::uint32_t width;
            std::uint32_t height;
            float noise_cost = -1;

            void collect_children(std::vector<sample_tile> &tiles) const noexcept {
                if (children[0] == nullptr) {
                    auto tile = sample_tile();
                    tile.x = x;
                    tile.y = y;
                    tile.width = width;
                    tile.height = height;

                    return tiles.push_back(tile);
                }

                for (const auto &child : children)
                    child->collect_children(tiles);
            }

            void split(const float *noise, int img_width, int current_depth) {
                if (current_depth > 8)
                    return;

                _calc_cost(noise, img_width);
                auto split_children = _split(noise, img_width);

                auto children_deltas = std::array<float, 4>();
                for (auto i = 0; i < 4; i++) {
                    // Find larger number
                    const auto max = std::max(split_children[i]->noise_cost, noise_cost);
                    const auto min = std::min(split_children[i]->noise_cost, noise_cost);

                    // Find percentile difference between two
                    const auto delta = min / max;
                    children_deltas[i] = delta;
                    // At this point we're at a 0..1 -> 0..100% mapping of how close the numbers are to each other
                }

                // Pass to see if the deltas are too different, if they are different then we will split.
                auto different = false;
                const auto avg = std::accumulate(children_deltas.begin(), children_deltas.end(), 0.0f) / 4.0f;
                for (auto i = 0; i < 4; i++) {
                    const auto delta = children_deltas[i];

                    const auto max = std::max(delta, avg);
                    const auto min = std::min(delta, avg);
                    const auto d = (min / max);

                    if (d < 0.8f)
                        different |= true;
                }

                if (different) {
                    // This is good, it means that splitting will result in a better split
                    children = std::move(split_children);
                    for (auto &child : children)
                        child->split(noise, img_width, current_depth + 1);
                }

            }

            std::array<std::unique_ptr<tile_node>, 4> children;
        };


        [[nodiscard]] inline float noise_at(const float *noise, int x, int y, int width, int height, int img_width) {
            float sum = 0.0f;
            for (auto i = y; i < y + height; i++)
                for (auto j = x; j < x + width; j++)
                    sum += noise[j + i * img_width];
            sum /= float(width * height);
            return sum;
        }

        [[nodiscard]] inline float
        surface_area_merged(const asp::sample_tile &left, const asp::sample_tile &right, asp::sample_tile &merged) {
            const auto min_x = std::min(left.x, right.x);
            const auto min_y = std::min(left.y, right.y);

            const auto max_x = std::max(left.x + left.width, right.x + right.width);
            const auto max_y = std::max(left.y + left.height, right.y + right.height);

            const auto width = max_x - min_x;
            const auto height = max_y - min_y;

            merged.x = min_x;
            merged.y = min_y;
            merged.width = width;
            merged.height = height;

            return width * height;
        }

    }

    [[nodiscard]] inline analyzed_noise analyzed_noise_bottom_up(noise_buffer buffer) {
        auto analyzed = analyzed_noise();

        constexpr auto tile_size = 16;
        const auto bottom_level_count_x = buffer.width / tile_size;
        const auto bottom_level_count_y = buffer.height / tile_size;
        auto nodes = std::vector<asp::sample_tile>();
        nodes.reserve(bottom_level_count_x + bottom_level_count_y);

        float max = 0.0f;

        for (auto x = 0; x < buffer.width; x += tile_size)
            for (auto y = 0; y < buffer.height; y += tile_size) {
                const auto width = x + tile_size >= buffer.width ? buffer.width - x : tile_size;
                const auto height = y + tile_size >= buffer.height ? buffer.height - y : tile_size;

                auto tile = asp::sample_tile();
                tile.x = x;
                tile.y = y;
                tile.width = width;
                tile.height = height;

                tile.noise = detail::noise_at(buffer.noise, x, y, width, height, buffer.width);
                max = std::max(tile.noise, max);

                nodes.push_back(tile);
            }

//        auto tile = asp::sample_tile();
//            tile.x = 0;
//            tile.y = 0;
//            tile.width = buffer.width / 2;
//            tile.height = buffer.height;
//        nodes.push_back(tile);
//        tile.x += tile.width;
//        nodes.push_back(tile);


        // After creating the lowest level ones, go through each and merge
        auto to_merge = std::deque<asp::sample_tile>();
        auto next_to_merge = std::deque<asp::sample_tile>();

        for (auto node : nodes)
            to_merge.push_back(node);

//        nodes.clear(); // We use nodes for the finished ones

        while (false) {
            if (to_merge.size() <= 1) {
                if (to_merge.size() == 1) {
                    nodes.push_back(to_merge.front());
                    to_merge.pop_front();
                }

                if (next_to_merge.empty()) break; // We've finished
                std::swap(to_merge, next_to_merge);
                continue;
            }

            auto current = to_merge.front();
            to_merge.pop_front();


            auto best_merged = asp::sample_tile();
            auto merged_with = size_t();
            auto best_sah = std::numeric_limits<float>::infinity();

            for (auto i = 0; i < to_merge.size(); i++) {
                const auto node = to_merge[i];
                auto merged = asp::sample_tile();
                // Get the theoretical merged node
                const auto sah = detail::surface_area_merged(current, node, merged);
                if (sah < best_sah) {
                    merged_with = i;
                    best_merged = merged;
                    best_sah = sah;
                }
            }

            if (best_sah != std::numeric_limits<float>::infinity()) {
                const auto noise = detail::noise_at(buffer.noise, best_merged.x, best_merged.y, best_merged.width,
                                                    best_merged.height, buffer.width);

                best_merged.noise = noise;
                if (std::abs(noise - current.noise) < 0.2) {
                    to_merge.erase(to_merge.begin() + merged_with);
                    next_to_merge.push_back(best_merged);
                } else {
                    nodes.push_back(current);
                }
            } else {
                nodes.push_back(current);
            }
        }

        analyzed.tiles = nodes;

        return analyzed;
    }

    [[nodiscard]] inline analyzed_noise analyse_noise(noise_buffer buffer) {
        auto analyzed = analyzed_noise();

        auto tile = detail::tile_node();
        tile.x = 0;
        tile.y = 0;
        tile.width = buffer.width;
        tile.height = buffer.height;
        tile.split(buffer.noise, buffer.width, 0);

        tile.collect_children(analyzed.tiles);
//        analyzed.tiles = bottom_level;
//        analyzed.max_noise = max;

        return analyzed;
    }

}

/*
        constexpr auto tile_size = 16;
        const auto bottom_level_count_x = buffer.width / tile_size;
        const auto bottom_level_count_y = buffer.height / tile_size;
        auto bottom_level = std::vector<asp::sample_tile>();
        bottom_level.reserve(bottom_level_count_x + bottom_level_count_y);

        float max = 0.0f;

        for (auto x = 0; x < buffer.width; x += tile_size)
            for (auto y = 0; y < buffer.height; y += tile_size) {
                const auto width = x + tile_size >= buffer.width ? buffer.width - x : tile_size;
                const auto height = y + tile_size >= buffer.height ? buffer.height - y : tile_size;

                auto tile = asp::sample_tile();
                tile.x = x;
                tile.y = y;
                tile.width = width;
                tile.height = height;

                tile.noise = detail::noise_at(buffer.noise, x, y, width, height, buffer.width);
                max = std::max(tile.noise, max);

                bottom_level.push_back(tile);
            }
 */
