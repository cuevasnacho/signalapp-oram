## Active branches
- `main`: In this branch there is the implementation using original C code. This implementation uses many params therefore it is fixed. Besides that, it only supports position maps with less than $1\ll14$ blocks, this is due to the oram position map implementation.
- `jazz-security-check`: CT and S-CT checks are passing using `main` code.
  - There are some `#declassify` which should be reviewed.

## Issues
- Since recursion is not possible, we need to know the number of blocks beforehand in order to write the position map using an oram implementation.
  - I tried to write an implementation using a single ORAM depth as a position map locally, but I have not been able to make it compile yet.
