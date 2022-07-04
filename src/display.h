
#pragma GCC diagnostic ignored "-Wformat-invalid-specifier"  // snprintf
#pragma GCC diagnostic ignored "-Wformat-extra-args"         // snprintf

void on_anterior_delimiter();
void on_posterior_delimiter();

////////////////////////////////////////////////////////////////////////////////
// START STEPS
////////////////////////////////////////////////////////////////////////////////

// Step with icon and text
UX_STEP_NOCB(step_confirm_address, pn, {&C_icon_eye, "Confirm Address"});

// Step with icon and text
UX_STEP_NOCB(step_review_transaction,
             pnn,
             {
                 &C_icon_eye,
                 "Review",
                 "Transaction",
             });

// Step with icon and text
UX_STEP_NOCB(step_review_message,
             pnn,
             {
                 &C_icon_eye,
                 "Review",
                 "Message",
             });

////////////////////////////////////////////////////////////////////////////////
// DYNAMIC STEPS
////////////////////////////////////////////////////////////////////////////////

UX_STEP_INIT(
   step_anterior_delimiter,
   NULL,
   NULL,
   {
      on_anterior_delimiter();
   }
);

UX_STEP_NOCB(
   step_generic,
   bnnn_paging,
   {
      .title = global_title,
      .text  = global_text,
   }
);

UX_STEP_INIT(
   step_posterior_delimiter,
   NULL,
   NULL,
   {
      on_posterior_delimiter();
   }
);

////////////////////////////////////////////////////////////////////////////////
// ENDING STEPS
////////////////////////////////////////////////////////////////////////////////

// Step with approve button
UX_STEP_CB(step_approve,
           pb,
           sign_transaction(),
           {
               &C_icon_validate_14,
               "Approve",
           });

// Step with reject button
UX_STEP_CB(step_reject,
           pb,
           reject_transaction(),
           {
               &C_icon_crossmark,
               "Reject",
           });

UX_STEP_CB(step_back,
           pb,
           ui_menu_main(),
           {
               &C_icon_back,
               "Back",
           });

////////////////////////////////////////////////////////////////////////////////

// The maximum number of steps
#define MAX_NUM_STEPS 8

// The array of steps. + 1 for FLOW_END_STEP
const ux_flow_step_t *ux_generic_flow[MAX_NUM_STEPS + 1];


void start_display() {
  uint8_t index = 0;

  if (cmd_type == INS_SIGN_TXN) {
    ux_generic_flow[index++] = &step_review_transaction;
  } else if (cmd_type == INS_SIGN_MSG) {
    ux_generic_flow[index++] = &step_review_message;
  } else if (cmd_type == INS_DISPLAY_ACCOUNT) {
    ux_generic_flow[index++] = &step_confirm_address;
  }

  ux_generic_flow[index++] = &step_anterior_delimiter;
  ux_generic_flow[index++] = &step_generic;
  ux_generic_flow[index++] = &step_posterior_delimiter;

  if (is_signing) {
    ux_generic_flow[index++] = &step_approve;
    ux_generic_flow[index++] = &step_reject;
  } else {
    ux_generic_flow[index++] = &step_back;
  }

  ux_generic_flow[index++] = FLOW_LOOP;
  ux_generic_flow[index++] = FLOW_END_STEP;

  // start the display
  ux_flow_init(0, ux_generic_flow, NULL);
}

////////////////////////////////////////////////////////////////////////////////

// State of the dynamic display.
// Use to keep track of whether we are displaying screens that are inside the
// array (dynamic), or outside the array (static).
enum e_state {
   STATIC_SCREEN,
   DYNAMIC_SCREEN,
};

// An instance of our new enum
enum e_state current_state;

// This is a special function for bnnn_paging to work properly in an edgecase
void bnnn_paging_edgecase() {
    G_ux.flow_stack[G_ux.stack_count - 1].prev_index = G_ux.flow_stack[G_ux.stack_count - 1].index - 2;
    G_ux.flow_stack[G_ux.stack_count - 1].index--;
    ux_flow_relayout();
}

////////////////////////////////////////////////////////////////////////////////
// CALLBACKS
////////////////////////////////////////////////////////////////////////////////

void on_first_page_cb(bool has_dynamic_data) {
  if (has_dynamic_data) {
    current_state = DYNAMIC_SCREEN;
    ux_flow_next();  // move from the anterior delimiter to the dynamic screen  ->
  } else {
    ux_flow_prev();  // move from the anterior delimiter to the static screen   <-
  }
}

void on_prev_page_cb(bool has_dynamic_data) {
  if (has_dynamic_data) {
    ux_flow_next();  // move from the anterior delimiter to the dynamic screen  ->
  } else {
    current_state = STATIC_SCREEN;
    ux_flow_prev();  // move from the anterior delimiter to the static screen   <-
  }
}

void on_last_page_cb(bool has_dynamic_data) {
  if (has_dynamic_data) {
    current_state = DYNAMIC_SCREEN;
    ux_flow_prev();  // move from the posterior delimiter to the dynamic screen  <-
  } else {
    ux_flow_next();  // move from the posterior delimiter to the static screen   ->
  }
}

void on_next_page_cb(bool has_dynamic_data) {
  if (has_dynamic_data) {
    // similar to `ux_flow_prev()` but updates layout to account for `bnnn_paging`'s weird behaviour
    bnnn_paging_edgecase();  // move from the posterior delimiter to the dynamic screen  <-
  } else {
    current_state = STATIC_SCREEN;
    ux_flow_next();  // move from the posterior delimiter to the static screen  ->
  }
}

////////////////////////////////////////////////////////////////////////////////
// DELIMITERS
////////////////////////////////////////////////////////////////////////////////

void on_anterior_delimiter() {

  if (current_state == STATIC_SCREEN) {  // clicking NEXT [>]
    get_next_data(PAGE_FIRST, on_first_page_cb);
  } else {                               // clicking PREV [<]
    get_next_data(PAGE_PREV, on_prev_page_cb);
  }

}

void on_posterior_delimiter() {

  if (current_state == STATIC_SCREEN) {  // clicking PREV [<]
    get_next_data(PAGE_LAST, on_last_page_cb);
  } else {                               // clicking NEXT [>]
    get_next_data(PAGE_NEXT, on_next_page_cb);
  }

}
