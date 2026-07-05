# Fixtures

topology | index  | ranges     | tag
---------|--------|------------|----
none     | N/A    | N/A        | `none_resolves`
linear   | found  | N/A        | `linear_resolves`
linear   | missing| N/A        | `linear_missing`
diamond  | found  | compatible | `diamond_compatible`
diamond  | found  | disjoint   | `diamond_disjoint`
diamond  | missing (renderer) | N/A | `diamond_missing_renderer`
diamond  | missing (audio)    | N/A | `diamond_missing_audio`
diamond  | missing (math)     | N/A | `diamond_missing_math`
cycle    | found  | N/A        | `cycle_direct`
cycle    | found  | N/A        | `cycle_indirect`
diamond  | found  | compatible | `system_deps`
diamond  | found  | compatible | `system_deps_dedup`

## Link units

Fixtures for per-link-unit resolution. Package deps are public by default: a
public dep's types may appear in the package's API, so it resolves in its
consumer's scope and version conflicts are errors. A dep declared private is
an implementation detail: behind a shared (dynamic) boundary its subtree
resolves in its own scope and may diverge from the consumer's picks, with the
embedded copy's symbols hidden at link time. Privacy inherits transitively
down the private edge (a private dep's public deps are still private to the
declaring package). Privacy never splits a link unit (a static lib's private
dep still unifies with its consumer) and never permits two dynamic instances
of one package in a process (loader dedupe makes that unsound regardless of
policy). Version divergence is otherwise legal across process boundaries
(build deps, test deps, tools). A dependency cycle is an error when it runs
through a single instance; a tool may link an older release of the package
that build-depends on it.

Expectations name the unit a package must be a member of: `NULL` is any unit,
`""` is the root unit, a qualified name is the unit rooted at that request.
`instances` asserts how many distinct versions of a package the resolve holds.
`event` asserts the error kind a failed resolve pushes: range conflicts are
`err_unsatisfiable_version`, instance cycles across units are
`err_unit_cycle`, and two dynamic instances in one process is
`err_dynamic_duplicate`. Note the flat resolver emits no event at all when a
root-level request conflicts with an existing pick, so several conflict cases
fail today on the event assert alone. Cases marked *pin* pass against the
flat resolver and must keep passing; the rest fail until link units land.

boundary   | ranges     | tag | status
-----------|------------|-----|-------
build dep (root)  | disjoint   | `build_dep_root_conflict` | fails today
build dep (dep)   | disjoint   | `build_dep_transitive_conflict` | fails today
test dep (root)   | disjoint   | `test_dep_root_conflict` | fails today
test dep (dep)    | disjoint   | `transitive_test_dep_pruned` | fails today
build dep         | compatible | `build_dep_compatible_unifies` | pin
build dep         | overlapping | `preference_prefers_unified` | fails today
shared lib, public dep (root) | disjoint | `shared_lib_public_conflict_fails` | fails today (event)
shared lib, public dep (dep)  | disjoint | `shared_lib_transitive_conflict_fails` | fails today (event)
shared lib, private dep       | disjoint | `shared_lib_private_diverges` | fails today
shared lib, private transitive | disjoint | `shared_lib_private_transitive_diverges` | fails today
static lib        | disjoint   | `static_lib_conflict_fails` | fails today (event)
static lib, private dep | disjoint | `static_lib_private_conflict_fails` | fails today (event)
shared lib (two consumers) | compatible | `shared_lib_consumers_unify` | pin
shared lib (two consumers) | disjoint | `shared_lib_consumer_disjoint_fails` | pin
shared lib, private dynamic dup | disjoint | `shared_lib_private_dynamic_dup_fails` | fails today
build dep cycle   | N/A        | `build_dep_cycle_fails` | fails today (event)
build dep cycle   | bootstrap  | `build_dep_bootstrap` | fails today
build dep missing | N/A        | `build_dep_missing_still_fails` | pin

- `none_resolves`: math as root, no deps, resolves with empty result
- `linear_resolves`: audio as root depends on math `^1.0.0`, index has 1.0.0 1.1.0 2.0.0, picks 1.1.0
- `linear_missing`: audio as root depends on math `^1.0.0`, math missing from index, unknown package error
- `diamond_compatible`: game depends on renderer and audio, both depend on math with overlapping ranges, resolves to newest in intersection
- `diamond_disjoint`: game depends on renderer and audio, renderer wants math `^1.0.0`, audio wants math `^2.0.0`, unsatisfiable error
- `diamond_missing_renderer`: game depends on renderer and audio, renderer missing from index, unknown package error
- `diamond_missing_audio`: game depends on renderer and audio, audio missing from index, unknown package error
- `diamond_missing_math`: game depends on renderer and audio, both depend on math, math missing from index, unknown package error
- `cycle_direct`: audio depends on math, math depends on audio, circular dependency error
- `cycle_indirect`: game depends on audio, audio depends on math, math depends on renderer, renderer depends on audio, circular dependency error
- `system_deps`: audio has system dep `alsa`, renderer has system dep `x11`, both collected
- `system_deps_dedup`: audio and renderer both have system dep `alsa`, collected once
- `build_dep_root_conflict`: root wants foo `^2.0.0` linked and foo `^1.0.0` at build time, both resolve into separate units
- `build_dep_transitive_conflict`: renderer build-depends on foo `^1.0.0`, root links foo `^2.0.0`, the build dep roots its own unit
- `test_dep_root_conflict`: root links foo `^2.0.0` and tests against foo `^1.0.0`, test unit resolves separately
- `transitive_test_dep_pruned`: renderer's test dep on foo never builds in a consumer graph, so it is pruned instead of conflicting
- `build_dep_compatible_unifies`: root links foo `^1.0.0`, tool build-deps foo `^1.0.0`, both units share one foo instance
- `preference_prefers_unified`: build unit's range admits foo 2.0.0 but root picked 1.9.0, solver prefers the already-picked version
- `shared_lib_public_conflict_fails`: shared gfx's foo dep is public (the default), so it resolves in the root's scope and conflicts with the root's foo `^2.0.0`
- `shared_lib_transitive_conflict_fails`: same conflict discovered through app -> gfx rather than on a root dep
- `shared_lib_private_diverges`: gfx declares foo private, so gfx.so carries its own foo 1.0.0 while the root links foo 2.0.0
- `shared_lib_private_transitive_diverges`: bar is private to gfx and bar's public dep baz inherits that privacy, diverging from the root's baz
- `static_lib_conflict_fails`: static twin of `shared_lib_public_conflict_fails`; identical outcome
- `static_lib_private_conflict_fails`: privacy never splits a link unit; static gfx's private foo still conflicts with the root's
- `shared_lib_consumers_unify`: audio and video both consume shared gfx with overlapping ranges, one gfx instance from the intersection
- `shared_lib_consumer_disjoint_fails`: audio and video want disjoint gfx versions; two dynamic instances in one process, error regardless of privacy
- `shared_lib_private_dynamic_dup_fails`: audio.so and video.so privately want disjoint gfx, but gfx only builds shared; both copies would load, `err_dynamic_duplicate`
- `build_dep_cycle_fails`: audio build-deps tool, tool links the same audio instance; unbuildable, `err_unit_cycle`
- `build_dep_bootstrap`: audio 2.0.0 build-deps tool, tool links audio `^1.0.0`; two instances, no cycle, builds
- `build_dep_missing_still_fails`: a build dep absent from the index is still an unknown package error
