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

Don't evaluate this mechanically; the fact that the type literally says `bool` is meaningless. A particularly nasty case is flagging this kind of violation, only for the author to refactor the boolean to a two-state enum. Pointless. In general, flag every boolean parameter as either fix it, justify why it must be this way (even accounting for @hacks.md), or fail the review.

# branching based on arguments is almost always wrong

Branches in general are the number one signal of poorly designed code. The worst form of this disease is when a function has a small number of callers, and it uses an argument as a proxy for which caller:

```c
void do(bool all) {
  do_thing();
  if (all) do_extra_thing();
}

void something() {
  do(true);
}

void whatever() {
  do(false);
}
```

You should task tracking to add a task checking every branch added for this. This should be flagged on site as an instant fail. If the author firmly believes this is the right shape, they can ask the human for permission to do this. This pattern shows up with strings, NULL checks, enums, all kinds of stuff. It is not limited to when the caller passes a constant.

Be wary of conditional logic in *any* function; obviously, every program will branch. There's nothing wrong with branching, but every branch must be evaluated for this disease. Do this. Really. Actually review every branch, and think about whether it conforms to the requirement that leaf functions are pure and conditional logic centralized.

