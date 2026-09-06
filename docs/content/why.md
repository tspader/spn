---
title: Why do I care?
order: 12
site: false
---

## "My CI isn't absurdly fast with zero configuration"

Look. I get it. Setting up the mess of tooling and integrations needed to cache an object file reliably is hard. It's so much easier to just sink into that warm abyss of twenty minute builds (twenty minutes is normal, right?) and tell yourself it doesn't matter.

Twenty minutes is not normal! An hour is not normal! 90% of your builds in CI should be so fast that you have to double check that it actually happened.

If this isn't the case for whatever you work on, [send me an email right god damn now](mailto:admin@spader.zone)! Building native code is an *extremely well understood* problem. The problem's not that we don't know how, it's that we know how, and, well, it's pretty fucking hard unless you spend a *lot* of time on it. You, person who are presumably making and/or selling something other than an obscure meta-tool, don't have that much spare time[^grass].

takes quite a bit of rigor and care that most folks who are actually making and selling things can't spare.

## "My company doesn't approve tools with dependencies (e.g. Python or JS)"

`spn` has *zero* dependencies. It is exactly one binary. Builds are hermetic and sandboxed by default, and it's as easy to pin the exact commits of your dependencies as it is to use packages from the index.

[^grass]: And if you did, you'd probably spend it running your hands through the soft day-warmed grass of some shaded grove, those whom you love most by your side, feeling the beams of the sun against your face and basking in the simple pleasure of being alive.
