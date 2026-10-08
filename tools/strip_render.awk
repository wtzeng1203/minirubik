# Build the CLI variant of rubik_rv32i.s: every line from "#if RENDER"
# through the matching "#endif" becomes an empty line, so the output keeps
# the source's line numbers and contains no LED_MATRIX reference.
# Usage (rename only on success, so a failed run leaves no CLI file):
#   mkdir -p build && rm -f build/rubik_rv32i_cli.s &&
#   awk -f tools/strip_render.awk rubik_rv32i.s > build/rubik_rv32i_cli.s.tmp &&
#   mv build/rubik_rv32i_cli.s.tmp build/rubik_rv32i_cli.s

function fail(msg) {
    printf "strip_render.awk:%d: %s\n", NR, msg > "/dev/stderr"
    failed = 1
    exit 1
}

{
    cr = sub(/\r$/, "")
    line = $0
    gsub(/^[ \t]+|[ \t]+$/, "", line)
}

line == "#if RENDER" {
    if (inside)
        fail("nested #if RENDER (block opened at line " start ")")
    inside = 1
    start = NR
}

line ~ /^#(if|else|elif|endif)/ && line != "#if RENDER" && line != "#endif" {
    fail("unrecognized marker: " line)
}

!inside && /LED_MATRIX/ {
    fail("LED_MATRIX outside #if RENDER block")
}

{
    printf "%s%s\n", (inside ? "" : $0), (cr ? "\r" : "")
}

line == "#endif" {
    if (!inside)
        fail("#endif without #if RENDER")
    inside = 0
}

END {
    if (failed)
        exit 1
    if (inside) {
        printf "strip_render.awk: #if RENDER at line %d has no #endif\n", start > "/dev/stderr"
        exit 1
    }
}
