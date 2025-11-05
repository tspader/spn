# spn remove
first, check the project's deps. if it's not there, SP_FATAL(). load the resolve graph from the lock file; if the package isn't there, you're good to go. just remove it from `app.package.deps`, update `spn.toml`.

if it IS in the lock file, you just need to take care to remove from the lock file anything that was transitively included by that dep
