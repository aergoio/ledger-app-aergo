
#define MAX_CHARS_PER_LINE 13  // some strings do not appear entirely on the screen if bigger than this


#define PAGE_FIRST 1
#define PAGE_NEXT  2
#define PAGE_PREV  3
#define PAGE_LAST  4


struct items {
  char *title;
  char *text;
  unsigned int size;
  bool in_hex;
  bool is_multicall;
};

static struct items screens[8];
static int num_screens;

static int current_screen;

static int current_page;
static int page_to_display;

void (*display_page_callback)(bool);



static char parsed_text[30];     // remaining part

static unsigned int  parsed_size;
static unsigned int  last_utf8_char;
static unsigned int  last_char;
static bool is_in_command;
static bool is_in_string;
static bool in_internal_str;
static bool is_in_object;
static bool is_in_array;
static int  obj_level;

static unsigned char*input_text;
static unsigned int  input_size;
static unsigned int  input_pos;  // current position in the text to parse



static bool parse_next_page();
static bool parse_page_text();
static bool parse_multicall_page();

static void display_page();
static void reset_display_state();
static void reset_page();

static bool on_last_screen();
static bool on_last_page();



/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char UTF8Trans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};

#define READ_UTF8(zIn, zEnd, c)                            \
  c = *(zIn++);                                            \
  is_utf8 = (c >= 0xc0);                                   \
  if( is_utf8 ){                                           \
    c = UTF8Trans1[c-0xc0];                                \
    while( zIn!=zEnd && (*zIn & 0xc0)==0x80 ){             \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){ c = 0xFFFD; }         \
  }

#define READ_REMAINING_UTF8(zIn, zEnd, c)                  \
  while( zIn!=zEnd && (*zIn & 0xc0)==0x80 ){               \
    c = (c<<6) + (0x3f & *(zIn++));                        \
  }                                                        \
  if( c<0x80                                               \
      || (c&0xFFFFF800)==0xD800                            \
      || (c&0xFFFFFFFE)==0xFFFE ){ c = 0xFFFD; }           \

/*
** Array for converting from half-bytes (nybbles) into ASCII hex
** digits.
*/
static const char hexdigits[] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};


////////////////////////////////////////////////////////////////////////////////
// SCREENS | PAGES
////////////////////////////////////////////////////////////////////////////////


static void clear_screens() {
  num_screens = 0;
  memset(screens, 0, sizeof(screens));
  current_screen = 0;

  //reset_page();
  current_page = 0;

  input_size = 0;
  input_pos = 0;
  parsed_size = 0;
  last_utf8_char = 0;
  last_char = 0;
  is_in_command = false;
  is_in_string = false;
  in_internal_str = false;
  is_in_object = false;
  is_in_array = false;
  obj_level = 0;
}

static void add_screens(char *title, char *text, unsigned int len, bool parse_text) {
  int i;

  if (parse_text) {

    i = num_screens++;
    screens[i].title = title;
    screens[i].text = text;
    screens[i].size = len;

  } else {

    char *ptr = text;
    while (len > 0) {
      i = num_screens++;
      screens[i].title = title;
      screens[i].text = ptr;
      screens[i].size = len > MAX_CHARS_PER_LINE ? MAX_CHARS_PER_LINE : len;
      ptr += screens[i].size;
      len -= screens[i].size;
    }

  }

}

static bool prepare_screen(int n) {

  // using base 1
  current_screen = n;
  current_page = n - 1;  // increased on parse_next_page()

  // title
  strcpy(global_title, screens[current_screen-1].title);

  // raw text (input)
  input_text = (unsigned char*)
               screens[current_screen-1].text;
  input_size = screens[current_screen-1].size;
  input_pos  = 0;

  // parsed text (output)
  last_utf8_char = 0;
  last_char = 0;
  is_in_command = false;
  is_in_string = false;
  in_internal_str = false;
  is_in_object = false;
  is_in_array = false;
  obj_level = 0;
  parsed_size = 0;
  return parse_next_page();

}

static bool display_screen(int n) {

  bool page_is_ready = prepare_screen(n);
  if (page_is_ready) {  // if not requested next part
    display_page();
  }
  return page_is_ready;

}

static void display_page() {

  if (display_page_callback) {
    display_page_callback(true);
    reset_display_state();
  }

}

static void reset_screen() {

  current_screen = 0;

  input_size = 0;
  input_pos  = 0;

  last_utf8_char = 0;
  last_char = 0;
  is_in_command = false;
  is_in_string = false;
  in_internal_str = false;
  is_in_object = false;
  is_in_array = false;
  obj_level = 0;
  parsed_size = 0;

}

static void reset_page() {

  current_page = 0;
  page_to_display = 0;
  display_page_callback = NULL;

}

static void reset_display_state() {

  page_to_display = 0;
  display_page_callback = NULL;

}

static void display_proper_page() {

  if (!display_page_callback) {
    start_display();
    return;
  }

  if (page_to_display == 0) {
    return;
  } else if (page_to_display > 0 && page_to_display <= num_screens) {
    display_screen(page_to_display);
  } else {  // page_to_display > num_screens || page_to_display == -1 (last page)

    if (current_page < num_screens) {
      bool page_is_ready;
      if (current_screen < num_screens) {
        page_is_ready = prepare_screen(num_screens);
      } else {
        page_is_ready = parse_next_page();
      }
      if (!page_is_ready) {  // requested next part
        return;
      }
    }

    // parse the text up to the page to be displayed
    while(true){
      if (current_page == page_to_display) {
        display_page_callback(true);
        reset_display_state();
        return;
      }

      if (on_last_page()) {
        if (page_to_display == -1) {
          display_page_callback(true);
        } else {
          display_page_callback(false);
        }
        reset_display_state();
        return;
      }

      bool page_is_ready = parse_next_page();
      if (!page_is_ready) {  // requested next part
        return;
      }
    }

  }

}

static void display_new_input() {

  input_text = (unsigned char*)
               screens[current_screen-1].text;
  input_size = screens[current_screen-1].size;
  input_pos  = 0;

  display_proper_page();

}

static void get_next_data(int move_to, void(*callback)(bool)) {

  display_page_callback = callback;

  if (current_page == 0) {
    current_page = current_screen;
  }

  switch (move_to) {
  case PAGE_FIRST:
    page_to_display = 1;
    break;
  case PAGE_NEXT:
    page_to_display = current_page + 1;
    break;
  case PAGE_PREV:
    page_to_display = current_page - 1;
    break;
  case PAGE_LAST:
    page_to_display = -1;
    break;
  }

  if (move_to == PAGE_PREV && page_to_display < 1) {
    display_page_callback(false);
    reset_screen();
    reset_page();
    return;
  }

  if (page_to_display > 0 && page_to_display < num_screens) {
    if (!is_first_part) {
      request_first_part();
    } else {
      display_screen(page_to_display);
    }
    return;
  }

  if (move_to == PAGE_PREV) {
    if (!is_first_part) {
      // request the first chunk of the transaction, and other
      // chunks until it has the requested page to display
      request_first_part();
      return;
    } else {
      current_screen = 0;
      current_page = 0;
    }
  }

  // page_to_display  should NOT be overwriten on the first txn part!

  display_proper_page();

}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


static bool on_last_screen() {
  return (current_screen == num_screens);
}

static bool on_last_page() {
  // 1. on last screen
  // 2. no more input to parse
  // 3. no more output to display
  return (current_page >= num_screens &&
          input_pos >= input_size && txn_is_complete &&
          parsed_size == 0);
}

// parse text from input and output it to a new page  (update current screen)
// update pointer to unparsed text - or move memory, removing old/parsed input text
static bool parse_next_page() {
  unsigned int len;

  // if the parsed text is shorter than the max size to display
  if (parsed_size < MAX_CHARS_PER_LINE) {
    if (screens[current_screen-1].is_multicall) {
      // parse text from the input
      bool has_complete_page = parse_multicall_page();
      bool parsed_all_input = (input_pos >= input_size);
      // should we request the next txn part?
      if (!has_complete_page && parsed_all_input && !txn_is_complete) {
        request_next_part();
        return false;
      }
    } else {
      // parse text from the input
      bool parsed_all_input = parse_page_text();
      // should we request the next txn part?
      if (on_last_screen() && parsed_size < MAX_CHARS_PER_LINE && parsed_all_input && !txn_is_complete) {
        request_next_part();
        return false;
      }
    }
  }

  // copy output to the display buffer (limited to MAX_CHARS_PER_LINE)
  len = parsed_size;
  if (len > MAX_CHARS_PER_LINE) {
    len = MAX_CHARS_PER_LINE;
  }
  memcpy(global_text, parsed_text, len);
  global_text[len] = '\0';

  // if there is a remaining part to be displayed on the next page
  if (len < parsed_size) {
    memmove(parsed_text, parsed_text + len, parsed_size - len);
    parsed_size -= len;
  } else {
    parsed_size = 0;
  }

  // update the page number
  current_page++;

  return true;
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
        unsigned int c = 0;
        bool is_utf8 = false;
        bool copy_it = false;

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

        if (is_in_string) {
          if (c == '"' && last_char != '\\') {
            is_in_string = false;
            return true;  //goto loc_display_page;
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
              return true;  //goto loc_display_page;
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
              return true;  //goto loc_display_page;
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
              // copy to buffer: "-> "
              parsed_text[parsed_size++] = '-';
              parsed_text[parsed_size++] = '>';
              parsed_text[parsed_size++] = ' ';
            } else {
              is_in_array = true;
            }
          } else if (c == ']') {
            is_in_command = false;
          } else if (c == '"') {
            is_in_string = true;
          } else if (c == '{') {
            is_in_object = true;
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
