# Replace the lines between "# BEGIN TABLES" and "# END TABLES" in
# rubik_rv32i.s with the file given by -v tables=..., normally the output of
# verify_gates.exe --emit. The marker lines are kept; output uses LF.
# Compare only:
#   ./verify_gates.exe --emit > build/tables.s &&
#   awk -v tables=build/tables.s -f tools/sync_tables.awk rubik_rv32i.s > build/synced.s &&
#   tr -d '\r' < rubik_rv32i.s | cmp - build/synced.s && echo "tables in sync"
# Replace the tables in rubik_rv32i.s (rename only on success):
#   ./verify_gates.exe --emit > build/tables.s &&
#   awk -v tables=build/tables.s -f tools/sync_tables.awk rubik_rv32i.s > build/synced.s.tmp &&
#   mv build/synced.s.tmp rubik_rv32i.s

function fail(msg) {
    printf "sync_tables.awk:%d: %s\n", NR, msg > "/dev/stderr"
    failed = 1
    exit 1
}

{ sub(/\r$/, "") }

$0 == "# BEGIN TABLES" {
    if (begins++)
        fail("more than one # BEGIN TABLES")
    print
    while ((r = (getline line < tables)) > 0) {
        sub(/\r$/, "", line)
        print line
        ++copied
    }
    close(tables)
    if (r < 0 || !copied)
        fail("cannot read tables file: " tables)
    inside = 1
    next
}

$0 == "# END TABLES" {
    if (!inside)
        fail("# END TABLES without # BEGIN TABLES")
    inside = 0
    ++ends
}

!inside { print }

END {
    if (failed)
        exit 1
    if (begins != 1 || ends != 1) {
        printf "sync_tables.awk: need one # BEGIN TABLES / # END TABLES pair, found %d/%d\n", begins, ends > "/dev/stderr"
        exit 1
    }
}
