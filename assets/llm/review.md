Read the entire set of changes and verify the following. Use task tracking:
- Code is free of new comments
- Tests are written declaratively as defined in test.md
- No core algorithms, data structures, OS utilities, etc. were handrolled but exist in sp.h
- Structured data is used instead of strings; if a string only exists to be passed through an error channel, it should be structured data (e.g. an enum) which is converted to a string at the point a string is needed
