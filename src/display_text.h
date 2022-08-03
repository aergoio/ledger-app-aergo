#include "common/utf8.h"

static char args_separator[] = ",\"Args\":[";
static int  args_pos;
static bool display_char;
static bool already_in_hex;
static unsigned int last_utf8_char;
static unsigned int last_char;
static bool is_in_command;
static bool is_in_string;
static bool in_internal_str;
static bool is_in_literal;
static bool is_in_object;
static bool is_in_array;
static int  obj_level;

void get_payload_info(unsigned int *ppayload_part_offset, unsigned int *ppayload_len);

static void reset_text_parser() {

  display_char = true;
  already_in_hex = false;
  last_utf8_char = 0;
  last_char = 0;
  is_in_command = false;
  is_in_string = false;
  in_internal_str = false;
  is_in_literal = false;
  is_in_object = false;
  is_in_array = false;
  obj_level = 0;
  parsed_size = 0;

}

static bool is_json_literal(unsigned char c) {
  return (strchr("truefalsn0123456789.", c) > 0);
}

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

    if (screens[current_screen-1].is_call) {
      bool showing_function = (strcmp(global_title,"Function") == 0);
      if (display_char && showing_function && c == '"') {
        display_char = false;
        args_pos = 0;
        return (zIn >= zEnd);  // parsed_all_input
      }
      if (!display_char) {
        if (c == args_separator[args_pos]) {
          args_pos++;
          if (args_pos == 9) {  // strlen(args_separator)
            strcpy(global_title, "Parameters");
            display_char = true;
          }
        }
        continue;
      }
      if (display_char && !showing_function) {
        unsigned int payload_pos, payload_part_offset, payload_len;
        get_payload_info(&payload_part_offset, &payload_len);
        payload_pos = 8 + payload_part_offset + input_pos;
        // discard last }] bytes
        if (payload_pos == payload_len - 2) {
          input_pos += 2;
          zIn += 2;
          return (zIn >= zEnd);  // parsed_all_input
        }
      }
    }

    bool in_hex = screens[current_screen-1].in_hex;
    bool use_hex_delimiters = !in_hex;
    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
      in_hex = true;
    }
    if (use_hex_delimiters && !in_hex) {
      if (already_in_hex) {
        already_in_hex = false;
        parsed_text[parsed_size++] = '>';
      }
    }

    if (in_hex) {
      if (use_hex_delimiters && !already_in_hex) {
        already_in_hex = true;
        parsed_text[parsed_size++] = '<';
      }
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
    } else if (c == '\t') {
      parsed_text[parsed_size++] = ' ';
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
    } else if (is_in_literal) {
      if (!is_json_literal(c)) {
        is_in_literal = false;
        input_pos--;  // read this char again later
        return true;  // display the page
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
      } else if (is_json_literal(c)) {
        is_in_literal = true;
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
      //} else if (c == 0x08) { /* backspace should not be hidden */
      } else if (c < 0x20) {
        parsed_text[parsed_size++] = '?';
      } else {
        parsed_text[parsed_size++] = c;
      }
    }

    last_char = c;
  }

  return (parsed_size >= MAX_CHARS_PER_LINE);
}
