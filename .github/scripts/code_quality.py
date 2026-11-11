#!/usr/bin/env python3
"""Collect code-quality findings, and compare two sets of them.

The Code quality workflow runs each collector twice, on the pull request and
on its base, and fails only on findings the pull request adds. Existing ones
are reported but do not block anything, so the check can be required from the
start and the backlog paid off separately.

A finding is identified by file, kind and message, *not* by line: unrelated
edits move every line below them, and a line-sensitive diff would report all
of those as new. The cost is that a second, identical finding in the same file
(the same unused variable name, say) is not seen as new.

  code_quality.py warnings --compiler gcc|clang --out F   compiler warnings
  code_quality.py cppcheck --out F                         static analysis
  code_quality.py shellcheck --out F                       shell scripts
  code_quality.py compare --name N BASE HEAD               new findings -> exit 1

Each collector runs in the current directory, which must be a MEDUSA checkout
with MoToBuddy initialised (make init).
"""
import argparse
import os
import re
import shlex
import subprocess  # nosec B404
import sys
import tempfile

# -Wall -Wextra on top of the release optimisation level: several warnings
# (maybe-uninitialized, for one) only appear with the optimiser on. LTO is off
# so that every warning is reported at compile time, once per file.
WARN_CFLAGS = "-O2 -g -Wall -Wextra"

# What each compiler builds. Every leaf type compiles the shared sources with
# different macros, so all of them are built. clang has no __float128 support
# on devel yet (no libquadmath), and the C++ gates need GCC's
# -fext-numeric-literals, so clang builds the rest.
BUILDS = {
    "gcc": [
        ["buddy_doubles_all", "buddy_gmp"],
        ["buddy_doubles_f128", "USE_CXX=1"],
    ],
    "clang": [
        ["buddy_doubles_f32", "buddy_doubles_f64", "buddy_doubles_f80", "buddy_gmp"],
    ],
}

# Builds whose compile commands cppcheck replays, one per leaf backend: the
# same sources are compiled with different force-included (-include) headers.
CPPCHECK_VARIANTS = ["buddy_doubles_f128", "buddy_gmp"]

GCC_WARNING = re.compile(r"^(?P<file>[^:\s][^:]*):\d+:\d+: warning: (?P<msg>.*?)(?: \[(?P<flag>-W[^\]]+)\])?$")
CPPCHECK_LINE = re.compile(r"^(?P<file>[^:]+):\d+:(?P<sev>[a-z]+):(?P<id>[A-Za-z0-9_]+):(?P<msg>.*)$")
SHELLCHECK_LINE = re.compile(r"^(?P<file>[^:]+):\d+:\d+: (?P<sev>[a-z]+): (?P<msg>.*) \[(?P<id>SC\d+)\]$")


def norm_path(path):
    """Repository-relative, with ../ folded, so base and head agree."""
    return os.path.relpath(os.path.normpath(os.path.join(os.getcwd(), path)))


def norm_msg(msg):
    # Messages that cite another line ("... at line 347") would differ on
    # every unrelated edit above that line.
    return re.sub(r"\bline \d+\b", "line N", msg).strip()


def write(findings, out):
    with open(out, "w", encoding="utf-8") as f:
        f.writelines("\t".join(item) + "\n" for item in sorted(set(findings)))
    print(f"{len(set(findings))} distinct finding(s) -> {out}")


def run(cmd, check=False, **kw):
    """Run cmd (a list, no shell) and echo it, as `set -x` would.

    The only place this script starts a process. The commands are fixed tool
    names (make, cppcheck, shellcheck, git) with arguments taken from the
    repository itself (`make -n`, `git ls-files`), run in CI on the
    repository's own checkout. Most callers read the exit status themselves,
    hence check=False by default.
    """
    print("+ " + " ".join(shlex.quote(c) for c in cmd), flush=True)
    return subprocess.run(cmd, check=check, **kw)  # nosec B603 # nosemgrep


def collect_warnings(compiler, out):
    cc, cxx = ("gcc", "g++") if compiler == "gcc" else ("clang", "clang++")
    findings = []
    run(["make", "clean-artifacts"], stdout=subprocess.DEVNULL)
    for targets in BUILDS[compiler]:
        # --output-sync keeps each compiler's output in one piece: with -j,
        # interleaved diagnostics turn into lines no pattern can parse.
        cmd = ["make", "-k", f"-j{os.cpu_count() or 2}", "--output-sync=target", "LTO=0",
               f"CC={cc}", f"CXX={cxx}", f"CFLAGS={WARN_CFLAGS}", *targets]
        res = run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for line in res.stdout.splitlines():
            m = GCC_WARNING.match(line)
            if m:
                findings.append((norm_path(m["file"]), m["flag"] or "-W", norm_msg(m["msg"])))
        if res.returncode != 0:
            # A build that stops early would just report fewer warnings and so
            # look like an improvement. The errors come first: with make -k
            # they can be anywhere in the output, not just at its end.
            lines = res.stdout.splitlines()
            errors = [i for i, line in enumerate(lines)
                      if re.search(r"\berror\b|undefined reference|\*\*\*", line)]
            for i in errors[:40]:
                print("\n".join(lines[max(0, i - 2):i + 3]))
                print("--")
            print(res.stdout[-4000:])
            sys.exit(f"build failed: {' '.join(targets)}")
    write(findings, out)


def compile_commands(target):
    """(source, -I/-D flags, -include headers) for every C file `make target` compiles."""
    res = run(["make", "-Bn", target], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    # make can print some commands and then fail; analysing that partial list
    # would let the check pass on code it never saw.
    if res.returncode != 0:
        sys.exit(f"make -Bn {target} failed ({res.returncode}):\n{res.stderr}")
    units = []
    for line in res.stdout.replace("\\\n", " ").splitlines():
        args = shlex.split(line) if " -c " in line else []
        if not args or (args[0] not in ("gcc", "cc", "clang") and not args[0].endswith("gcc")):
            continue
        src = args[args.index("-c") + 1]
        if not src.endswith(".c"):
            continue
        flags, includes = [], []
        it = iter(args[1:])
        for a in it:
            if a in ("-I", "-D"):
                flags.append(a + next(it))
            elif a.startswith(("-I", "-D")):
                flags.append(a)
            elif a == "-include":
                includes.append(next(it))
        units.append((src, flags, includes))
    return units


def collect_cppcheck(out):
    findings = []
    with tempfile.TemporaryDirectory() as tmp:
        for target in CPPCHECK_VARIANTS:
            units = compile_commands(target)
            if not units:
                sys.exit(f"no compile commands found for {target}")
            for src, flags, includes in units:
                # cppcheck honours only one --include, so the force-included
                # headers go through one prelude.
                prelude = os.path.join(tmp, "prelude.h")
                with open(prelude, "w", encoding="utf-8") as f:
                    f.writelines(f'#include "{os.path.abspath(inc)}"\n' for inc in includes)
                cmd = ["cppcheck", "-q", "--std=c11", "--enable=warning,portability,performance",
                       "--inline-suppr", "--suppress=missingIncludeSystem",
                       # cppcheck does not know the compiler's predefined macros.
                       "-D__SIZEOF_FLOAT128__=16", "-D__GNUC__=13",
                       f"--include={prelude}",
                       "--template={file}:{line}:{severity}:{id}:{message}", *flags, src]
                res = run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
                # Without --error-exitcode, cppcheck exits 0 whatever it finds
                # (an analysis stopped by a syntax error included), so anything
                # else means it did not run to the end - and a partial report
                # would make the comparison pass on findings it never saw.
                if res.returncode != 0:
                    sys.exit(f"cppcheck failed on {src} ({res.returncode}):\n{res.stderr}")
                for line in res.stderr.splitlines():
                    m = CPPCHECK_LINE.match(line)
                    if m:
                        findings.append((norm_path(m["file"]), f"{m['sev']}:{m['id']}", norm_msg(m["msg"])))
    write(findings, out)


def collect_shellcheck(out):
    # NUL-separated, so a path with a space stays one argument.
    out_z = run(["git", "ls-files", "-z", "*.sh"], check=True, stdout=subprocess.PIPE,
                text=True).stdout
    files = [f for f in out_z.split("\0") if f]
    res = run(["shellcheck", "-S", "warning", "-f", "gcc", *files],
              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    # 1 means findings; anything else (2: a file could not be read, ...) means
    # the scan itself failed, and its report would be incomplete.
    if res.returncode not in (0, 1):
        sys.exit(f"shellcheck failed ({res.returncode}):\n{res.stdout}")
    findings = []
    for line in res.stdout.splitlines():
        m = SHELLCHECK_LINE.match(line)
        if m:
            findings.append((norm_path(m["file"]), m["id"], norm_msg(m["msg"])))
    write(findings, out)


def read(path):
    with open(path, encoding="utf-8") as f:
        return {tuple(line.rstrip("\n").split("\t")) for line in f if line.strip()}


def compare(name, base_path, head_path):
    base, head = read(base_path), read(head_path)
    new, fixed = sorted(head - base), sorted(base - head)
    lines = [f"### {name}", "",
             "| | findings |", "|---|---|",
             f"| base | {len(base)} |", f"| this change | {len(head)} |",
             f"| **new** | **{len(new)}** |", f"| fixed | {len(fixed)} |", ""]
    if new:
        lines += ["New findings (they fail the check):", "", "```"]
        lines += ["  ".join(item) for item in new]
        lines += ["```", ""]
    if fixed:
        lines += ["<details><summary>Fixed by this change</summary>", "", "```"]
        lines += ["  ".join(item) for item in fixed]
        lines += ["```", "</details>", ""]
    if head:
        lines += ["<details><summary>All findings in this change</summary>", "", "```"]
        lines += ["  ".join(item) for item in sorted(head)]
        lines += ["```", "</details>", ""]
    text = "\n".join(lines)
    print(text)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write(text + "\n")
    for file, kind, msg in new:
        # GitHub annotation on the file (no line: findings are line-free).
        print(f"::error file={file},title={name}: new {kind}::{msg}")
    return 1 if new else 0


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    w = sub.add_parser("warnings")
    w.add_argument("--compiler", choices=sorted(BUILDS), required=True)
    w.add_argument("--out", required=True)
    for name in ("cppcheck", "shellcheck"):
        sub.add_parser(name).add_argument("--out", required=True)
    c = sub.add_parser("compare")
    c.add_argument("--name", required=True)
    c.add_argument("base")
    c.add_argument("head")
    a = p.parse_args()
    out = os.path.abspath(a.out) if hasattr(a, "out") else None
    if a.cmd == "warnings":
        collect_warnings(a.compiler, out)
    elif a.cmd == "cppcheck":
        collect_cppcheck(out)
    elif a.cmd == "shellcheck":
        collect_shellcheck(out)
    else:
        sys.exit(compare(a.name, a.base, a.head))


if __name__ == "__main__":
    main()
