#pragma once

// Download url into dest_dir.
// Filename is derived from the URL path; content-type is used to
// pick extension if the URL has none (falls back to .txt).
// Returns 1 on success, 0 on failure.
int dl_download(const char *url, const char *dest_dir);
