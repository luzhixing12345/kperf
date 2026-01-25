#!/usr/bin/env bash
# Bash completion script for kperf

_kperf() {
    local cur prev words cword
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    words=("${COMP_WORDS[@]}")
    cword=$COMP_CWORD

    # Check if we're after --
    local double_dash_pos=0
    for ((i=1; i<=cword; i++)); do
        if [[ "${words[i]}" == "--" ]]; then
            double_dash_pos=$i
            break
        fi
    done

    # After --, complete executable files
    if [[ $double_dash_pos -gt 0 ]]; then
        local completions=($(compgen -f -- "$cur"))
        local filtered=()
        local has_dir=0

        for f in "${completions[@]}"; do
            if [[ -d "$f" ]]; then
                # Directory: add trailing slash
                filtered+=("$f")
                has_dir=1
            elif [[ -x "$f" ]]; then
                # Executable file
                filtered+=("$f")
            fi
        done

        # Use filenames option to display only basename while keeping full path
        compopt -o filenames 2>/dev/null || true
        # If there are directories, don't add trailing space after directory name
        [[ $has_dir -eq 1 ]] && compopt -o nospace 2>/dev/null || true

        COMPREPLY=("${filtered[@]}")
        return
    fi

    # Before --, complete options
    case "$prev" in
        -p|--pid)
            # Complete PIDs
            COMPREPLY=($(compgen -W "$(ls /proc | grep -E '^[0-9]+$')" -- "$cur"))
            return
            ;;
        -F|--freq|-t|--timeout|--port)
            # Numeric values, no completion
            return
            ;;
        -h|--help|-v|--version|-d|--debug|-k|--kernel|-s|--server|--tui)
            # Boolean flags, no completion
            return
            ;;
        *)
            COMPREPLY=($(compgen -W "-p --pid -k --kernel -- -F --freq -t --timeout -s --server --port --tui -h --help -v --version -d --debug" -- "$cur"))
            ;;
    esac
}

complete -F _kperf kperf
