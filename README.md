## Active branches
- `main`: In this branch there is the implementation using original C code. It only supports position map sizes under `SCAN_THRESHOLD` since it only has linear scan position map implemented.
- `jazz-security-check`: CT and S-CT checks are passing using `main` code.
- `oram-position-map`: Implementation of oram position map (2-level position map)
- `oram-pm-security`: `oram-position-map` code defended against CT and S-CT. We added a CT division and modulo, and made oblivious a memory access which was causing security issues.
- `3-level-pm`: Implementation of oram position map (3-level position map)
- `3-level-pm-security`: `3-level-pm` code defended against CT and S-CT. It has implemented stash extension using `#mmap`. This causes a leak, but this is known and accepted by us.
- `4-level-pm`: Implementation of oram position map (4-level position map)

> Note: parameters in the last three branches were modified so that the oram fits in memory.
