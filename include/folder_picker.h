#pragma once

// Browse directories on the FAT filesystem (SD / CF).
// out_path : buffer to receive chosen directory (e.g. "fat:/downloads")
// maxlen   : size of out_path buffer
// Returns 1 if user pressed START to select, 0 if SELECT to cancel.
int folder_pick(char *out_path, int maxlen);
