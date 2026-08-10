Read the entire set of changes and verify the following. Use task tracking:
- Code is free of new comments
- Tests are written declaratively as defined in test.md
- No core algorithms, data structures, OS utilities, etc. were handrolled but exist in sp.h
- Both the CMake build and the self hosted build work
- Headers only contain functions which are part of a module's public API; they should never include internal functions
- Internal functions are marked `static` and do not use a prefix
- Structured data is used instead of strings; if a string only exists to be passed through an error channel, it should be structured data (e.g. an enum) which is converted to a string at the point a string is needed

# overly defensive code
Be suspicious of defensive code, and verify whether checks are useful rather than obscuring and/or duplicating real invariants.

For example, the author writing `NULL` checks at the top of a function may intend to assert an invariant: This parameter is nonnull. But what they are communicating is that they have no idea what the state of their program is; they don't know what specifically calls the function, and they are unable to trace the various transformations of data that precede it.

It's very often better in this case to establish non-null as an invariant, once, as early as possible. This is exactly the same concept as pushing `if`s up, and very important.

Defensive code is not "safe". It masks bugs, it makes debugging harder, and if it actually catches something it's usually just allowing your program to silently continue in an unknown, soft invalid state until it misbehaves at a later, more subtle time. Failing loudly is not a preference; it is a matter of correctness. Carefully read all defensive checks and ensure they are purposeful.

# no boolean parameters
Boolean parameters are almost always a symptom of confusing conditional logic, and can almost always be solved by [pushing `if`s up and `for`s down](push-ifs-up-and-fors-down.md). In short, control flow should be centralized and pushed to higher level code which calls pure leaf functions.

Don't evaluate this mechanically; the fact that the type literally says `bool` is meaningless; this can sneak through as e.g. an enum, or a NULL check. Be wary of conditional logic in *any* function; obviously, every program will branch. There's nothing wrong with branching, but every branch must be evaluated for this disease.

