# Google C++ Style Guide — Condensed Reference

## C++ Version
- Target: **C++20** (not C++23)
- No non-standard extensions
- Consider portability before using C++17/C++20 features

---

## Header Files

### Self-contained Headers
- Headers must be self-contained and end in `.h`
- Non-header files meant for inclusion end in `.inc` (sparingly)
- Inline functions/templates: definitions must also be in the header

### `#define` Guard
Format: `<PROJECT>_<PATH>_<FILE>_H_`
```cpp
#ifndef FOO_BAR_BAZ_H_
#define FOO_BAR_BAZ_H_
// ...
#endif  // FOO_BAR_BAZ_H_
```

### Include What You Use
- Include headers directly for every symbol you use
- Don't rely on transitive inclusions
- Avoid forward declarations when possible

### Names and Order of Includes
1. Related header
2. *(blank line)*
3. C system headers, POSIX/Windows headers (angle brackets)
4. *(blank line)*
5. C++ standard library headers
6. *(blank line)*
7. Other libraries' headers
8. *(blank line)*
9. Project headers

Within each section: **alphabetical order**.

---

## Scoping

### Namespaces
- Place code in a namespace (unique, project-based names)
- No `using` directives (`using namespace foo` forbidden)
- No inline namespaces
- No declarations in `namespace std`
- No namespace aliases in header files (except internal-only namespaces)
- Terminate multi-line namespaces with comments: `} // namespace mynamespace`
- Single-line nested namespaces preferred: `namespace my_project::component {}`

### Internal Linkage
- Give internal `.cc` definitions internal linkage via unnamed namespace or `static`
- Don't use internal linkage in `.h` files

### Nonmember, Static Member, and Global Functions
- Prefer nonmember functions in a namespace
- Don't use classes just to group static members

### Local Variables
- Declare in narrowest scope possible; initialize at declaration
- Declare loop variables within loop statements
- Beware of constructor/destructor cost in tight loops — hoist if needed

### Static and Global Variables
- Forbidden unless **trivially destructible**
- `constexpr`/`constinit` required for non-local static variables
- Avoid dynamic initialization; if unavoidable, ensure no ordering dependencies
- Avoid `std::string`, `std::map`, smart pointers as statics — use arrays, `string_view`, or function-local static pointers

### `thread_local` Variables
- Namespace/class scope: must be compile-time constant, annotate with `constinit`
- Function-scope `thread_local` is safer
- Prefer trivial types or types with no user-provided destructor

---

## Classes

### Constructors
- Avoid virtual method calls
- Avoid initialization that can fail; use factory functions or `Init()` instead

### Implicit Conversions
- Mark single-argument constructors and conversion operators `explicit`
- Exception: copy/move constructors, `initializer_list` constructors

### Copyable and Movable Types
- Explicitly declare or `= delete`: copy constructor, copy assignment, move constructor, move assignment
- Be explicit; don't rely on compiler-generated defaults in non-trivial types

### Structs vs. Classes
- **Structs**: passive data carriers, all fields public, no invariants
- **Classes**: have invariants or non-trivial logic

### Structs vs. Pairs and Tuples
- Prefer struct with named fields over `pair`/`tuple`

### Inheritance
- Composition often better than inheritance
- All inheritance must be `public`
- Avoid multiple implementation inheritance
- Annotate overrides with `override` or `final` (not `virtual`)

### Operator Overloading
- Only when meaning is obvious and consistent with built-in semantics
- Define in same header/namespace as the type
- Prefer non-member binary operators
- **Don't overload**: `&&`, `||`, `,` (comma), unary `&`, `operator""`
- For equality: define `operator==`. Define `operator<=>` only if natural ordering exists

### Access Control
- Data members: `private` (except constants in structs)

### Declaration Order
1. Types, type aliases, enums, nested structs, friend types
2. Non-static data members (structs only)
3. Static constants
4. Factory functions
5. Constructors and assignment operators
6. Destructor
7. All other functions
8. All other data members

---

## Functions

### Inputs and Outputs
- Prefer return values over output parameters
- Non-optional inputs: values or `const` references
- Non-optional outputs/input-outputs: references (non-const pointer also OK)
- Optional inputs: `std::optional` or `const` pointer
- Input parameters before output parameters

### Write Short Functions
- Aim for ~40 lines or less

### Function Overloading
- Only when meaning is unambiguous at call site
- No semantic differences between overloads

### Default Arguments
- Only on non-virtual functions
- Default must always evaluate to same value
- Banned on virtual functions

### Trailing Return Type Syntax
- Use only when required (lambdas) or significantly more readable

---

## Ownership and Smart Pointers
- Prefer single, fixed owners for dynamic objects
- `std::unique_ptr` for exclusive ownership
- `std::shared_ptr` only when necessary; prefer immutable objects
- Never use `std::auto_ptr`

---

## Other C++ Features

### Rvalue References
- Use only for: move constructors/assignments, `&&`-qualified methods, `std::forward` with forwarding references, paired overloads
- Avoid in general function parameters

### Friends
- Allow within reason; define in same file

### Exceptions
- **Don't use** C++ exceptions

### `noexcept`
- Use when it accurately reflects semantics and provides meaningful benefit
- Especially useful on move constructors

### RTTI
- Avoid in general code; allowed in unit tests
- Prefer virtual methods, Visitor pattern, or guaranteed type
- No hand-implemented RTTI workarounds

### Casting
- Use C++ casts: `static_cast`, `const_cast`, `reinterpret_cast`
- No C-style casts except to `void`
- `std::bit_cast` for type punning

### Streams
- OK for local, human-readable, developer-targeted I/O
- Avoid for external/untrusted data
- Don't use stateful stream API

### Pre/Post Increment
- Prefer prefix form (`++i`) unless postfix semantics needed

### `const`
- Use in APIs: parameters, methods, non-local variables
- Declare methods `const` unless they alter state
- All `const` operations must be thread-safe

### `constexpr`, `constinit`, `consteval`
- `constexpr`: define true constants or ensure constant initialization
- `constinit`: ensure constant initialization for non-constant variables
- `consteval`: restrict to compile-time only use

### Integer Types
- Use `int` for small integers
- `int16_t`, `int32_t`, `int64_t` for specific sizes (`<stdint.h>`)
- `int64_t`/`uint64_t` for potentially large values
- No `unsigned` except bitfields or modular arithmetic

### Floating-Point Types
- Only `float` and `double` (IEEE-754)
- No `long double`

### Preprocessor Macros
- Avoid in headers; define right before use, `#undef` right after
- Never use to define API pieces
- Format: `ALL_CAPS` with project prefix

### `nullptr` / `NULL` / `0`
- `nullptr` for pointers
- `'\0'` for null character

### `sizeof`
- Prefer `sizeof(varname)` over `sizeof(type)`

### Type Deduction (`auto`)
- Use only if it makes code clearer or safer
- Local variables: eliminate obvious/irrelevant type info
- Return type deduction: only for small functions with narrow scope
- Structured bindings: OK, add comments for field names if helpful

### CTAD
- Only with templates having explicit deduction guides

### Designated Initializers
- OK in C++20 form; initializers must match struct field order

### Lambda Expressions
- Explicit captures when lambda escapes current scope
- Default `[&]` only when lambda lifetime is obviously shorter than captures
- Don't use captures with initializers to introduce new names

### Template Metaprogramming
- Avoid complex TMP; hide as implementation detail
- Document carefully; error messages are part of the API

### Concepts and Constraints
- Use sparingly; prefer standard library concepts
- Internal use only; don't expose in public headers
- Must be statically verifiable

### C++20 Modules
- **Don't use**

### Coroutines
- Only via approved libraries

### Boost
- Only approved subset: call_traits, compressed_pair, BGL, property_map, iterator, polygon, bimap, math distributions, multi_index, heap, flat containers, intrusive, sort, preprocessor

### Disallowed Standard Library Features
- `<ratio>`, `<cfenv>`, `<fenv.h>`, `<filesystem>`

### Switch Statements
- Always have default case (unless compiler warns on missing enum values)
- Annotate fall-through with `[[fallthrough]];`

---

## Naming

| Element | Convention | Example |
|---|---|---|
| Files | `snake_case.cc` / `.h` | `http_server.cc` |
| Types / Classes | PascalCase | `MyClass` |
| Concepts | PascalCase | `MyConstraint` |
| Variables | `snake_case` | `my_var` |
| Class data members | `snake_case_` (trailing `_`) | `member_` |
| Struct data members | `snake_case` (no trailing `_`) | `field` |
| Constants (static storage) | `kPascalCase` | `kDaysInAWeek` |
| Functions | `snake_case()` | `add_entry()` |
| Accessors/mutators | `snake_case()` | `count()`, `set_count()` |
| Namespaces | `snake_case` | `my_project` |
| Enumerators | `kPascalCase` | `kOk` |
| Macros | `ALL_CAPS` | `MY_PROJECT_MACRO` |

- Descriptive names; abbreviations only if widely known
- No internal letter deletion (use `error_count` not `er_cnt`)

---

## Comments

### Style
- Prefer `//` for most comments

### File Comments
- Start with license boilerplate
- Describe the file's purpose if non-obvious

### Class/Struct Comments
- What it is, how to use it, thread-safety assumptions
- Small usage example snippet helpful

### Function Comments (declarations)
- Describe what it does, inputs/outputs, nullability
- Start with verb phrase: "opens the file"

### Function Comments (definitions)
- Explain *how* it works — tricks, why this approach

### Variable Comments
- Class members: explain purpose if not obvious from name/type
- Globals: explain what and why global

### Implementation Comments
- Tricky/non-obvious blocks; don't state the obvious

### TODO Comments
```cpp
// TODO: b/12345 - Description of what needs to be done.
// TODO(username): Description.
```

---

## Formatting

### Line Length
- **Maximum 80 characters**
- Exceptions: long URLs, string literals, `#include`, header guards

### Indentation
- **2 spaces** per level (no tabs)
- Wrapped parameters: **4-space** indent

### Namespaces
- **No indent** within namespaces

### Classes
- `public:`, `protected:`, `private:` indented **1 space**
- Order: public → protected → private

### Functions
- Open brace at end of last parameter line
- Separate declaration and definition aligned at parenthesis or 4-space indent

### Lambdas
- Format like normal functions
- Short lambdas inline as arguments

### Conditionals / Loops
- Space after keyword: `if (`, `while (`, `for (`
- Always use braces for controlled statements
- Exception: entire statement fits on 1-2 lines
- Empty loops: `{}` or `continue`, never `;`

### Switch
- Case label: +2-space indent from `switch`
- Case body: +4-space indent from `switch`

### Pointers / References
- No space before `*`/`&` in declarations
- No multiple declarations with pointers: `int* a, b;` forbidden

### Boolean Expressions
- Use `&&`/`||` (not `and`/`or`)
- Break before operator at 80 chars

### Horizontal Whitespace
- Two spaces before inline comments
- No trailing whitespace
- Spaces around `=`, binary operators
- No spaces inside `()`, `[]`, `<>` for templates/casts

### Vertical Whitespace
- Use sparingly; blank lines separate logical chunks
- No blank lines at start/end of block

### Floating-point Literals
- Always include radix point: `1.0`, `0.5f`, `1.0e10`

---

## Inclusive Language
- Avoid: master/slave, blacklist/whitelist, and similar terms
- Use gender-neutral language

---

## Exceptions to the Rules
- Existing non-conformant code: match local style for consistency
- Windows code: no Hungarian notation; use Windows types with Windows API; `#pragma once` not used (use guards); limited multiple inheritance for COM/ATL only
