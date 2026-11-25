#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER_BIN="${SERVER_BIN:-$ROOT_DIR/bin/server}"
PASS_COUNT=0
FAIL_COUNT=0

declare -A CLIENT_IN_FD
declare -A CLIENT_PID
declare -A CLIENT_READ_LINE

start_server() {
    TEST_TMP="$(mktemp -d)"
    mkdir -p "$TEST_TMP/data" "$TEST_TMP/logs"
    PORT=$((20000 + RANDOM % 10000))
    (
        cd "$TEST_TMP" || exit 1
        exec "$SERVER_BIN" "$PORT" >"$TEST_TMP/server.out" 2>&1
    ) &
    SERVER_PID=$!

    local i
    for i in $(seq 1 40); do
        if nc -z 127.0.0.1 "$PORT" >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.05
    done
    echo "FATAL: server failed to start on port $PORT" >&2
    cat "$TEST_TMP/server.out" >&2 || true
    return 1
}

restart_server() {
    if [[ -n "${SERVER_PID:-}" ]]; then
        kill "$SERVER_PID" >/dev/null 2>&1 || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    (
        cd "$TEST_TMP" || exit 1
        exec "$SERVER_BIN" "$PORT" >>"$TEST_TMP/server.out" 2>&1
    ) &
    SERVER_PID=$!
    local i
    for i in $(seq 1 40); do
        if nc -z 127.0.0.1 "$PORT" >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.05
    done
    echo "FATAL: server failed to restart on port $PORT" >&2
    return 1
}

stop_server() {
    local name
    for name in "${!CLIENT_PID[@]}"; do
        client_close "$name"
    done
    if [[ -n "${SERVER_PID:-}" ]]; then
        kill "$SERVER_PID" >/dev/null 2>&1 || true
        wait "$SERVER_PID" 2>/dev/null || true
        unset SERVER_PID
    fi
    if [[ -n "${TEST_TMP:-}" && -d "$TEST_TMP" ]]; then
        rm -rf "$TEST_TMP"
    fi
}

client_open() {
    local name="$1"
    local fifo="$TEST_TMP/${name}.in"
    local out="$TEST_TMP/${name}.out"
    rm -f "$fifo" "$out"
    mkfifo "$fifo"
    : >"$out"
    nc 127.0.0.1 "$PORT" <"$fifo" >"$out" 2>/dev/null &
    CLIENT_PID["$name"]=$!
    local fd
    exec {fd}>"$fifo"
    CLIENT_IN_FD["$name"]=$fd
    CLIENT_READ_LINE["$name"]=0
    sleep 0.05
}

client_close() {
    local name="$1"
    if [[ -n "${CLIENT_IN_FD[$name]:-}" ]]; then
        local fd="${CLIENT_IN_FD[$name]}"
        eval "exec ${fd}>&-"
        unset 'CLIENT_IN_FD[$name]'
    fi
    if [[ -n "${CLIENT_PID[$name]:-}" ]]; then
        kill "${CLIENT_PID[$name]}" >/dev/null 2>&1 || true
        wait "${CLIENT_PID[$name]}" 2>/dev/null || true
        unset 'CLIENT_PID[$name]'
    fi
}

client_send() {
    local name="$1"
    local line="$2"
    local fd="${CLIENT_IN_FD[$name]}"
    printf '%s\r\n' "$line" >&"$fd"
}

client_send_raw() {
    local name="$1"
    local raw="$2"
    local fd="${CLIENT_IN_FD[$name]}"
    printf '%b' "$raw" >&"$fd"
}

expect_line() {
    local name="$1"
    local pattern="$2"
    local timeout="${3:-3}"
    local out="$TEST_TMP/${name}.out"
    local target=$(( ${CLIENT_READ_LINE[$name]:-0} + 1 ))
    local ticks=$(( timeout * 20 ))
    local i line total

    for i in $(seq 1 "$ticks"); do
        total=$(wc -l <"$out" 2>/dev/null || echo 0)
        if (( total >= target )); then
            line=$(sed -n "${target}p" "$out" | tr -d '\r')
            CLIENT_READ_LINE["$name"]=$target
            if [[ "$line" =~ $pattern ]]; then
                return 0
            else
                echo "  [expect_line $name] line #$target '$line' !~ /$pattern/" >&2
                return 1
            fi
        fi
        sleep 0.05
    done
    echo "  [expect_line $name] timeout (${timeout}s) waiting for line #$target /$pattern/" >&2
    return 1
}

expect_no_line() {
    local name="$1"
    local sec="${2:-1}"
    local out="$TEST_TMP/${name}.out"
    local before="${CLIENT_READ_LINE[$name]:-0}"
    sleep "$sec"
    local total
    total=$(wc -l <"$out" 2>/dev/null || echo 0)
    if (( total > before )); then
        local line
        line=$(sed -n "$((before + 1))p" "$out" | tr -d '\r')
        echo "  [expect_no_line $name] unexpected line: '$line'" >&2
        return 1
    fi
    return 0
}

login_as() {
    local name="$1"
    local user="$2"
    local pass="$3"
    client_send "$name" "REGISTER $user $pass"
    expect_line "$name" "^(201|409) " || return 1
    client_send "$name" "LOGIN $user $pass"
    expect_line "$name" "^200 Login successful$" || return 1
}

pass_at() {
    local id="$1"
    echo "PASS $id"
    PASS_COUNT=$((PASS_COUNT + 1))
}

fail_at() {
    local id="$1"
    local reason="$2"
    echo "FAIL $id: $reason" >&2
    FAIL_COUNT=$((FAIL_COUNT + 1))
}

finish_suite() {
    stop_server
    echo "Summary: PASS=$PASS_COUNT FAIL=$FAIL_COUNT"
    (( FAIL_COUNT == 0 ))
}
