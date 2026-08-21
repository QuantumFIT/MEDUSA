# Sourced by bash test scripts — colorful pass/fail summary table.
# Usage:
#   source "$(dirname "$0")/test_summary.sh"
#   summary_init
#   summary_record "label" 0|1   # 0=pass, 1=fail
#   summary_print "suite_name"; exit $?

summary_init() {
    SUMMARY_LABELS=()
    SUMMARY_STATUS=()  # 0 pass, 1 fail
    if [[ -t 1 ]]; then
        SUMMARY_USE_COLOR=1
    else
        SUMMARY_USE_COLOR=0
    fi
}

_summary_color() {
    local name="$1"
    if [[ "${SUMMARY_USE_COLOR}" -ne 1 ]]; then
        return
    fi
    case "${name}" in
        bold)  printf '\033[1m' ;;
        green) printf '\033[32m' ;;
        red)   printf '\033[31m' ;;
        cyan)  printf '\033[36m' ;;
        dim)   printf '\033[2m' ;;
        reset) printf '\033[0m' ;;
    esac
}

summary_record() {
    local label="$1"
    local fail="$2"
    SUMMARY_LABELS+=("${label}")
    if [[ "${fail}" -eq 0 ]]; then
        SUMMARY_STATUS+=(0)
    else
        SUMMARY_STATUS+=(1)
    fi
}

_summary_hline() {
    local name_w="$1" status_w="$2" i
    printf '+'
    for ((i = 0; i < name_w + 2; i++)); do printf '-'; done
    printf '+'
    for ((i = 0; i < status_w + 2; i++)); do printf '-'; done
    printf '+\n'
}

summary_print() {
    local suite="$1"
    local n="${#SUMMARY_LABELS[@]}"
    local name_w=24
    local status_w=6
    local i label len passed=0 failed=0

    for ((i = 0; i < n; i++)); do
        len=${#SUMMARY_LABELS[i]}
        if ((len > name_w)); then name_w=$len; fi
        if [[ "${SUMMARY_STATUS[i]}" -eq 0 ]]; then
            passed=$((passed + 1))
        else
            failed=$((failed + 1))
        fi
    done
    if ((${#suite} + 8 > name_w)); then name_w=$((${#suite} + 8)); fi
    if ((name_w > 72)); then name_w=72; fi

    echo
    printf '%s%s══ Test summary: %s ══%s\n' \
        "$(_summary_color bold)" "$(_summary_color cyan)" "${suite}" "$(_summary_color reset)"
    _summary_hline "${name_w}" "${status_w}"
    printf '| %s%-*s%s | %s%-*s%s |\n' \
        "$(_summary_color bold)" "${name_w}" "Test" "$(_summary_color reset)" \
        "$(_summary_color bold)" "${status_w}" "Status" "$(_summary_color reset)"
    _summary_hline "${name_w}" "${status_w}"

    for ((i = 0; i < n; i++)); do
        label="${SUMMARY_LABELS[i]}"
        if ((${#label} > name_w)); then
            label="${label:0:$((name_w - 3))}..."
        fi
        if [[ "${SUMMARY_STATUS[i]}" -eq 0 ]]; then
            printf '| %-*s | %s%-*s%s |\n' \
                "${name_w}" "${label}" \
                "$(_summary_color green)" "${status_w}" "PASS" "$(_summary_color reset)"
        else
            printf '| %-*s | %s%-*s%s |\n' \
                "${name_w}" "${label}" \
                "$(_summary_color red)" "${status_w}" "FAIL" "$(_summary_color reset)"
        fi
    done

    _summary_hline "${name_w}" "${status_w}"

    if [[ "${failed}" -eq 0 ]]; then
        printf '%s%sOK%s %s: %d tests  %s(%d/%d passed)%s\n' \
            "$(_summary_color bold)" "$(_summary_color green)" "$(_summary_color reset)" \
            "${suite}" "${n}" \
            "$(_summary_color dim)" "${passed}" "${n}" "$(_summary_color reset)"
        return 0
    fi
    printf '%s%sFAILED%s %s: %d/%d failed\n' \
        "$(_summary_color bold)" "$(_summary_color red)" "$(_summary_color reset)" \
        "${suite}" "${failed}" "${n}"
    return 1
}
