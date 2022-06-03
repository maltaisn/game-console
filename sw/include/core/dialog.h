
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

#ifndef CORE_DIALOG_H
#define CORE_DIALOG_H

/*
 * The dialog library provides a very basic UI dialog with a title, action buttons,
 * and a list of items: buttons, choice list, number picker and text field.
 * Dialogs can be customized by drawing on top of the content area and handling user input
 * appropriatedly while the dialog is shown.
 *
 * To reduce code size, the following defines are available to disable some features:
 * - DIALOG_NO_CHOICE: disable choice items
 * - DIALOG_NO_NUMBER: disable number items
 * - DIALOG_NO_TEXT: disable text field items
 * - DIALOG_MAX_ITEMS: indicates the maximum number of items that will be used (limits RAM usage).
 */

#include <core/display.h>
#include <core/input.h>

#include <core/graphics.h>

#include <stdint.h>

#ifndef DIALOG_MAX_ITEMS
#define DIALOG_MAX_ITEMS 5
#endif

#if defined(DIALOG_NO_CHOICE) && defined(DIALOG_NO_NUMBER) && defined(DIALOG_NO_TEXT)
#define DIALOG_NO_ITEM_TEXT
#endif

// keybindings in a dialog
#define DIALOG_BUTTON_LEFT BUTTON1
#define DIALOG_BUTTON_RIGHT BUTTON5
#define DIALOG_BUTTON_UP BUTTON2
#define DIALOG_BUTTON_DOWN BUTTON3
#define DIALOG_BUTTON_ENTER BUTTON4
#define DIALOG_BUTTON_DISMISS BUTTON0

#define DIALOG_RESULT_NONE 0xff

typedef uint8_t dialog_result_t;

typedef enum {
    DIALOG_SELECTION_POS = 0xff,
    DIALOG_SELECTION_NEG = 0xfe,
    DIALOG_SELECTION_NONE = 0xfd,
    // values from 0 are used for selected items.
} dialog_selection_t;

typedef enum {
    DIALOG_ITEM_BUTTON,
#ifndef DIALOG_NO_CHOICE
    DIALOG_ITEM_CHOICE,
#endif
#ifndef DIALOG_NO_NUMBER
    DIALOG_ITEM_NUMBER,
#endif
#ifndef DIALOG_NO_TEXT
    DIALOG_ITEM_TEXT,
#endif
} dialog_item_type_t;

typedef struct {
    dialog_result_t result;
} dialog_button_t;

#ifndef DIALOG_NO_CHOICE
typedef struct {
    uint8_t choices_count;
    uint8_t selection;
    const char* const * choices;
} dialog_choice_t;
#endif

#ifndef DIALOG_NO_NUMBER
typedef struct {
    uint8_t value;
    uint8_t min;
    uint8_t max;
    uint8_t mul;
} dialog_number_t;
#endif

#ifndef DIALOG_NO_TEXT
typedef struct {
    uint8_t length;
    uint8_t max_length;
    char* text;
} dialog_text_t;
#endif

typedef struct {
    dialog_item_type_t type;
    const char* name;
    union {
        dialog_button_t button;
#ifndef DIALOG_NO_CHOICE
        dialog_choice_t choice;
#endif
#ifndef DIALOG_NO_NUMBER
        dialog_number_t number;
#endif
#ifndef DIALOG_NO_TEXT
        dialog_text_t text;
#endif
    };
} dialog_item_t;

enum {
    // if set, dialog can be dismissed and returns dismiss result.
    DIALOG_FLAG_DISMISSABLE = 1 << 0,
};

typedef struct {
    uint8_t flags;

    disp_x_t x;
    disp_y_t y;
    uint8_t width;
    uint8_t height;

    graphics_font_t title_font;
    graphics_font_t action_font;
#ifndef DIALOG_NO_ITEM_TEXT
    graphics_font_t item_font;
#endif

    const char* title;
    const char* pos_btn;
    const char* neg_btn;

    dialog_result_t pos_result;
    dialog_result_t neg_result;
    dialog_result_t dismiss_result;

    uint8_t cursor_pos;

    dialog_selection_t selection;
    uint8_t item_count;
    dialog_item_t items[DIALOG_MAX_ITEMS];
} dialog_t;

/**
 * The dialog instance.
 * This structure can be safely modified, however there are dedicated functions that should
 * be used for adding options and setting the font.
 */
extern dialog_t dialog;

/**
 * Initialize a dialog with the dialog coordinates and size.
 * The dialog initially has no title and contains no items and no action buttons.
 * The dialog is also not dismissable by default.
 *
 * The dialog can be customized afterwards. Note that there cannot be a negative button without
 * a positive button. There also must be at least one selectable element, the selection can never
 * be `DIALOG_SELECTION_NONE`.
 *
 * If action buttons or the dismiss action are used, their result code must be set.
 * By default, the result codes are `DIALOG_RESULT_NONE`.
 * If dismiss action is not set, it will default to the same action as the negative action button.
 */
void dialog_init(disp_x_t x, disp_y_t y, uint8_t width, uint8_t height);

#define dialog_init_centered(w, h) (dialog_init((DISPLAY_WIDTH - (w)) / 2, \
                                    (DISPLAY_HEIGHT - (h)) / 2, (w), (h)))
#define dialog_init_hcentered(y, w, h) (dialog_init((DISPLAY_WIDTH - (w)) / 2, y, (w), (h)))
#define dialog_init_vcentered(x, w, h) (dialog_init(x, (DISPLAY_HEIGHT - (h)) / 2, (w), (h)))

/**
 * Set the fonts used by the dialog.
 */
void dialog_set_font(graphics_font_t title_font, graphics_font_t action_font
#ifndef DIALOG_NO_ITEM_TEXT
                     , graphics_font_t item_font
#endif
                     );

/**
 * Add a button item to the dialog. The item is added following the last one.
 * The button is centered and its text is drawn with the large font.
 */
void dialog_add_item_button(const char* name, dialog_result_t result);

#ifndef DIALOG_NO_CHOICE
/**
 * Add an item with a list of choices to the dialog. The item is added following the last one.
 * The choices list is drawn with action font while the item's name is drawn with item font.
 */
void dialog_add_item_choice(const char* name, uint8_t selection,
                            uint8_t choices_count, const char* const * choices);
#endif

#ifndef DIALOG_NO_NUMBER
/**
 * Add an item with a number picker to the dialog. The item is added following the last one.
 * The current value is drawn with action font while the item's name is drawn with item font.
 */
void dialog_add_item_number(const char* name, uint8_t min, uint8_t max, uint8_t mul, uint8_t value);
#endif

#ifndef DIALOG_NO_TEXT
/**
 * Add an item with a text field to the dialog. The item is added following the last one.
 * The item's name is drawn with item font font and the text field is drawn with action font.
 */
void dialog_add_item_text(const char* name, uint8_t max_length, char text[]);
#endif

/**
 * Handle buttons input to navigate the dialog, given the last and current input state.
 * If a button item or action button is clicked, its result code is returned.
 * Otherwise, `DIALOG_RESULT
 */
dialog_result_t dialog_handle_input(uint8_t last_state, uint8_t curr_state);

/**
 * Draw the dialog to the display.
 * This must be called in the display loop to show the dialog.
 */
void dialog_draw(void);

#endif //CORE_DIALOG_H
