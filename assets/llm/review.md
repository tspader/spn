Read the entire set of changes and verify the following. Use task tracking:
- Code is free of new comments
- Tests are written declaratively as defined in test.md
- No core algorithms, data structures, OS utilities, etc. were handrolled but exist in sp.h
- Structured data is used instead of strings; if a string only exists to be passed through an error channel, it should be structured data (e.g. an enum) which is converted to a string at the point a string is needed
- Code is not overly defensive; instead of e.g. checking NULL at every call site, it should be established once, as an invariant. Defensive code is not safe code; it is a sign that you have no idea what the actual state of your program is. Moreover, it masks bugs and makes debugging difficult. If such a check were to actually trigger and prevent a segfault, the program is still in an unknown state, and will misbehave at a later, more subtle time. Failing loudly is not a preference; it is a matter of correctness. Carefully read all defensive checks and ensure they are purposeful.
