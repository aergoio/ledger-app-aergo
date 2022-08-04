
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
  bool is_call;
  bool is_multicall;
};

static struct items screens[10];
static int num_screens;

static int current_screen;

static int current_page;
static int page_to_display;
static int max_pages;

void (*display_page_callback)(bool);


static char parsed_text[30];     // remaining part
static unsigned int  parsed_size;

static unsigned char*input_text;
static unsigned int  input_size;
static unsigned int  input_pos;  // current position in the text to parse


// FUNCTIONS DECLARATIONS

static bool parse_next_page();
static bool parse_page_text();
static bool parse_multicall_page();

static void display_page();
static void reset_display_state();
static void reset_page();

static void reset_screen();

static bool on_last_screen();
static bool on_last_page();

static void reset_text_parser();

////////////////////////////////////////////////////////////////////////////////
// SCREENS | PAGES
////////////////////////////////////////////////////////////////////////////////


static void clear_screens() {
  num_screens = 0;
  memset(screens, 0, sizeof(screens));
  reset_screen();
  //reset_page();
  current_page = 0;
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
  strlcpy(global_title, screens[current_screen-1].title, sizeof(global_title));

  // raw text (input)
  input_text = (unsigned char*)
               screens[current_screen-1].text;
  input_size = screens[current_screen-1].size;
  input_pos  = 0;

  // parsed text (output)
  reset_text_parser();
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

  reset_text_parser();

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
      if (!page_is_ready) {  // it requested the txn next part
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
      if (!page_is_ready) {  // it requested the txn next part
        return;
      }
    }

  }

}

// called when a new txn part arrives
static void display_new_input() {

  input_text = (unsigned char*)
               screens[current_screen-1].text;
  input_size = screens[current_screen-1].size;
  input_pos  = 0;

  display_proper_page();

}

// called by the navigation buttons press
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

  if (move_to == PAGE_PREV || move_to == PAGE_LAST) {
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

  // when restarting from the first page
  if (move_to == PAGE_FIRST && !is_first_part) {
    request_first_part();
    return;
  }

  // page_to_display  should NOT be overwritten on the first txn part!

  display_proper_page();

}

////////////////////////////////////////////////////////////////////////////////

static bool on_last_screen() {
  return (current_screen == num_screens);
}

static bool on_last_page() {
  // is there a maximum number of pages to display?
  // have we already displayed the maximum number of pages?
  if (max_pages > 0 && current_page - num_screens >= max_pages) {
    return txn_is_complete;
  }
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

  // have we already displayed the maximum number of pages?
  if (max_pages > 0 && current_page - num_screens >= max_pages) {
    if (!txn_is_complete) {
      request_next_part();
      return false;
    }
  }

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

  if (parsed_size == 0) {
    return true;
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

  // will we discard the next pages?
  if (max_pages > 0 && current_page - num_screens >= max_pages) {
    // add ... at the end
    if (len > 5) {
      global_text[--len] = '.';
      global_text[--len] = '.';
      global_text[--len] = '.';
    }
  }

  return true;
}
