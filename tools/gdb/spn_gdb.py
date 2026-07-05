import gdb

MAX_ELEMENTS = 4096

_intern_cache = {}


def _intern_table():
    try:
        var = gdb.convenience_variable("spn_intern")
    except AttributeError:
        var = None
    if var is not None and int(var) != 0:
        return var
    try:
        sym = gdb.lookup_global_symbol("spn")
        if sym is None:
            return None
        intern = sym.value()["intern"]
        return intern if int(intern) != 0 else None
    except gdb.error:
        return None


def _intern_strings(intern):
    key = int(intern)
    index = intern.dereference()["index"]
    count = int(index["count"])
    cached = _intern_cache.get(key)
    if cached is not None and cached[0] == count:
        return cached[1]
    strings = {}
    slots = index["slots"]
    for i in range(int(index["capacity"])):
        slot = slots[i]
        data = slot["data"]
        if int(data) == 0:
            continue
        try:
            strings[int(slot["id"])] = data.string(
                length=int(slot["len"]), encoding="utf-8", errors="replace"
            )
        except gdb.error:
            pass
    _intern_cache[key] = (count, strings)
    return strings


def intern_lookup(id_val):
    intern = _intern_table()
    if intern is None:
        return None
    try:
        return _intern_strings(intern).get(int(id_val))
    except gdb.error:
        return None


def da_size(arr):
    if int(arr) == 0:
        return 0
    header_type = gdb.lookup_type("sp_da_header_t")
    base = arr.cast(gdb.lookup_type("char").pointer()) - header_type.sizeof
    return int(base.cast(header_type.pointer()).dereference()["size"])


def enum_short(val):
    return str(val).rsplit("_", 1)[-1].lower()


def fmt_str(val):
    length = int(val["len"])
    data = val["data"]
    if int(data) == 0 or length == 0:
        return ""
    return data.string(length=length, encoding="utf-8", errors="replace")


def fmt_intern_id(val):
    s = intern_lookup(val)
    if s is None:
        return str(int(val))
    return '%d "%s"' % (int(val), s)


def intern_id_str(val):
    s = intern_lookup(val)
    return s if s is not None else "#%d" % int(val)


SEMVER_MAX = 0xFFFFFFFF


def fmt_semver(val):
    return "%d.%d.%d" % (int(val["major"]), int(val["minor"]), int(val["patch"]))


_OPS = {0: "<", 1: "<=", 2: ">", 3: ">=", 4: "="}


def fmt_bound(val):
    return "%s%s" % (_OPS.get(int(val["op"]), "?"), fmt_semver(val["version"]))


def fmt_range(val):
    low = val["low"]
    high = val["high"]
    mod = enum_short(val["mod"])
    lv = low["version"]
    hv = high["version"]
    if mod == "caret":
        return "^" + fmt_semver(lv)
    if mod == "tilde":
        return "~" + fmt_semver(lv)
    if mod == "wildcard":
        if int(hv["major"]) == SEMVER_MAX:
            return "*"
        if int(hv["major"]) != int(lv["major"]):
            return "%d.*" % int(lv["major"])
        return "%d.%d.*" % (int(lv["major"]), int(lv["minor"]))
    return "%s %s" % (fmt_bound(low), fmt_bound(high))


def fmt_pkg_id(val):
    return "%s@%s" % (intern_id_str(val["qualified"]), fmt_semver(val["version"]))


def fmt_triple(val):
    parts = [enum_short(val["arch"]), enum_short(val["os"]), enum_short(val["abi"])]
    while parts and parts[-1] == "none":
        parts.pop()
    return "-".join(parts) if parts else "host"


def dep_tags(val):
    tags = []
    kind = enum_short(val["kind"])
    if kind != "package":
        tags.append(kind)
    if bool(val["private"]):
        tags.append("private")
    return " [%s]" % ", ".join(tags) if tags else ""


def fmt_requested(val):
    qualified = fmt_str(val["qualified"])
    source = enum_short(val["source"])
    if source == "index":
        spec = fmt_range(val["index"]["range"])
    elif source == "file":
        spec = "file:" + fmt_str(val["file"]["path"])
    else:
        spec = source
    return "%s %s%s" % (qualified, spec, dep_tags(val))


class InternIdPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return fmt_intern_id(self.val)
        except gdb.error as e:
            return "%d <%s>" % (int(self.val), e)


class SemverPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return fmt_semver(self.val)


class SemverRangePrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return fmt_range(self.val)
        except gdb.error as e:
            return "<spn_semver_range_t: %s>" % e


class PkgIdPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return fmt_pkg_id(self.val)
        except gdb.error as e:
            return "<spn_pkg_id_t: %s>" % e


class RequestedPkgPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return fmt_requested(self.val)
        except gdb.error as e:
            return "<spn_requested_pkg_t: %s>" % e


class ResolvedDepPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return "%s%s" % (fmt_pkg_id(self.val["id"]), dep_tags(self.val))
        except gdb.error as e:
            return "<spn_resolved_dep_t: %s>" % e


class LinkUnitIdPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            root = int(self.val["root"])
            name = "<root>" if root == 0 else intern_id_str(self.val["root"])
            return "%s (%s)" % (name, fmt_triple(self.val["triple"]))
        except gdb.error as e:
            return "<spn_link_unit_id_t: %s>" % e


class ResolvedPkgPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            v = self.val
            source = enum_short(v["source"])
            return "%s <%s> deps=%d edges=%d units=%d" % (
                fmt_pkg_id(v["id"]),
                source,
                da_size(v["deps"]),
                da_size(v["edges"]),
                da_size(v["units"]),
            )
        except gdb.error as e:
            return "<spn_resolved_pkg_t: %s>" % e

    def children(self):
        v = self.val
        try:
            source = enum_short(v["source"])
            if source == "file":
                yield "file.path", fmt_str(v["file"]["path"])
            elif source == "index":
                yield "index.release", v["index"]["release"]
            elif source == "root":
                yield "root.info", v["root"]["info"]
            for field in ("deps", "edges", "units"):
                arr = v[field]
                size = da_size(arr)
                for i in range(min(size, MAX_ELEMENTS)):
                    yield "%s[%d]" % (field, i), arr[i]
        except gdb.error:
            return


PRINTERS = {
    "sp_intern_id_t": InternIdPrinter,
    "spn_semver_t": SemverPrinter,
    "spn_semver_range_t": SemverRangePrinter,
    "spn_pkg_id_t": PkgIdPrinter,
    "spn_requested_pkg_t": RequestedPkgPrinter,
    "spn_pkg_req": RequestedPkgPrinter,
    "spn_resolved_dep_t": ResolvedDepPrinter,
    "spn_link_unit_id_t": LinkUnitIdPrinter,
    "spn_resolved_pkg_t": ResolvedPkgPrinter,
}


def _lookup(val):
    name = val.type.name
    printer = PRINTERS.get(name)
    if printer is not None:
        return printer(val)
    t = val.type.strip_typedefs()
    printer = PRINTERS.get(t.name or t.tag)
    if printer is not None and t.code == gdb.TYPE_CODE_STRUCT:
        return printer(val)
    return None


_lookup.spn_tag = "spn_gdb"
gdb.pretty_printers[:] = [
    p for p in gdb.pretty_printers if getattr(p, "spn_tag", None) != "spn_gdb"
]
gdb.pretty_printers.append(_lookup)


class SpnInternCommand(gdb.Command):
    def __init__(self):
        super(SpnInternCommand, self).__init__("spn-intern", gdb.COMMAND_DATA)

    def invoke(self, argument, from_tty):
        args = gdb.string_to_argv(argument)
        if len(args) != 1:
            print("Usage: spn-intern <id-expression>")
            return
        try:
            print(fmt_intern_id(gdb.parse_and_eval(args[0])))
        except gdb.error as e:
            print("Error: %s" % e)


SpnInternCommand()
