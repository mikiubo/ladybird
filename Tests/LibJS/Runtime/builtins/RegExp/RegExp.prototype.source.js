test("basic functionality", () => {
    expect(RegExp.prototype.source).toBe("(?:)");
    expect(RegExp().source).toBe("(?:)");
    expect(/test/.source).toBe("test");
    expect(/\n/.source).toBe("\\n");
    expect(/foo\/bar/.source).toBe("foo\\/bar");
});

test("escaped characters", () => {
    const tests = [
        { regex: /\\n/, source: "\\\\n" },
        { regex: /\\\n/, source: "\\\\\\n" },
        { regex: /\\\\n/, source: "\\\\\\\\n" },
        { regex: /\\r/, source: "\\\\r" },
        { regex: /\\\r/, source: "\\\\\\r" },
        { regex: /\\\\r/, source: "\\\\\\\\r" },
        { regex: /\\u2028/, source: "\\\\u2028" },
        { regex: /\\\u2028/, source: "\\\\\\u2028" },
        { regex: /\\\\u2028/, source: "\\\\\\\\u2028" },
        { regex: /\\u2029/, source: "\\\\u2029" },
        { regex: /\\\u2029/, source: "\\\\\\u2029" },
        { regex: /\\\\u2029/, source: "\\\\\\\\u2029" },
        { regex: /\//, source: "\\/" },
        { regex: /\\\//, source: "\\\\\\/" },
        { regex: /[/]/, source: "[/]" },
        { regex: /[\/]/, source: "[\\/]" },
        { regex: /[\\/]/, source: "[\\\\/]" },
        { regex: /[\\\/]/, source: "[\\\\\\/]" },
        { regex: /\[\/\]/, source: "\\[\\/\\]" },
        { regex: /\[\/\]/, source: "\\[\\/\\]" },
        { regex: /\[\\\/\]/, source: "\\[\\\\\\/\\]" },
    ];

    for (const test of tests) {
        expect(test.regex.source).toBe(test.source);
    }
});

test("lone surrogates escape according to UnicodeMode flag (u/v)", () => {
    const loneHigh = String.fromCharCode(0xd83d);
    const loneLow = String.fromCharCode(0xdc00);

    // In ~UnicodeMode raw lone surrogate code units remain valid SourceCharacters,
    // so they round-trip as-is.
    expect(new RegExp(loneHigh).source).toBe(loneHigh);
    expect(new RegExp(loneLow).source).toBe(loneLow);

    // In Pattern[+UnicodeMode] (flag "u") lone surrogates must be escaped as RegExpUnicodeEscapeSequence.
    expect(new RegExp(loneHigh, "u").source).toBe("\\uD83D");
    expect(new RegExp(loneLow, "u").source).toBe("\\uDC00");

    // Flag "v" implies +UnicodeMode as well.
    expect(new RegExp(loneHigh, "v").source).toBe("\\uD83D");
    expect(new RegExp(loneLow, "v").source).toBe("\\uDC00");
});

test("astral characters (valid surrogate pairs) are preserved in u/v modes", () => {
    // U+1F600 GRINNING FACE - encoded as the surrogate pair D83D DE00.
    const grinning = "\u{1F600}";

    expect(new RegExp(grinning, "u").source).toBe(grinning);
    expect(new RegExp(grinning, "v").source).toBe(grinning);
    expect(new RegExp(grinning).source).toBe(grinning);
});

test("source is re-parseable as a RegExp in the same mode", () => {
    const cases = [
        { pattern: "abc", flags: "" },
        { pattern: "abc", flags: "u" },
        { pattern: "abc", flags: "v" },
        { pattern: "a/b", flags: "" },
        { pattern: "a/b", flags: "u" },
        { pattern: "a\nb", flags: "" },
        { pattern: "a\nb", flags: "u" },
        { pattern: "a\u2028b", flags: "" },
        { pattern: "a\u2029b", flags: "u" },
        { pattern: "\u{1F600}", flags: "u" },
        { pattern: String.fromCharCode(0xd83d), flags: "u" },
        { pattern: String.fromCharCode(0xdc00), flags: "v" },
    ];

    for (const { pattern, flags } of cases) {
        const original = new RegExp(pattern, flags);
        // Re-parsing original.source with the same flags must succeed and produce
        // an equivalent RegExp whose source matches the first round.
        const roundtrip = new RegExp(original.source, original.flags);
        expect(roundtrip.source).toBe(original.source);
        expect(roundtrip.flags).toBe(original.flags);
    }
});
