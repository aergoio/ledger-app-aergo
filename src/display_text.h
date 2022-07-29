#include "common/utf8.h"

/*
** Reads characters from the source text and writes into the
** display buffer until it has enough content to display or
** the source buffer was all read.
*/
static bool parse_page_text() {  // in_hex as argument?
    unsigned char *zIn, *zEnd;

    zIn  = &input_text[input_pos];
    zEnd = &input_text[input_size];

    while (zIn < zEnd && parsed_size < MAX_CHARS_PER_LINE) {
        unsigned int c = 0;
        bool is_utf8 = false;

        if (screens[current_screen-1].in_hex) {
          c = *(zIn++);
        } else {
          if (last_utf8_char != 0) {
            c = last_utf8_char;
            last_utf8_char = 0;
            READ_REMAINING_UTF8(zIn, zEnd, c);
          } else {
            READ_UTF8(zIn, zEnd, c);
          }
        }

        input_pos = zIn - input_text;

        /* do we have a partial UTF8 char at the end? */
        if (zIn == zEnd && is_utf8 && has_partial_payload) {
          last_utf8_char = c;
          return true;
        }

        if (screens[current_screen-1].in_hex) {
            parsed_text[parsed_size++] = hexdigits[(c >> 4) & 0xf];
            parsed_text[parsed_size++] = hexdigits[c & 0xf];
        } else if (c > 0x7F) { /* non-ascii chars */
            parsed_text[parsed_size++] = '\\';
            parsed_text[parsed_size++] = 'u';
            snprintf(&parsed_text[parsed_size], sizeof(parsed_text) - parsed_size, "%X", c);
            parsed_size += strlen(&parsed_text[parsed_size]);
        } else if (c == '\n' || c == '\r') {
            parsed_text[parsed_size++] = ' ';
            parsed_text[parsed_size++] = '|';
            parsed_text[parsed_size++] = ' ';
        } else if (c == 0x08) { /* backspace should not be hidden */
            parsed_text[parsed_size++] = '?';
        } else {
            parsed_text[parsed_size++] = c;
        }
    }

    return (zIn >= zEnd);  // parsed_all_input
}

static bool parse_multicall_page() {
    unsigned char *zIn, *zEnd;

    zIn  = &input_text[input_pos];
    zEnd = &input_text[input_size];

    while (zIn < zEnd && parsed_size < MAX_CHARS_PER_LINE) {
        unsigned int c;
        bool is_utf8 = false;
        bool copy_it;

        if (last_utf8_char != 0) {
          c = last_utf8_char;
          last_utf8_char = 0;
          READ_REMAINING_UTF8(zIn, zEnd, c);
        } else {
          READ_UTF8(zIn, zEnd, c);
        }

        input_pos = zIn - input_text;

        /* do we have a partial UTF8 char at the end? */
        if (zIn == zEnd && is_utf8 && has_partial_payload) {
          last_utf8_char = c;
          return (parsed_size >= MAX_CHARS_PER_LINE);
        }

        copy_it = false;

        if (is_in_string) {
          if (c == '"' && last_char != '\\') {
            is_in_string = false;
            if (parsed_size > 0) return true;  // display the page
          } else {
            copy_it = true;
          }
        } else if (in_internal_str) {
          copy_it = true;
          if (c == '"' && last_char != '\\') {
            in_internal_str = false;
          }
        } else if (is_in_object) {
          copy_it = true;
          if (c == '}') {
            obj_level--;
            if (obj_level == 0) {
              is_in_object = false;
              parsed_text[parsed_size++] = c;
              return true;  // display the page
            }
          } else if (c == '{') {
            obj_level++;
          } else if (c == '"') {
            in_internal_str = true;
          }
        } else if (is_in_array) {
          copy_it = true;
          if (c == ']') {
            obj_level--;
            if (obj_level == 0) {
              is_in_array = false;
              parsed_text[parsed_size++] = c;
              return true;  // display the page
            }
          } else if (c == '[') {
            obj_level++;
          } else if (c == '"') {
            in_internal_str = true;
          }
        } else {
          if (c == '[') {
            if (!is_in_command) {
              is_in_command = true;
              // copy to buffer: "=> "
              parsed_text[parsed_size++] = '=';
              parsed_text[parsed_size++] = '>';
              parsed_text[parsed_size++] = ' ';
            } else {
              is_in_array = true;
              obj_level = 1;
              copy_it = true;
            }
          } else if (c == ']') {
            is_in_command = false;
          } else if (c == '"') {
            is_in_string = true;
          } else if (c == '{') {
            is_in_object = true;
            obj_level = 1;
            copy_it = true;
          }
        }

        if (copy_it) {
          if (c > 0x7F) { /* non-ascii chars */
            parsed_text[parsed_size++] = '\\';
            parsed_text[parsed_size++] = 'u';
            snprintf(&parsed_text[parsed_size], sizeof(parsed_text) - parsed_size, "%X", c);
            parsed_size += strlen(&parsed_text[parsed_size]);
          } else if (c == '\n' || c == '\r') {
            parsed_text[parsed_size++] = ' ';
            parsed_text[parsed_size++] = '|';
            parsed_text[parsed_size++] = ' ';
          } else if (c == 0x08) { /* backspace should not be hidden */
            parsed_text[parsed_size++] = '?';
          } else {
            parsed_text[parsed_size++] = c;
          }
        }

        last_char = c;
    }

    return (parsed_size >= MAX_CHARS_PER_LINE);
}
