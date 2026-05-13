#pragma once

// Single-line touchscreen keyboard on the bottom screen.
// buf     : output buffer
// maxlen  : max chars (including null terminator)
// prompt  : label drawn above the input field
// Returns 1 on confirm (A/Start), 0 on cancel (B).
int kb_get_string(char *buf, int maxlen, const char *prompt);
