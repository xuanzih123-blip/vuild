"""il2cpp.py - parser for Unity IL2CPP global-metadata.dat."""
import struct

HEADER_FIELDS = [
    "stringLiteral", "stringLiteralData", "string", "events", "properties",
    "methods", "parameterDefaultValues", "fieldDefaultValues",
    "fieldAndParameterDefaultValueData", "fieldMarshaledSizes",
    "parameters", "fields", "genericParameters",
    "genericParameterConstraints", "genericContainers", "nestedTypes",
    "interfaces", "vtableMethods", "interfaceOffsets", "typeDefinitions",
    "images", "assemblies", "fieldRefs", "referencedAssemblies",
    "attributesInfo", "attributeTypes", "unresolvedVirtualCallParameterTypes",
    "unresolvedVirtualCallParameterRanges", "windowsRuntimeTypeNames",
    "windowsRuntimeStrings", "exportedTypeDefinitions", "metadataUsageLists",
    "metadataUsagePairs", "fieldRefs2", "attributeData",
]


class Meta:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        self.sanity, self.version = struct.unpack_from("<II", self.d, 0)
        if self.sanity != 0xFAB11BAF:
            raise SystemExit("not an IL2CPP metadata file (sanity %#x)" % self.sanity)
        self.tables = {}
        self._read_header()

    def _read_header(self):
        d = self.d
        v = self.version
        off = 8
        # the header grew over the versions; read as many pairs as the file has
        # (pairs are (offset, size) 32 bit each)
        pairs = []
        while off + 8 <= len(d):
            o, s = struct.unpack_from("<ii", d, off)
            if o == 0 and s == 0 and len(pairs) >= 20:
                break
            pairs.append((o, s))
            off += 8
            if len(pairs) > 70:
                break
        self.pairs = pairs
        self._pick(pairs)

    def _pick(self, pairs):
        """Identify the tables by their known order for the detected version."""
        names = list(HEADER_FIELDS)
        # v27+ inserted stringLiteralData right after stringLiteral
        if self.version >= 27:
            names = ["stringLiteral", "stringLiteralData", "string", "events",
                     "properties", "methods", "parameterDefaultValues",
                     "fieldDefaultValues", "fieldAndParameterDefaultValueData",
                     "fieldMarshaledSizes", "parameters", "fields",
                     "genericParameters", "genericParameterConstraints",
                     "genericContainers", "nestedTypes", "interfaces",
                     "vtableMethods", "interfaceOffsets", "typeDefinitions",
                     "images", "assemblies", "fieldRefs", "referencedAssemblies",
                     "attributesInfo", "attributeTypes",
                     "unresolvedVirtualCallParameterTypes",
                     "unresolvedVirtualCallParameterRanges",
                     "windowsRuntimeTypeNames", "windowsRuntimeStrings",
                     "exportedTypeDefinitions"]
        for name, (o, s) in zip(names, pairs):
            self.tables[name] = (o, s)

    def table(self, name):
        return self.tables.get(name, (0, 0))

    # ---------------------------------------------------------------- strings
    def string(self, index):
        """Index into the string table (null terminated utf8)."""
        o, s = self.table("string")
        if not s:
            return ""
        p = o + index
        if p >= len(self.d):
            return ""
        e = self.d.find(b"\x00", p)
        return self.d[p:e].decode("utf8", "replace")

    def literal(self, index):
        """String literal (utf16 or utf8 depending on version)."""
        o, s = self.table("stringLiteral")
        if not s:
            return ""
        p = o + index * 8
        length, data_index = struct.unpack_from("<II", self.d, p)
        do, ds = self.table("stringLiteralData")
        if self.version >= 27:
            do, ds = self.table("stringLiteralData")
        q = do + data_index
        raw = self.d[q:q + length]
        if self.version >= 27:
            return raw.decode("utf8", "replace")
        return raw.decode("utf-16-le", "replace")

    # --------------------------------------------------------- type defs
    def type_def_size(self):
        v = self.version
        if v >= 31:
            return 0x58
        if v >= 27:
            return 0x64
        if v >= 24:
            return 0x5C
        if v >= 23:
            return 0x58
        if v >= 22:
            return 0x54
        if v >= 21:
            return 0x50
        return 0x48

    def types(self):
        o, s = self.table("typeDefinitions")
        size = self.type_def_size()
        n = s // size
        out = []
        for i in range(n):
            p = o + i * size
            name_i, ns_i = struct.unpack_from("<2I", self.d, p)
            if self.version >= 31:
                fstart, mstart = struct.unpack_from("<2i", self.d, p + 0x20)
                counts = struct.unpack_from("<8H", self.d, p + 0x40)
            elif self.version >= 24:
                fstart, mstart = struct.unpack_from("<2i", self.d, p + 0x24)
                counts = struct.unpack_from("<8H", self.d, p + 0x44)
            else:
                fstart, mstart = struct.unpack_from("<2i", self.d, p + 0x1C)
                counts = struct.unpack_from("<8H", self.d, p + 0x3C)
            out.append({
                "idx": i,
                "name": self.string(name_i),
                "ns": self.string(ns_i),
                "field_start": fstart,
                "method_start": mstart,
                "field_count": counts[2],
                "method_count": counts[0],
                "property_count": counts[1],
                "event_count": counts[3],
                "nested_count": counts[4],
                "vtable_count": counts[5],
            })
        return out

    def method_size(self):
        if self.version >= 31:
            return 0x28
        return 0x1C if self.version >= 24 else 0x18

    def methods(self):
        o, s = self.table("methods")
        size = self.method_size()
        n = s // size
        out = []
        for i in range(n):
            p = o + i * size
            name_i = struct.unpack_from("<I", self.d, p)[0]
            if self.version >= 31:
                token = struct.unpack_from("<I", self.d, p + 0x1C)[0]
                flags = struct.unpack_from("<H", self.d, p + 0x20)[0]
                slot = struct.unpack_from("<H", self.d, p + 0x24)[0]
                pcount = struct.unpack_from("<H", self.d, p + 0x26)[0]
                decl = -1
            elif self.version >= 24:
                decl, pstart = struct.unpack_from("<2H", self.d, p + 4)
                token = struct.unpack_from("<i", self.d, p + 8)[0]
                flags, iflags, slot, pcount = struct.unpack_from("<4H", self.d, p + 12)
            else:
                decl = struct.unpack_from("<H", self.d, p + 4)[0]
                token = struct.unpack_from("<i", self.d, p + 8)[0]
                flags = struct.unpack_from("<H", self.d, p + 12)[0]
                slot = struct.unpack_from("<H", self.d, p + 14)[0]
                pcount = struct.unpack_from("<H", self.d, p + 16)[0]
            out.append({
                "idx": i,
                "name": self.string(name_i),
                "declaring_type": decl,
                "token": token,
                "flags": flags,
                "slot": slot,
                "param_count": pcount,
            })
        return out

    def fields(self):
        o, s = self.table("fields")
        size = 0x0C if self.version >= 24 else 0x08
        n = s // size
        out = []
        for i in range(n):
            p = o + i * size
            name_i, type_i = struct.unpack_from("<2I", self.d, p)
            if self.version >= 24:
                token = struct.unpack_from("<i", self.d, p + 8)[0]
            else:
                token = 0
            out.append({"idx": i, "name": self.string(name_i),
                        "type_index": type_i, "token": token})
        return out

    def images(self):
        o, s = self.table("images")
        out = []
        for i in range(s // 40):
            p = o + i * 40
            name_i, asm_i = struct.unpack_from("<2I", self.d, p)
            out.append((self.string(name_i), self.string(asm_i)))
        return out

    def assemblies(self):
        o, s = self.table("assemblies")
        out = []
        for i in range(s // 68):
            p = o + i * 68
            name_i, culture_i = struct.unpack_from("<2I", self.d, p)
            out.append(self.string(name_i))
        return out


if __name__ == "__main__":
    import sys
    m = Meta(sys.argv[1] if len(sys.argv) > 1
             else r"C:\Users\USER\Desktop\tizngu\global-metadata.dat")
    print("sanity %#x version %d  file %d bytes" % (m.sanity, m.version, len(m.d)))
    for k, (o, s) in m.tables.items():
        if s:
            print("   %-36s off %#010x size %#x" % (k, o, s))
    t = m.types()
    print("types: %d" % len(t))
    for x in t[:5]:
        print("   ", x)
    mm = m.methods()
    print("methods: %d  first: %s" % (len(mm), mm[:2]))
    ff = m.fields()
    print("fields: %d  first: %s" % (len(ff), ff[:2]))
    print("images:", m.images()[:5])
    print("assemblies:", m.assemblies()[:8])
