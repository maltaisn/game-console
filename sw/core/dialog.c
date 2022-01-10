
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

#include <sys/input.h>

#include <core/dialog.h>
#include <core/trace.h>

// dialog-arrow-left.png, 3x5, 1-bit.
static const uint8_t ARROW_LEFT[] = {0x01, 0x02, 0x04, 0x17, 0x6c, 0x40};
// dialog-arrow-right.png, 3x5, 1-bit.
static const uint8_t ARROW_RIGHT[] = {0x01, 0x02, 0x04, 0x4d, 0x7a, 0x00};

dialog_t dialog;

void dialog_init(disp_x_t x, disp_y_t y, uint8_t width, uint8_t height) {
    dialog.x = x;
    dialog.y = y;
    dialog.width = width;
    dialog.height = height;
    dialog.dismissable = false;
    dialog.title = 0;
    dialog.pos_btn = 0;
    dialog.neg_btn = 0;
    dialog.pos_result = DIALOG_RESULT_NONE;
    dialog.neg_result = DIALOG_RESULT_NONE;
    dialog.dismiss_result = DIALOG_RESULT_NONE;
    dialog.item_count = 0;
    dialog.selection = DIALOG_SELECTION_NONE;
}

void dialog_set_font(graphics_font_t title_font, graphics_font_t action_font,
                     graphics_font_t item_font) {
    dialog.title_font = title_font;
    dialog.action_font = action_font;
    dialog.item_font = item_font;
}

static bool dialog_add_item_check(void) {
#ifdef RUNTIME_CHECKS
    if (dialog.item_count == DIALOG_MAX_ITEMS) {
        trace("dialog already reached maximum number of items.");
        return false;
    }
#endif
    return true;
}

void dialog_add_item_button(const char* name, dialog_result_t result) {
    if (!dialog_add_item_check()) {
        return;
    }
    dialog_item_t* item = &dialog.items[dialog.item_count];
    item->type = DIALOG_ITEM_BUTTON;
    item->name = name;
    item->button.result = result;
    ++dialog.item_count;
}

void dialog_add_item_choice(const char* name, uint8_t selection,
                            uint8_t choices_count, const char** choices) {
    if (!dialog_add_item_check()) {
        return;
    }
    dialog_item_t* item = &dialog.items[dialog.item_count];
    item->type = DIALOG_ITEM_CHOICE;
    item->name = name;
    item->choice.choices_count = choices_count;
    item->choice.selection = selection;
    item->choice.choices = choices;
    ++dialog.item_count;
}

void dialog_add_item_number(const char* name, uint8_t min, uint8_t max,
                            uint8_t mul, uint8_t value) {
#ifdef RUNTIME_CHECKS
    if (!dialog_add_item_check()) {
        return;
    }
    if (max < min || value < min || value > max) {
        trace("invalid number item values");
        return;
    }
#endif
    dialog_item_t* item = &dialog.items[dialog.item_count];
    item->type = DIALOG_ITEM_NUMBER;
    item->name = name;
    item->number.value = value;
    item->number.min = min;
    item->number.max = max;
    item->number.mul = mul;
    ++dialog.item_count;
}

dialog_result_t dialog_handle_input(uint8_t last_state) {
    dialog_item_t* curr_item = dialog.selection >= dialog.item_count ? 0 :
                               &dialog.items[dialog.selection];
    dialog_result_t result = DIALOG_RESULT_NONE;

#ifdef RUNTIME_CHECKS
    if (dialog.neg_btn && !dialog.pos_btn) {
        trace("cannot have negative button without positive button.");
        return result;
    }
#endif

    uint8_t clicked = ~last_state & input_get_state();
    if (clicked) {
        if (clicked & DIALOG_BUTTON_ENTER) {
            if (dialog.selection == DIALOG_SELECTION_POS) {
                result = dialog.pos_result;
            } else if (dialog.selection == DIALOG_SELECTION_NEG) {
                result = dialog.neg_result;
            } else if (curr_item && curr_item->type == DIALOG_ITEM_BUTTON) {
                result = curr_item->button.result;
            } else if (curr_item) {
                // choice option, go to next option or dialog button.
                if (dialog.selection == dialog.item_count - 1) {
                    if (dialog.pos_btn) {
                        dialog.selection = DIALOG_SELECTION_POS;
                    }
                } else {
                    ++dialog.selection;
                }
            }

        } else if (clicked & DIALOG_BUTTON_DISMISS) {
            if (dialog.dismissable) {
                if (dialog.dismiss_result == DIALOG_RESULT_NONE) {
                    result = dialog.neg_result;
                } else {
                    result = dialog.dismiss_result;
                }
            }

        } else if (clicked & DIALOG_BUTTON_UP) {
            if (dialog.selection >= DIALOG_SELECTION_NEG) {
                // positive or negative button selected, go to last item.
                if (dialog.item_count != 0) {
                    dialog.selection = dialog.item_count - 1;
                }
            } else if (dialog.selection != 0) {
                // go to previous item if not already on first.
                --dialog.selection;
            }

        } else if (clicked & DIALOG_BUTTON_DOWN) {
            if (dialog.selection < dialog.item_count - 1) {
                ++dialog.selection;
            } else if (dialog.selection == dialog.item_count - 1 && dialog.pos_btn) {
                dialog.selection = DIALOG_SELECTION_POS;
            }

        } else if (clicked & DIALOG_BUTTON_LEFT) {
            if (dialog.selection == DIALOG_SELECTION_POS && dialog.neg_btn) {
                // there's a negative button on the left of the positive button, select it.
                dialog.selection = DIALOG_SELECTION_NEG;
            } else if (curr_item) {
                if (curr_item->type == DIALOG_ITEM_CHOICE) {
                    // go to previous choice in item or wrap around if on first choice.
                    dialog_choice_t* choice = &curr_item->choice;
                    if (choice->selection == 0) {
                        choice->selection = choice->choices_count - 1;
                    } else {
                        --choice->selection;
                    }
                } else if (curr_item->type == DIALOG_ITEM_NUMBER) {
                    // decrement the number by one.
                    dialog_number_t* number = &curr_item->number;
                    if (number->value > number->min) {
                        --number->value;
                    }
                }
            }

        } else if (clicked & DIALOG_BUTTON_RIGHT) {
            if (dialog.selection == DIALOG_SELECTION_NEG) {
                // if there's a negative button there's necessarily a positive one too.
                dialog.selection = DIALOG_SELECTION_POS;
            } else if (curr_item) {
                if (curr_item->type == DIALOG_ITEM_CHOICE) {
                    // go to next choice in item or wrap around if on last choice.
                    dialog_choice_t* choice = &curr_item->choice;
                    ++choice->selection;
                    if (choice->selection == choice->choices_count) {
                        choice->selection = 0;
                    }
                } else if (curr_item->type == DIALOG_ITEM_NUMBER) {
                    // increment the number by one.
                    dialog_number_t* number = &curr_item->number;
                    if (number->value < number->max) {
                        ++number->value;
                    }
                }
            }
        }
    }

    return result;
}

static void draw_action(disp_color_t color, disp_x_t x, disp_y_t y, uint8_t width,
                        uint8_t height, const char* text, bool selected, bool inactive_frame) {
    if (selected) {
        graphics_set_color(color);
        graphics_fill_rect(x, y, width, height);
        graphics_set_color(DISPLAY_COLOR_BLACK);
    } else {
        graphics_set_color(DISPLAY_COLOR_BLACK);
        graphics_fill_rect(x, y, width, height);
        graphics_set_color(color);
        if (inactive_frame) {
            graphics_rect(x, y, width, height);
        }
    }
    const uint8_t text_width = graphics_text_width(text);
    graphics_text((int8_t) (x + (width - text_width) / 2), (int8_t) (y + 2), text);
}

/**
 * Format 0-255 number to char buffer.
 * Returns a pointer within the buffer to start of the formatted. string
 */
static const char* uint8_to_str(char* buf, uint8_t n) {
    buf[3] = '\0';
    char* ptr = &buf[3];
    do {
        *(--ptr) = (char) (n % 10 + '0');
        n /= 10;
    } while (n);
    return ptr;
}

void dialog_draw(void) {
    // title frame & text
    disp_y_t y = dialog.y;
    int8_t height = (int8_t) dialog.height;
    if (dialog.title) {
        graphics_set_font(dialog.title_font);
        uint8_t h = graphics_text_height() + 5;
        y += h;
        height = (int8_t) (height - h);

        // title background
        graphics_set_color(11);
        graphics_fill_rect(dialog.x, dialog.y, dialog.width, h - 1);
        // title text
        graphics_set_color(DISPLAY_COLOR_BLACK);
        uint8_t width = graphics_text_width(dialog.title);
        graphics_text((int8_t) (dialog.x + (dialog.width - width) / 2), (int8_t) (dialog.y + 2),
                      dialog.title);
        // line between title frame and dialog content
        graphics_hline(dialog.x, dialog.x + dialog.width, y - 1);
    }

    // background color
    if (height > 2) {
        graphics_set_color(DISPLAY_COLOR_BLACK);
        graphics_fill_rect(dialog.x + 1, y + 1, dialog.width - 2, height - 2);
    }

    // action buttons
    graphics_set_font(dialog.action_font);
    uint8_t action_height = graphics_text_height() + 4;
    if (dialog.pos_btn) {
        height = (int8_t) (height - action_height - 1);

        // line between dialog content and action buttons
        graphics_set_color(DISPLAY_COLOR_BLACK);
        graphics_hline(dialog.x, dialog.x + dialog.width, y + height);

        disp_y_t btn_y = dialog.y + dialog.height - action_height;
        disp_x_t pos_btn_x = dialog.x;
        uint8_t pos_btn_width = dialog.width;
        if (dialog.neg_btn) {
            // negative button
            pos_btn_x += dialog.width / 2 + 1;
            pos_btn_width = pos_btn_width / 2 - 1;
            draw_action(11, dialog.x, btn_y, dialog.width / 2, action_height,
                        dialog.neg_btn, dialog.selection == DIALOG_SELECTION_NEG, false);
            // line between the two buttons
            graphics_set_color(DISPLAY_COLOR_BLACK);
            graphics_vline(btn_y, btn_y + action_height, pos_btn_x - 1);
        }
        // positive button
        draw_action(11, pos_btn_x, btn_y, pos_btn_width, action_height,
                    dialog.pos_btn, dialog.selection == DIALOG_SELECTION_POS, false);
    }

    // button items, choice display (current font is action font)
    disp_y_t action_y = y + 3;
    for (uint8_t i = 0; i < dialog.item_count; ++i) {
        dialog_item_t* item = &dialog.items[i];
        if (item->type == DIALOG_ITEM_BUTTON) {
            draw_action(DISPLAY_COLOR_WHITE, dialog.x + 4, action_y, dialog.width - 8,
                        action_height, item->name, dialog.selection == i, true);
        } else {
            const char* choice_str;
            if (item->type == DIALOG_ITEM_NUMBER) {
                char buf[4];
                choice_str = uint8_to_str(buf, item->number.value * item->number.mul);
            } else {
                choice_str = item->choice.choices[item->choice.selection];
            }

            uint8_t choice_width = graphics_text_width(choice_str);
            uint8_t arrow_right_x = dialog.x + dialog.width - 6;
            uint8_t action_x = arrow_right_x - choice_width - 3;
            // action text (number or choice)
            draw_action(DISPLAY_COLOR_WHITE, action_x, action_y,
                        choice_width + 2, action_height, choice_str, dialog.selection == i, false);

            // draw arrows on the left and right side of choice item.
            uint8_t arrow_y = action_y + (action_height - 5) / 2;
            graphics_set_color(DISPLAY_COLOR_WHITE);
            graphics_image(data_mcu(ARROW_RIGHT), arrow_right_x, arrow_y);
            graphics_image(data_mcu(ARROW_LEFT), action_x - 4, arrow_y);
        }
        action_y += action_height + 2;
    }

    // item names
    graphics_set_font(dialog.item_font);
    graphics_set_color(DISPLAY_COLOR_WHITE);
    action_y = y + 3 + ((int8_t) action_height - graphics_text_height()) / 2;
    for (uint8_t i = 0; i < dialog.item_count; ++i) {
        dialog_item_t* item = &dialog.items[i];
        if (item->type != DIALOG_ITEM_BUTTON) {
            graphics_text((int8_t) (dialog.x + 3), (int8_t) action_y, item->name);
        }
        action_y += action_height + 2;
    }

    // dialog outline
    if (height >= 2) {
        // dialog inner outline (only draw if high enough too appear)
        graphics_set_color(DISPLAY_COLOR_WHITE);
        graphics_rect(dialog.x, y, dialog.width, height);
    }
    // dialog outer outline
    graphics_set_color(DISPLAY_COLOR_BLACK);
    graphics_rect(dialog.x - 1, dialog.y - 1, dialog.width + 2, dialog.height + 2);
}
