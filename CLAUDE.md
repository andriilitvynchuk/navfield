## Workflow Orchestration

### 1. Plan Node Default
- Enter plan mode for ANY non-trivial task (3+ steps or architectural decisions)
- If something goes sideways, STOP and re-plan immediately – don't keep pushing
- Use plan mode for verification steps, not just building
- Write detailed specs upfront to reduce ambiguity

### 2. Subagent Strategy
- Use subagents liberally to keep main context window clean
- Offload research, exploration, and parallel analysis to subagents
- For complex problems, throw more compute at it via subagents
- One task per subagent for focused execution

### 3. Self-Improvement Loop
- After ANY correction from the user: update `tasks/lessons.md` with the pattern
- Write rules for yourself that prevent the same mistake
- Ruthlessly iterate on these lessons until mistake rate drops
- Review lessons at session start for relevant project

### 4. Verification Before Done
- Never mark a task complete without proving it works
- Diff behavior between main and your changes when relevant
- Ask yourself: "Would a staff engineer approve this?"
- Run tests, check logs, demonstrate correctness

### 5. Demand Elegance (Balanced)
- For non-trivial changes: pause and ask "is there a more elegant way?"
- If a fix feels hacky: "Knowing everything I know now, implement the elegant solution"
- Skip this for simple, obvious fixes – don't over-engineer
- Challenge your own work before presenting it

### 6. Autonomous Bug Fixing
- When given a bug report: just fix it. Don't ask for hand-holding
- Point at logs, errors, failing tests – then resolve them
- Zero context switching required from the user
- Go fix failing CI tests without being told how

## Task Management

1. **Plan First**: Write plan to `tasks/todo.md` with checkable items
2. **Verify Plan**: Check in before starting implementation
3. **Track Progress**: Mark items complete as you go
4. **Explain Changes**: High-level summary at each step
5. **Document Results**: Add review section to `tasks/todo.md`
6. **Capture Lessons**: Update `tasks/lessons.md` after corrections

## Core Principles

- **Simplicity First**: Make every change as simple as possible. Impact minimal code.
- **No Laziness**: Find root causes. No temporary fixes. Senior developer standards.
- **Minimal Impact**: Changes should only touch what's necessary. Avoid introducing bugs.

## C++ Style

- Use `cpp_style_guide.md` for all new C++ code
- Apply Clean Code throughout: small focused functions, intention-revealing names, no surprises
- Apply Clean Architecture: separate concerns into distinct layers; keep business logic independent of I/O, logging, and CLI parsing

## Build Commands

```bash
# One-time system dependency
brew install quill

# Configure — presets encode CMAKE_PREFIX_PATH and generator automatically
cmake --preset debug     # Debug build  (or: --preset release)

# Build
cmake --build build

# Test
ctest --test-dir build
```

## depthai Pitfalls (hard-won fixes — do not regress)

### 1. Queue ordering: BEFORE `pipeline.start()`
`createOutputQueue()` **must** be called before `pipeline.start()`. The pipeline
graph is compiled and sent to device on `start()` — any queue registered after
that is never wired, its ring-buffer is never initialised, and the first
`tryGet()` dereferences a null pointer (SEGV). Symptom when no device attached:
start() blocks ~60s, then throws; the exception unwinds before quill can flush
→ "no log output at all". See PROBLEM.md §Bug A.

```cpp
// CORRECT ORDER
auto queue = out->createOutputQueue(4, false);  // before start
pipeline.start();
```

### 2. quill + depthai macOS: disable singleton check
Always set `check_backend_singleton_instance = false` in `BackendOptions` when
both quill and depthai are linked. depthai's static initializers (spdlog, dcl)
run before `main()` and corrupt the POSIX semaphore that quill's `BackendWorkerLock`
creates → `sem_trywait((sem_t*)3)` → SIGSEGV at `0x3` before the first log line.
See PROBLEM.md §Bug B.

```cpp
quill::BackendOptions opts;
opts.check_backend_singleton_instance = false;
quill::Backend::start(opts);
```

### 3. `getCvFrame()` ownership — GRAY8 wraps, BGR888p allocates
`ImgFrame::getCvFrame()` for **GRAY8** returns a cv::Mat that **wraps the
ImgFrame's internal buffer** (no copy). If the Mat outlives the ImgFrame
(e.g., moved into a shared slot for a background thread), it becomes a
dangling pointer the moment the ImgFrame shared_ptr is released. Symptom:
1–2 fps, visual corruption, or silent UB in the display thread. Fix: always
`.clone()` after `getCvFrame()` when the Mat must outlive the ImgFrame:

```cpp
cv::Mat lf = lmsg->getCvFrame().clone();  // owns its buffer
```

**BGR888p is safe without clone** — `getCvFrame()` must allocate a new Mat
to perform the planar→interleaved conversion.

### 4. NV12 `getCvFrame()` already returns BGR — do not double-convert
`requestOutput()` with `NV12` halves USB bandwidth (1.5 vs 3 bytes/pixel).
`getCvFrame()` on an NV12 ImgFrame does the YUV→BGR conversion internally
and returns an owned 3-channel (CV_8UC3) BGR mat. Calling `cv::cvtColor` on
top of it will crash: `Bad number of channels — scn = 3` (the converter
expects 1-channel NV12 input, not the already-converted BGR). Just use the
mat directly, same as BGR888p.

### 5. `BGR888i` not supported on OAK-D Lite
`requestOutput()` with `dai::ImgFrame::Type::BGR888i` compiles and links fine,
but the OAK-D Lite ISP never produces frames in that format — the queue stays
permanently empty, flooding the log with "No RGB frame (N misses)" at 1ms
intervals. Always use `BGR888p` for CAM_A output; getCvFrame() allocates an
owned cv::Mat during the planar→interleaved conversion, so the Mat is safe to
hold beyond the ImgFrame's lifetime (required for threaded capture).

### 4. Transitive deps & build system
- All depthai transitive `find_package` calls live in `cmake/depthai_bootstrap.cmake` — do not inline them in `CMakeLists.txt`.
- Always configure via `cmake --preset debug|release` — the preset sets `CMAKE_PREFIX_PATH` for depthai's vcpkg deps automatically.
