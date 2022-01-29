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
#include <zlib.h>
#include <png.h>

#include <fstream>
#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <algorithm>
#include <numeric>

extern "C" {
#include <core/graphics.h>
#include <sim/display.h>
}

// when set to true, test that have no reference frames will save them and skip the test.
#define SAVE_REFERENCE false
// when set to true, all test results are written as png files.
#define SAVE_REFERENCE_PNG false

// maximum error masks to save per test, to avoid producing too much files.
static const size_t MAX_ERROR_MASKS = 3;

using Frame = std::array<uint8_t, DISPLAY_SIZE>;

class GraphicsTest : public ::testing::Test {
private:
    std::vector<Frame> frames;
    size_t current_frame;
    size_t error_masks;
    bool no_reference;

    static std::string get_frames_filename() {
        return std::string("ref/") +
               ::testing::UnitTest::GetInstance()->current_test_info()->name() + "_ref.dat";
    }

    void save_error_mask(const Frame& expected, const uint8_t* actual) const {
        std::ostringstream filename;
        filename << "output/" << ::testing::UnitTest::GetInstance()->current_test_info()->name()
                 << "_" << current_frame << ".png";
        std::filesystem::create_directories("output/");
        FILE* out = fopen(filename.str().c_str(), "wb");
        if (!out) {
            FAIL() << "couldn't save error mask";
        }

        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                      nullptr, nullptr, nullptr);
        png_infop info_ptr = png_create_info_struct(png_ptr);
        png_init_io(png_ptr, out);
        png_set_IHDR(png_ptr, info_ptr, DISPLAY_WIDTH, DISPLAY_HEIGHT, 8, PNG_COLOR_TYPE_RGBA,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_COMPRESSION_TYPE_DEFAULT);
        png_write_info(png_ptr, info_ptr);

        // create the error mask: transparent if same color, otherwise actual color
        png_byte row[DISPLAY_WIDTH * 4];
        size_t pos = 0;
        for (size_t y = 0; y < DISPLAY_HEIGHT; ++y) {
            png_byte* row_ptr = row;
            for (size_t col = 0; col < DISPLAY_NUM_COLS; ++col) {
                for (int nibble = 0; nibble < 2; ++nibble) {
                    uint8_t exp_color = (expected[pos] >> (nibble * 4)) & 0xf;
                    uint8_t act_color = (actual[pos] >> (nibble * 4)) & 0xf;
                    if (exp_color == act_color) {
                        row_ptr[0] = 0;
                        row_ptr[1] = 0;
                        row_ptr[2] = 0;
                        row_ptr[3] = 0;
                    } else {
                        act_color *= 17;
                        row_ptr[0] = act_color;
                        row_ptr[1] = act_color;
                        row_ptr[2] = act_color;
                        row_ptr[3] = 0xff;
                    }
                    row_ptr += 4;
                }
                ++pos;
            }
            png_write_row(png_ptr, row);
        }

        png_write_end(png_ptr, info_ptr);
        png_destroy_write_struct(&png_ptr, &info_ptr);

        fclose(out);
    }

public:
    GraphicsTest() : current_frame(0), error_masks(0), no_reference(false) {}

    template<typename T>
    void do_test(const T& test, const std::string& name = "") {
        display_first_page();
        do {
            graphics_clear(DISPLAY_COLOR_BLACK);
            test();
        } while (display_next_page());

        if (SAVE_REFERENCE_PNG) {
            std::ostringstream filename;
            filename << "output/" << ::testing::UnitTest::GetInstance()->current_test_info()->name()
                     << "_" << current_frame;
            if (!name.empty()) {
                filename << "_" << name;
            }
            filename << ".png";
            std::filesystem::create_directories("output/");
            FILE* out = fopen(filename.str().c_str(), "wb");
            display_save(out);
            fclose(out);
        }

        if (no_reference) {
            // don't compare, only save the result.
            Frame& frame = frames.emplace_back();
            std::copy_n(display_data(), DISPLAY_SIZE, frame.begin());
        } else {
            if (current_frame >= frames.size()) {
                FAIL() << "not enough reference frames";
            }

            // compare result with reference frame at same position.
            const Frame& expected = frames[current_frame];
            if (!std::equal(expected.begin(), expected.end(), display_data())) {
                // frame is different
                ADD_FAILURE() << "difference in frame " << current_frame <<
                              (name.empty() ? "" : std::string(" (") + name + ")");

                // save error mask if max not reached
                if (error_masks < MAX_ERROR_MASKS) {
                    save_error_mask(expected, display_data());
                    ++error_masks;
                }
            }
        }
        ++current_frame;
    }

protected:
    void SetUp() override {
        if (SAVE_REFERENCE) {
            no_reference = true;
            return;
        }

        std::ifstream in(get_frames_filename(), std::ios::binary);
        if (!in.good()) {
            // no reference frames
            no_reference = true;
            return;
        }

        z_stream strm;
        unsigned char in_buf[DISPLAY_SIZE];
        unsigned char out_buf[DISPLAY_SIZE];
        strm.zalloc = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;
        int ret = inflateInit(&strm);
        if (ret != Z_OK) {
            return;
        }

        Frame frame;
        auto frame_it = frame.begin();
        do {
            strm.avail_in = in.readsome((char*) in_buf, sizeof in_buf);
            strm.next_in = in_buf;
            do {
                // read enough bytes to complete the current frame
                // if not enough bytes are available, more will be read next time.
                size_t bytes_left = frame.end() - frame_it;
                strm.avail_out = bytes_left;
                strm.next_out = out_buf;
                ret = inflate(&strm, Z_NO_FLUSH);
                assert(ret != Z_STREAM_ERROR);
                size_t count = bytes_left - strm.avail_out;
                std::copy_n(out_buf, count, frame_it);
                frame_it += count;
                if (frame_it == frame.end()) {
                    // end of frame, add it.
                    frames.push_back(frame);
                    frame_it = frame.begin();
                }
            } while (strm.avail_out == 0);
        } while (ret != Z_STREAM_END);
        if (frame_it != frame.begin() && frame_it != frame.end()) {
            inflateEnd(&strm);
            FAIL() << "partial reference frame in file";
        } else {
            frames.push_back(frame);
        }
        inflateEnd(&strm);
        in.close();
    }

    void TearDown() override {
        if (!no_reference) {
            return;
        }

        std::ofstream out(get_frames_filename(), std::ios::binary);
        std::filesystem::create_directories("ref/");

        // initialize zlib state & buffers
        z_stream strm;
        unsigned char in_buf[DISPLAY_SIZE];
        unsigned char out_buf[DISPLAY_SIZE];
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        int ret = deflateInit(&strm, Z_BEST_COMPRESSION);
        if (ret != Z_OK) {
            return;
        }

        // write all reference frames in order
        for (size_t i = 0; i < frames.size(); ++i) {
            std::copy_n(frames[i].begin(), DISPLAY_SIZE, in_buf);
            strm.avail_in = DISPLAY_SIZE;
            strm.next_in = in_buf;
            do {
                strm.avail_out = sizeof out_buf;
                strm.next_out = out_buf;
                ret = deflate(&strm, i == frames.size() - 1 ? Z_FINISH : Z_NO_FLUSH);
                assert(ret != Z_STREAM_ERROR);
                out.write((const char*) out_buf,
                          (std::streamsize) (sizeof out_buf - strm.avail_out));
            } while (strm.avail_out == 0);
        }
        deflateEnd(&strm);
        out.close();
        GTEST_SKIP();
    }
};

static std::vector<uint8_t> load_asset(const std::string& filename) {
    std::vector<uint8_t> data;
    std::ifstream in(std::string("assets/") + filename);
    if (!in.good()) {
        return data;
    }

    size_t read;
    do {
        char buf[8192];
        read = in.readsome(buf, sizeof buf);
        for (size_t i = 0; i < read; ++i) {
            data.push_back(buf[i]);
        }
    } while (read != 0);
    in.close();
    return data;
}

TEST_F(GraphicsTest, graphics_pixel) {
    do_test([=]() {
        uint32_t seed = 1;
        for (int i = 0; i < 1000; ++i) {
            graphics_set_color(seed % 16);
            disp_x_t x = seed % DISPLAY_WIDTH;
            disp_x_t y = seed % DISPLAY_HEIGHT;
            graphics_pixel(x, y);
            seed = 1103515245 * seed + 12345;
        }
    });
}

TEST_F(GraphicsTest, graphics_hline) {
    // full-width lines
    graphics_set_color(DISPLAY_COLOR_WHITE);
    for (disp_y_t i = 0; i < DISPLAY_HEIGHT; ++i) {
        do_test([=]() {
            graphics_set_color(15);
            graphics_hline(0, DISPLAY_WIDTH - 1, i);
        });
        do_test([=]() {
            graphics_set_color(10);
            graphics_hline(DISPLAY_WIDTH - 1, 0, i);
        });
    }
    // varying width lines
    for (disp_y_t i = 0; i < DISPLAY_HEIGHT; ++i) {
        do_test([=]() {
            graphics_set_color(5);
            graphics_hline(i, DISPLAY_WIDTH - i - 1, i);
        });
    }
}

TEST_F(GraphicsTest, graphics_vline) {
    // full-height lines
    for (disp_x_t i = 0; i < DISPLAY_WIDTH; ++i) {
        do_test([=]() {
            graphics_set_color(15);
            graphics_vline(0, DISPLAY_HEIGHT - 1, i);
        });
        do_test([=]() {
            graphics_set_color(10);
            graphics_vline(DISPLAY_HEIGHT - 1, 0, i);
        });
    }
    // varying width lines
    for (disp_x_t i = 0; i < DISPLAY_WIDTH; ++i) {
        do_test([=]() {
            graphics_set_color(5);
            graphics_vline(i, DISPLAY_HEIGHT - i - 1, i);
        });
    }
}

TEST_F(GraphicsTest, graphics_line) {
    graphics_set_color(DISPLAY_COLOR_WHITE);
    // diagonal lines
    for (int i = 0; i <= 56; i += 8) {
        for (disp_x_t j = i; j < (disp_x_t) (DISPLAY_WIDTH - i); j += 4) {
            uint8_t a = DISPLAY_WIDTH - i - 1;
            uint8_t b = DISPLAY_HEIGHT - j - 1;
            do_test([=]() { graphics_line(j, a, b, i); });  // octants 2 & 3
            do_test([=]() { graphics_line(a, b, i, j); });  // octants 4 & 5
            do_test([=]() { graphics_line(b, i, j, a); });  // octants 6 & 7
            do_test([=]() { graphics_line(i, j, a, b); });  // octants 8 & 1
        }
    }
    // horizontal & vertical lines
    for (disp_y_t i = 0; i < DISPLAY_HEIGHT; ++i) {
        do_test([=]() { graphics_line(i, i, DISPLAY_WIDTH - i - 1, i); });
        do_test([=]() { graphics_line(i, i, i, DISPLAY_WIDTH - i - 1); });
    }
    // short lines
    do_test([=]() { graphics_line(0, 0, 0, 0); });
    do_test([=]() { graphics_line(0, 0, 1, 1); });
    do_test([=]() { graphics_line(0, 0, 1, 0); });
    do_test([=]() { graphics_line(0, 0, 2, 1); });
    do_test([=]() { graphics_line(0, 0, 1, 2); });
}

template<typename T>
static void do_rect_test(GraphicsTest& test, T draw_rect) {
    // generic test for graphics rectangle function.
    graphics_set_color(DISPLAY_COLOR_WHITE);
    const uint8_t step = 32;  // =100 frames
    for (disp_x_t y0 = 0; y0 < DISPLAY_HEIGHT; y0 += step) {
        for (disp_x_t x0 = 0; x0 < DISPLAY_WIDTH; x0 += step) {
            for (disp_x_t y1 = y0; y1 < DISPLAY_WIDTH; y1 += step) {
                for (disp_x_t x1 = x0; x1 < DISPLAY_WIDTH; x1 += step) {
                    test.do_test([=]() { draw_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1); });
                }
            }
        }
    }
    // other special cases not covered above
    test.do_test([=]() { draw_rect(0, 0, 128, 128); });
    test.do_test([=]() { draw_rect(0, 0, 1, 128); });
    test.do_test([=]() { draw_rect(0, 0, 128, 1); });
    test.do_test([=]() { draw_rect(0, 127, 128, 1); });
    test.do_test([=]() { draw_rect(127, 0, 1, 128); });
}

TEST_F(GraphicsTest, graphics_rect) {
    do_rect_test(*this, graphics_rect);
}

TEST_F(GraphicsTest, graphics_fill_rect) {
    do_rect_test(*this, graphics_fill_rect);
}

static void do_image_test(GraphicsTest &test, const std::string &asset) {
    auto image_data = load_asset(asset);
    auto image_ptr = data_mcu(image_data.data());
    graphics_set_color(12);
    // 128x128 region
    for (int top = 0; top <= 128; top += 32) {
        for (int left = 0; left <= 128; left += 32) {
            test.do_test([=]() {
                graphics_image_region(image_ptr, 0, 0, left, top, left + 127, top + 127);
            });
        }
    }
    // 32x32 region
    for (int x = 0; x < DISPLAY_WIDTH; x += 32) {
        for (int y = 0; y < DISPLAY_HEIGHT; y += 32) {
            test.do_test([=]() {
                graphics_image_region(image_ptr, x, y, x + 64, y + 64, x + 95, y + 95);
            });
        }
    }

}

TEST_F(GraphicsTest, graphics_image_1bit) {
    do_image_test(*this, "image256x256-1bit.dat");
}

TEST_F(GraphicsTest, graphics_image_1bit_indexed) {
    do_image_test(*this, "image256x256-1bit-indexed.dat");
}

TEST_F(GraphicsTest, graphics_image_4bit) {
    do_image_test(*this, "image256x256-4bit.dat");
}

TEST_F(GraphicsTest, graphics_image_4bit_indexed) {
    do_image_test(*this, "image256x256-4bit-indexed.dat");
}

static void do_glyph_test(GraphicsTest& test, const std::string& asset, size_t num_chars) {
    auto font_data = load_asset(asset);
    graphics_set_font(data_mcu(font_data.data()));
    graphics_set_color(DISPLAY_COLOR_WHITE);

    // print all glyphs in pages
    uint8_t grid_width = graphics_text_width(" ") + 1;
    uint8_t grid_height = graphics_text_max_height() + 1;
    size_t num_cols = DISPLAY_WIDTH / grid_width;
    size_t num_rows = DISPLAY_HEIGHT / grid_height;
    size_t num_pages = std::ceil((double) num_chars / (double) (num_cols * num_rows));
    std::vector<char> charset(196, 0);
    std::iota(charset.begin(), charset.begin() + 95, 0x21);
    std::iota(charset.begin() + 95, charset.end(), 0xa0);
    size_t pos = 0;
    for (size_t page = 0; page < num_pages; ++page) {
        test.do_test([=]() {
            size_t curr = pos;
            for (size_t y = 0; y < num_rows; ++y) {
                for (size_t x = 0; x < num_cols; ++x) {
                    graphics_glyph((int8_t) (x * grid_width), (int8_t) (y * grid_height),
                                   charset[curr++]);
                    if (curr == num_chars) {
                        return;
                    }
                }
            }
        });
        pos += num_rows * num_cols;
    }

    // print glyph on screen border (partially hidden)
    const char glyph = '0';
    for (int8_t i = 0; i < (int8_t) std::max(grid_width, grid_height); ++i) {
        test.do_test([=]() {
            graphics_glyph((int8_t) -i, 56, glyph);
            graphics_glyph((int8_t) std::min(DISPLAY_WIDTH - 1,
                                             DISPLAY_WIDTH - grid_width + i), 56, glyph);
            graphics_glyph(56, (int8_t) -i, glyph);
            graphics_glyph(56, (int8_t) std::min(DISPLAY_HEIGHT - 1,
                                                 DISPLAY_HEIGHT - grid_width + i), glyph);
        });
    }

    // all chars <= 0x20 should print nothing
    test.do_test([]() {
        for (char i = '\0'; i <= ' '; ++i) {
            graphics_glyph(0, 0, i);
        }
    });
}

TEST_F(GraphicsTest, graphics_glyph) {
    do_glyph_test(*this, "font5x7.dat", 191);
    do_glyph_test(*this, "font6x9.dat", 25);
    do_glyph_test(*this, "font7x7.dat", 58);
    do_glyph_test(*this, "font16x16.dat", 96);
}

TEST_F(GraphicsTest, graphics_text) {
    auto font_data = load_asset("font5x7.dat");
    graphics_set_font(data_mcu(font_data.data()));
    graphics_set_color(DISPLAY_COLOR_WHITE);

    const char* text = "Hello world!";
    for (int y = -16; y < DISPLAY_HEIGHT; y += 7) {
        for (int x = -64; x < DISPLAY_WIDTH; x += 27) {
            do_test([=]() {
                graphics_text((int8_t) x, (int8_t) y, text);
            });
        }
    }
}

TEST_F(GraphicsTest, graphics_text_wrap) {
    auto font_data = load_asset("font5x7.dat");
    graphics_set_font(data_mcu(font_data.data()));
    graphics_set_color(DISPLAY_COLOR_WHITE);

    const char* text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
                       "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
                       "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex "
                       "ea commodo consequat. ";
    for (int y = -16; y < DISPLAY_HEIGHT; y += 14) {
        for (int x = -64; x < DISPLAY_WIDTH; x += 49) {
            int wrap_first = std::max((int) std::ceil(x / 32.0) * 32, 32);
            for (int wrap = wrap_first; wrap <= DISPLAY_WIDTH; wrap += 32) {
                std::ostringstream name;
                name << y << "_" << x << "_" << wrap;
                do_test([=]() {
                    graphics_text_wrap((int8_t) x, (int8_t) y, (uint8_t) wrap, text);
                }, name.str());
            }
        }
    }
}