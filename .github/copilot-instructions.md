# Project constraints

- This is a Linux C11 arena-learning experiment, not a feature-rich game.
- Preserve the documented allocator contract and keep checks active in release.
- Only application startup owns malloc; shutdown releases its backing block.
- Allocate simulation state in the arena, reuse it across generations, and reset
  only when replacing a simulation. Never add allocations merely to exercise it.
- Keep ncurses visualization and controls minimal; apply YAGNI.
- Run `make check` for debug/release/sanitizer tests and compile all application
  configurations with `make debug release sanitize` after relevant changes.
- Keep README documentation aligned with behavior and explain sanitizer limits.